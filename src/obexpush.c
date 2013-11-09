/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */

/*
 *  Copyright (C) 2004-2008 Red Hat, Inc.
 *  Copyright (C) 2013 Intel Corporation.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Bastien Nocera <hadess@hadess.net>
 *  Authors: Emilio Pozuelo Monfort <emilio.pozuelo@collabora.co.uk>
 *
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <bluetooth-client.h>
#include <libnotify/notify.h>
#include <canberra-gtk.h>

#include "user_share.h"
#include "user_share-private.h"

#include "obexpush.h"

#define MANAGER_SERVICE	"org.bluez.obex"
#define MANAGER_IFACE	"org.bluez.obex.AgentManager1"
#define MANAGER_PATH	"/org/bluez/obex"

#define AGENT_PATH	"/org/gnome/share/agent"
#define AGENT_IFACE	"org.bluez.obex.Agent1"

#define TRANSFER_IFACE	"org.bluez.obex.Transfer1"
#define SESSION_IFACE	"org.bluez.obex.Session1"

static GDBusNodeInfo *introspection_data = NULL;

static const gchar introspection_xml[] =
"<node name='"AGENT_PATH"'>"
"  <interface name='"AGENT_IFACE"'>"
"    <method name='Release'>"
"    </method>"
"    <method name='Cancel'>"
"    </method>"
"    <method name='AuthorizePush'>"
"      <arg name='transfer' type='o' />"
"      <arg name='path' type='s' direction='out' />"
"    </method>"
"  </interface>"
"</node>";

G_DEFINE_TYPE(ObexAgent, obex_agent, G_TYPE_OBJECT)

static ObexAgent *agent;
static GDBusProxy *manager;
static BluetoothClient *client;

static AcceptSetting accept_setting = -1;
static gboolean show_notifications = FALSE;

static GtkStatusIcon *statusicon = NULL;
static guint num_notifications = 0;

static void
hide_statusicon (void)
{
	num_notifications--;
	if (num_notifications == 0)
		gtk_status_icon_set_visible (statusicon, FALSE);
}

static void
on_close_notification (NotifyNotification *notification)
{
	hide_statusicon ();
	g_object_unref (notification);
}

static void
notification_launch_action_on_file_cb (NotifyNotification *notification,
				       const char *action,
				       const char *file_uri)
{
	GdkDisplay *display;
	GAppLaunchContext *ctx;
	GTimeVal val;

	g_assert (action != NULL);

	g_get_current_time (&val);

	display = gdk_display_get_default ();
	ctx = G_APP_LAUNCH_CONTEXT (gdk_display_get_app_launch_context (display));
	gdk_app_launch_context_set_timestamp (GDK_APP_LAUNCH_CONTEXT (ctx), val.tv_sec);

	/* We launch the file viewer for the file */
	if (g_str_equal (action, "display") != FALSE) {
		if (g_app_info_launch_default_for_uri (file_uri, ctx, NULL) == FALSE) {
			g_warning ("Failed to launch the file viewer\n");
		}
	}

	/* we open the Downloads folder */
	if (g_str_equal (action, "reveal") != FALSE) {
		GFile *file;
		GFile *parent;
		gchar *parent_uri;

		file = g_file_new_for_uri (file_uri);
		parent = g_file_get_parent (file);
		parent_uri = g_file_get_uri (parent);
		g_object_unref (file);
		g_object_unref (parent);

		if (!g_app_info_launch_default_for_uri (parent_uri, ctx, NULL)) {
			g_warning ("Failed to launch the file manager\n");
		}

		g_free (parent_uri);
	}

	notify_notification_close (notification, NULL);
	/* No need to call hide_statusicon(), closing the notification
	 * will call the close callback */
}

