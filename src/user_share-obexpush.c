/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */

/*
 *  Copyright (C) 2004-2008 Red Hat, Inc.
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
 *  Authors: Alexander Larsson <alexl@redhat.com>
 *
 */

#include "config.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <glib-unix.h>

#include "user_share-common.h"
#include "user_share-private.h"
#include "obexpush.h"

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <bluetooth-client.h>

/* GNOME Session */
#define GNOME_SESSION_DBUS_NAME      "org.gnome.SessionManager"
#define GNOME_SESSION_DBUS_OBJECT    "/org/gnome/SessionManager"
#define GNOME_SESSION_DBUS_INTERFACE "org.gnome.SessionManager"

static GSettings *settings = NULL;

static GDBusProxy *session_proxy = NULL;
static gboolean has_console = TRUE;

#define OBEX_ENABLED (has_console)
static gboolean
obex_service_should_run (void)
{
	return OBEX_ENABLED && g_settings_get_boolean (settings, FILE_SHARING_BLUETOOTH_OBEXPUSH_ENABLED);
}

static void
session_properties_changed_cb (GDBusProxy      *session,
			       GVariant        *changed,
			       char           **invalidated,
			       gpointer         user_data)
{
	GVariant *v;

	v = g_variant_lookup_value (changed, "SessionIsActive", G_VARIANT_TYPE_BOOLEAN);
	if (v) {
		has_console = g_variant_get_boolean (v);
		g_debug ("Received session is active change: now %s", has_console ? "active" : "inactive");

		if (obex_service_should_run ())
			obex_agent_up ();
		else
			obex_agent_down ();

		g_variant_unref (v);
	}
}

static gboolean
is_session_active (void)
{
	GVariant *variant;
	gboolean is_session_active = FALSE;

	variant = g_dbus_proxy_get_cached_property (session_proxy,
						    "SessionIsActive");
	if (variant) {
		is_session_active = g_variant_get_boolean (variant);
		g_variant_unref (variant);
	}

	return is_session_active;
}

static void
session_init (void)
{
	GError *error = NULL;

	session_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
						       G_DBUS_PROXY_FLAGS_NONE,
						       NULL,
						       GNOME_SESSION_DBUS_NAME,
						       GNOME_SESSION_DBUS_OBJECT,
						       GNOME_SESSION_DBUS_INTERFACE,
						       NULL,
						       &error);
	if (session_proxy == NULL) {
		g_warning ("Failed to get session proxy: %s", error->message);
		g_error_free (error);
		return;
	}
	g_signal_connect (session_proxy, "g-properties-changed",
			  G_CALLBACK (session_properties_changed_cb),
			  NULL);
	has_console = is_session_active ();
}

static void
file_sharing_bluetooth_obexpush_enabled_changed (void)
{
	if (obex_service_should_run ()) {
		obex_agent_up ();
	} else {
		obex_agent_down ();
		_exit (0);
	}
}

static void
file_sharing_bluetooth_obexpush_accept_files_changed (void)
{
	AcceptSetting setting;
	char *str;

	str = g_settings_get_string (settings, FILE_SHARING_BLUETOOTH_OBEXPUSH_ACCEPT_FILES);
	setting = accept_setting_from_string (str);
	g_free (str);

	obex_agent_set_accept_files_policy (setting);
}

static void
file_sharing_bluetooth_obexpush_notify_changed (void)
{
	obex_agent_set_notify (g_settings_get_boolean (settings, FILE_SHARING_BLUETOOTH_OBEXPUSH_NOTIFY));
}

static void
settings_changed (GSettings *settings,
		   gchar *path,
		   gpointer data)
{
	if (g_strcmp0 (FILE_SHARING_BLUETOOTH_OBEXPUSH_ENABLED, path) == 0)
		file_sharing_bluetooth_obexpush_enabled_changed ();

	else if (g_strcmp0 (FILE_SHARING_BLUETOOTH_OBEXPUSH_ACCEPT_FILES, path) == 0)
		file_sharing_bluetooth_obexpush_accept_files_changed ();

	else if (g_strcmp0 (FILE_SHARING_BLUETOOTH_OBEXPUSH_NOTIFY, path) == 0)
		file_sharing_bluetooth_obexpush_notify_changed ();
}

static gboolean
signal_handler (gpointer user_data)
{
	GApplication *app = user_data;
	g_application_quit (app);
	return FALSE;
}

static void
user_share_obexpush_bus_closed (GDBusConnection *connection,
				gboolean remote_peer_vanished,
				GError *error,
				GApplication *app)
{
	obex_agent_down ();
	_exit (2);
}

static void
user_share_obexpush_activate (GApplication *app)
{
	if (!obex_service_should_run ())
		return;

	/* Initial setting */
	file_sharing_bluetooth_obexpush_accept_files_changed ();
	file_sharing_bluetooth_obexpush_notify_changed ();
	file_sharing_bluetooth_obexpush_enabled_changed ();
	g_application_hold (app);
}

static void
user_share_obexpush_startup (GApplication *app)
{
	GDBusConnection *connection;

	signal (SIGPIPE, SIG_IGN);
	g_unix_signal_add (SIGINT, signal_handler, app);
	g_unix_signal_add (SIGHUP, signal_handler, app);
	g_unix_signal_add (SIGTERM, signal_handler, app);

	connection = g_application_get_dbus_connection (app);
	g_signal_connect (connection, "closed",
			  G_CALLBACK (user_share_obexpush_bus_closed), app);

	session_init ();

	settings = g_settings_new (GNOME_USER_SHARE_SCHEMAS);
	g_signal_connect (settings, "changed", G_CALLBACK (settings_changed), NULL);
}

int
main (int argc, char **argv)
{
	GApplication *application;
	gint res;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	if (g_strcmp0 (g_get_real_name (), "root") == 0) {
		g_warning ("gnome-user-share cannot be started as root for security reasons.");
		return 1;
	}

	application = g_application_new ("org.gnome.user-share.obexpush",
					 G_APPLICATION_FLAGS_NONE);
	g_signal_connect (application, "startup",
			  G_CALLBACK (user_share_obexpush_startup), NULL);
	g_signal_connect (application, "activate",
			  G_CALLBACK (user_share_obexpush_activate), NULL);

	res = g_application_run (application, argc, argv);

	obex_agent_down ();
	g_object_unref (application);

	return res;
}