static void
show_notification (const char *filename)
{
	char *file_uri, *notification_text, *display, *mime_type;
	NotifyNotification *notification;
	ca_context *ctx;
	GAppInfo *app;

	file_uri = g_filename_to_uri (filename, NULL, NULL);
	if (file_uri == NULL) {
		g_warning ("Could not make a filename from '%s'", filename);
		return;
	}

	display = g_filename_display_basename (filename);
	/* Translators: %s is the name of the filename received */
	notification_text = g_strdup_printf(_("You received \"%s\" via Bluetooth"), display);
	g_free (display);
	notification = notify_notification_new (_("You received a file"),
								 notification_text,
								 "dialog-information");

	notify_notification_set_timeout (notification, NOTIFY_EXPIRES_DEFAULT);
	notify_notification_set_hint_string (notification, "desktop-entry", "gnome-user-share-properties");

	mime_type = g_content_type_guess (filename, NULL, 0, NULL);
	app = g_app_info_get_default_for_type (mime_type, FALSE);
	if (app != NULL) {
		g_object_unref (app);
		notify_notification_add_action (notification, "display", _("Open File"),
						(NotifyActionCallback) notification_launch_action_on_file_cb,
						g_strdup (file_uri), (GFreeFunc) g_free);
	}
	notify_notification_add_action (notification, "reveal", _("Reveal File"),
					(NotifyActionCallback) notification_launch_action_on_file_cb,
					g_strdup (file_uri), (GFreeFunc) g_free);

	g_free (file_uri);
	
	g_signal_connect (G_OBJECT (notification), "closed", G_CALLBACK (on_close_notification), notification);

	if (!notify_notification_show (notification, NULL)) {
		g_warning ("failed to send notification\n");
	}
	g_free (notification_text);

	/* Now we do the audio notification */
	ctx = ca_gtk_context_get ();
	ca_context_play (ctx, 0,
			 CA_PROP_EVENT_ID, "complete-download",
			 CA_PROP_EVENT_DESCRIPTION, _("File reception complete"),
			 NULL);
}

static void
show_icon (void)
{
	if (statusicon == NULL) {
		statusicon = gtk_status_icon_new_from_icon_name ("gnome-obex-server");
	} else {
		gtk_status_icon_set_visible (statusicon, TRUE);
	}
	num_notifications++;
}

static void
ask_user_transfer_accepted (NotifyNotification *notification,
			    char *action,
			    GDBusMethodInvocation *invocation)
{
	gchar *file = g_object_get_data (G_OBJECT (invocation), "filename");

	g_debug ("Notification: transfer accepted! accepting transfer");

	g_dbus_method_invocation_return_value (invocation,
		g_variant_new ("(s)", file));

	show_icon ();
}

static void
ask_user_transfer_rejected (NotifyNotification *notification,
			    char *action,
			    GDBusMethodInvocation *invocation)
{
	g_debug ("Notification: transfer rejected! rejecting transfer");

	g_dbus_method_invocation_return_dbus_error (invocation,
		"org.bluez.obex.Error.Rejected", "Not Authorized");
}

static void
ask_user_on_close (NotifyNotification *notification,
		   GDBusMethodInvocation *invocation)
{
	g_debug ("Notification closed! rejecting transfer");

	g_dbus_method_invocation_return_dbus_error (invocation,
		"org.bluez.obex.Error.Rejected", "Not Authorized");
}

static void
ask_user (GDBusMethodInvocation *invocation,
	  const gchar *filename)
{
	NotifyNotification *notification;
	gchar *notification_text;

	/* Translators: %s is the name of the filename being received */
	notification_text = g_strdup_printf(_("You have been sent a file \"%s\" via Bluetooth"), filename);
	notification = notify_notification_new (_("You have been sent a file"),
						notification_text,
						"bluetooth");

	notify_notification_set_timeout (notification, NOTIFY_EXPIRES_NEVER);
	notify_notification_set_hint_string (notification, "desktop-entry",
					     "gnome-user-share-properties");

	notify_notification_add_action (notification, "receive", _("Receive"),
					(NotifyActionCallback) ask_user_transfer_accepted,
					invocation, NULL);
	notify_notification_add_action (notification, "cancel", _("Cancel"),
					(NotifyActionCallback) ask_user_transfer_rejected,
					invocation, NULL);

	/* We want to reject the transfer if the user closes the notification
	 * without accepting or rejecting it, so we connect to it. However
	 * if the user clicks on one of the actions, the callback for the
	 * action will be invoked, and then the notification will be closed
	 * and the callback for :closed will be called. So we disconnect
	 * from :closed if the invocation object goes away (which will happen
	 * after the handler of either action accepts or rejects the transfer).
	 */
	g_signal_connect_object (G_OBJECT (notification), "closed",
		G_CALLBACK (ask_user_on_close), invocation, 0);

	if (!notify_notification_show (notification, NULL))
		g_warning ("failed to send notification\n");

	g_free (notification_text);
}

gboolean
get_paired_for_address (const char *adapter, const char *device)
{
	GtkTreeModel *model;
	GtkTreeIter parent;
	gboolean next;
	gboolean ret = FALSE;
	char *addr;

	model = bluetooth_client_get_model (client);

	for (next = gtk_tree_model_get_iter_first (model, &parent);
	     next;
	     next = gtk_tree_model_iter_next (model, &parent)) {
		gtk_tree_model_get (model, &parent,
			BLUETOOTH_COLUMN_ADDRESS, &addr,
			-1);

		if (g_strcmp0 (addr, adapter) == 0) {
			GtkTreeIter child;
			char *dev_addr;

			for (next = gtk_tree_model_iter_children (model, &child, &parent);
			     next;
			     next = gtk_tree_model_iter_next (model, &child)) {
				gboolean paired;

				gtk_tree_model_get (model, &child,
					BLUETOOTH_COLUMN_ADDRESS, &dev_addr,
					BLUETOOTH_COLUMN_PAIRED, &paired,
					-1);
				if (g_strcmp0 (dev_addr, device) == 0) {
					ret = paired;
					next = FALSE;
				}
				g_free (dev_addr);
			}
		}
		g_free (addr);
	}

	g_object_unref (model);
	return ret;
}

static void
on_session_acquired (GObject *object,
		     GAsyncResult *res,
		     gpointer user_data)
{
	GDBusMethodInvocation *invocation = user_data;
	GDBusProxy *session;
	GError *error = NULL;
	GVariant *v;
	char *device, *adapter;
	gboolean paired;

	session = g_dbus_proxy_new_for_bus_finish (res, &error);

	if (!session) {
		g_debug ("Failed to create a proxy for the session: %s", error->message);
		g_clear_error (&error);
		goto out;
	}

	/* obexd puts the remote device in Destination and our adapter
	 * in Source */
	v = g_dbus_proxy_get_cached_property (session, "Destination");
	if (v) {
		device = g_variant_dup_string (v, NULL);
		g_variant_unref (v);
	}

	v = g_dbus_proxy_get_cached_property (session, "Source");
	if (v) {
		adapter = g_variant_dup_string (v, NULL);
		g_variant_unref (v);
	}

	g_object_unref (session);

	if (!device || !adapter) {
		g_debug ("Could not get remote device for the transfer");
		g_free (device);
		g_free (adapter);
		goto out;
	}

	paired = get_paired_for_address (adapter, device);
	g_free (device);
	g_free (adapter);

	if (paired) {
		g_debug ("Remote device is paired, auto-accepting the transfer");
		g_dbus_method_invocation_return_value (invocation,
			g_variant_new ("(s)", g_object_get_data (G_OBJECT (invocation), "filename")));
		return;
	} else {
		g_debug ("Remote device is not paired, auto-rejecting the transfer");
	}

out:
	g_debug ("Rejecting transfer");
	g_dbus_method_invocation_return_dbus_error (invocation,
		"org.bluez.obex.Error.Rejected", "Not Authorized");
}

static void
check_if_bonded (GDBusProxy *transfer,
		 GDBusMethodInvocation *invocation,
		 const gchar *filename)
{
	GVariant *v;
	const gchar *session = NULL;

	v = g_dbus_proxy_get_cached_property (transfer, "Session");

	if (v) {
		session = g_variant_get_string (v, NULL);
		g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
					  G_DBUS_PROXY_FLAGS_NONE,
					  NULL,
					  MANAGER_SERVICE,
					  session,
					  SESSION_IFACE,
					  NULL,
					  on_session_acquired,
					  invocation);
		g_variant_unref (v);
	} else {
		g_debug ("Could not get session path for the transfer, "
			 "rejecting the transfer");
		g_dbus_method_invocation_return_dbus_error (invocation,
			"org.bluez.obex.Error.Rejected", "Not Authorized");
	}
}

static void
obex_agent_release (GError **error)
{
}

static void
obex_agent_cancel (GError **error)
{
}

static void
transfer_property_changed (GDBusProxy *transfer,
			   GVariant   *changed_properties,
			   GStrv       invalidated_properties,
			   gpointer    user_data)
{
	GVariantIter iter;
	const gchar *key;
	GVariant *value;
	const char *filename;

	g_debug ("Calling transfer_property_changed()");

	filename = g_object_get_data (G_OBJECT (transfer), "filename");

	g_variant_iter_init (&iter, changed_properties);
	while (g_variant_iter_next (&iter, "{&sv}", &key, &value)) {
		char *str = g_variant_print (value, TRUE);

		if (g_str_equal (key, "Status")) {
			const gchar *status;

			status = g_variant_get_string (value, NULL);

			g_debug ("Got status %s = %s for filename %s", status, str, filename);

			if (g_str_equal (status, "complete")) {
				if (show_notifications) {
					g_debug ("transfer completed, showing a notification");
					show_notification (filename);
				} else {
					hide_statusicon ();
				}
			}
		} else {
			g_debug ("Unhandled property changed %s = %s for filename %s", key, str, filename);
		}
		g_free (str);
		g_variant_unref (value);
	}
}

static void
obex_agent_authorize_push (GObject *source_object,
			   GAsyncResult *res,
			   gpointer user_data)
{
	GDBusProxy *transfer = g_dbus_proxy_new_for_bus_finish (res, NULL);
	GDBusMethodInvocation *invocation = user_data;
	GVariant *variant = g_dbus_proxy_get_cached_property (transfer, "Name");
	const gchar *filename = g_variant_get_string (variant, NULL);
	gchar *download_dir, *file;
	gboolean authorize = FALSE;

	g_debug ("AuthorizePush received");

	download_dir = lookup_download_dir ();
	file = g_build_filename (download_dir, filename, NULL);
	g_free (download_dir);

	g_object_set_data_full (G_OBJECT (transfer), "filename", g_strdup (file), g_free);
	g_object_set_data_full (G_OBJECT (invocation), "filename", g_strdup (file), g_free);

	g_signal_connect (transfer, "g-properties-changed",
		G_CALLBACK (transfer_property_changed), NULL);

	switch (accept_setting) {
	case ACCEPT_ALWAYS:
		authorize = TRUE;
		break;
	case ACCEPT_BONDED:
		check_if_bonded (transfer, invocation, filename);
		g_variant_unref (variant);
		/* check_if_bonded() will accept or reject the transfer */
		return;
	case ACCEPT_ASK:
		ask_user (invocation, filename);
		g_variant_unref (variant);
		/* ask_user() will accept or reject the transfer */
		return;
	default:
		g_warn_if_reached ();
	}

	if (authorize) {
		g_dbus_method_invocation_return_value (invocation,
			g_variant_new ("(s)", file));

		show_icon ();

		g_debug ("Incoming transfer authorized: %s", file);
		g_free (file);
	} else {
		g_dbus_method_invocation_return_dbus_error (invocation,
			"org.bluez.obex.Error.Rejected", "Not Authorized");
		g_debug ("Incoming transfer rejected: %s", filename);
	}

	g_variant_unref (variant);
}

static void
handle_method_call (GDBusConnection       *connection,
		    const gchar           *sender,
		    const gchar           *object_path,
		    const gchar           *interface_name,
		    const gchar           *method_name,
		    GVariant              *parameters,
		    GDBusMethodInvocation *invocation,
		    gpointer               user_data)
{
	if (g_str_equal (method_name, "Cancel")) {
		obex_agent_cancel (NULL);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_str_equal (method_name, "Release")) {
		obex_agent_release (NULL);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_str_equal (method_name, "AuthorizePush")) {
		const gchar *transfer;

		g_variant_get (parameters, "(&o)", &transfer);

		g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
					  G_DBUS_PROXY_FLAGS_NONE,
					  NULL,
					  MANAGER_SERVICE,
					  transfer,
					  TRANSFER_IFACE,
					  NULL,
					  obex_agent_authorize_push,
					  invocation);
	} else {
		g_warning ("Unknown method name or unknown parameters: %s",
			   method_name);
	}
}

static const GDBusInterfaceVTable interface_vtable =
{
  handle_method_call,
  NULL,
  NULL
};

static void
on_bus_acquired (GDBusConnection *connection,
		 const gchar     *name,
		 gpointer         user_data)
{
	ObexAgent *self = user_data;

	/* parse introspection data */
	introspection_data = g_dbus_node_info_new_for_xml (introspection_xml,
							   NULL);

	self->connection = connection;
	self->object_reg_id = g_dbus_connection_register_object (connection,
								 AGENT_PATH,
								 introspection_data->interfaces[0],
								 &interface_vtable,
								 NULL,  /* user_data */
								 NULL,  /* user_data_free_func */
								 NULL); /* GError** */

	g_dbus_node_info_unref (introspection_data);

	g_assert (self->object_reg_id > 0);
}

static void
on_agent_registered (GObject *source_object,
		     GAsyncResult *res,
		     gpointer user_data)
{
	GError *error = NULL;
	GVariant *v;

	v = g_dbus_proxy_call_finish (manager, res, &error);

	if (error) {
		g_warning ("error: %s", error->message);
		g_error_free (error);
		return;
	}

	g_variant_unref (v);
}

static void
on_proxy_acquired (GObject *source_object,
		   GAsyncResult *res,
		   gpointer user_data)
{
	GError *error = NULL;

	manager = g_dbus_proxy_new_for_bus_finish (res, &error);

	if (!manager) {
		g_warning ("error: %s", error->message);
		g_error_free (error);
		return;
	}

	g_dbus_proxy_call (manager,
			   "RegisterAgent",
			   g_variant_new ("(o)", AGENT_PATH),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   NULL,
			   on_agent_registered,
			   NULL);
}

static void
on_name_acquired (GDBusConnection *connection,
		  const gchar     *name,
		  gpointer         user_data)
{
}

static void
on_name_lost (GDBusConnection *connection,
	      const gchar     *name,
	      gpointer         user_data)
{
}

static void
obex_agent_init (ObexAgent *self)
{
}

static void
obex_agent_dispose (GObject *obj)
{
	ObexAgent *self = OBEX_AGENT (obj);

	g_dbus_connection_unregister_object (self->connection, self->object_reg_id);
	g_bus_unown_name (self->owner_id);

	g_clear_object (&client);

	G_OBJECT_CLASS (obex_agent_parent_class)->dispose (obj);
}

static void
obex_agent_class_init (ObexAgentClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->dispose = obex_agent_dispose;
}

static ObexAgent *
obex_agent_new (void)
{
	ObexAgent *self = NULL;

	self = (ObexAgent *) g_object_new (OBEX_AGENT_TYPE, NULL);

	self->owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
					 AGENT_IFACE,
					 G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT,
					 on_bus_acquired,
					 on_name_acquired,
					 on_name_lost,
					 self,
					 NULL);

	client = bluetooth_client_new ();

	return self;
}

void
obex_agent_down (void)
{
	g_dbus_proxy_call (manager,
			   "UnregisterAgent",
			   g_variant_new ("(o)", AGENT_PATH),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   NULL,
			   NULL,
			   NULL);

	g_clear_object (&manager);
	g_clear_object (&agent);
}

gboolean
obex_agent_up (void)
{
	if (agent == NULL) {
		agent = obex_agent_new ();
		g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
					  G_DBUS_PROXY_FLAGS_NONE,
					  NULL,
					  MANAGER_SERVICE,
					  MANAGER_PATH,
					  MANAGER_IFACE,
					  NULL,
					  on_proxy_acquired,
					  NULL);
	}

	if (!notify_init("gnome-user-share")) {
		g_warning("Unable to initialize the notification system");
	}

	return agent != NULL;
}

void
obex_agent_set_accept_files_policy (AcceptSetting setting)
{
	accept_setting = setting;
}

void
obex_agent_set_notify (gboolean enabled)
{
	show_notifications = enabled;
}
