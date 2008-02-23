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
 *  Authors: Bastien Nocera <hadess@hadess.net>
 *
 */

#include "config.h"

#include <gtk/gtk.h>
#include <dbus/dbus-glib.h>
#include <gconf/gconf-client.h>

#include <string.h>

#include "marshal.h"
#include "obexpush.h"
#include "user_share.h"

#define DBUS_TYPE_G_STRING_VARIANT_HASHTABLE (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_STRING))

static DBusGConnection *connection = NULL;
static DBusGProxy *manager_proxy = NULL;
static DBusGProxy *server_proxy = NULL;
static AcceptSetting accept_setting = -1;
static gboolean show_notifications = FALSE;
static GtkStatusIcon *statusicon = NULL;

static void
show_icon (void)
{
	if (statusicon == NULL) {
		statusicon = gtk_status_icon_new_from_icon_name ("gnome-obex-server");
	} else {
		gtk_status_icon_set_visible (statusicon, TRUE);
	}
}

static gboolean
device_is_authorised (const char *bdaddr)
{
	DBusGConnection *connection;
	DBusGProxy *manager;
	GError *error = NULL;
	char **adapters;
	gboolean retval = FALSE;
	guint i;

	connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL);
	if (connection == NULL)
		return FALSE;

	manager = dbus_g_proxy_new_for_name (connection, "org.bluez",
					     "/org/bluez", "org.bluez.Manager");
	if (manager == NULL) {
		dbus_g_connection_unref (connection);
		return FALSE;
	}

	if (dbus_g_proxy_call (manager, "ListAdapters", &error, G_TYPE_INVALID, G_TYPE_STRV, &adapters, G_TYPE_INVALID) == FALSE) {
		g_object_unref (manager);
		dbus_g_connection_unref (connection);
		return FALSE;
	}

	for (i = 0; adapters[i] != NULL; i++) {
		DBusGProxy *adapter;
		gboolean bonded, trusted;

		g_message ("checking adapter %s", adapters[i]);

		adapter = dbus_g_proxy_new_for_name (connection, "org.bluez",
						    adapters[i], "org.bluez.Adapter");
		if (dbus_g_proxy_call (adapter, "HasBonding", NULL,
				      G_TYPE_STRING, bdaddr, G_TYPE_INVALID,
				      G_TYPE_BOOLEAN, &bonded, G_TYPE_INVALID) != FALSE) {
			g_message ("%s is %s", bdaddr, bonded ? "bonded" : "not bonded");
			if (bonded != FALSE) {
				retval = TRUE;
				g_object_unref (adapter);
				break;
			}
		}
		if (accept_setting == ACCEPT_BONDED_AND_TRUSTED &&
		    dbus_g_proxy_call (adapter, "IsTrusted", NULL,
				       G_TYPE_STRING, bdaddr, G_TYPE_INVALID,
				       G_TYPE_BOOLEAN, &trusted, G_TYPE_INVALID) != FALSE) {
			g_message ("%s is %s", bdaddr, trusted ? "trusted" : "not trusted");
			if (trusted != FALSE) {
				retval = TRUE;
				g_object_unref (adapter);
				break;
			}
		}

		g_object_unref(adapter);
	}

	g_strfreev(adapters);
	g_object_unref(manager);
	dbus_g_connection_unref(connection);

	return retval;
}

static void
transfer_started_cb (DBusGProxy *session,
		     const char *filename,
		     const char *local_path,
		     guint64 size,
		     gpointer user_data)
{
	GHashTable *dict;
	DBusGProxy *server = (DBusGProxy *) user_data;
	GError *error = NULL;
	gboolean authorise;

	g_message ("transfer started on %s", local_path);
	g_object_set_data_full (G_OBJECT (session), "filename", g_strdup (local_path), (GDestroyNotify) g_free);

	show_icon ();

	/* First transfer of the session */
	if (g_object_get_data (G_OBJECT (session), "bdaddr") == NULL) {
		const char *bdaddr;

		if (dbus_g_proxy_call (server, "GetServerSessionInfo", &error,
				       DBUS_TYPE_G_OBJECT_PATH, dbus_g_proxy_get_path (session), G_TYPE_INVALID,
				       DBUS_TYPE_G_STRING_VARIANT_HASHTABLE, &dict, G_TYPE_INVALID) == FALSE) {
			g_printerr ("Getting Server session info failed: %s\n",
				    error->message);
			g_error_free (error);
			return;
		}

		bdaddr = g_hash_table_lookup (dict, "BluetoothAddress");
		g_message ("transfer started for device %s", bdaddr);

		g_object_set_data_full (G_OBJECT (session), "bdaddr", g_strdup (bdaddr), (GDestroyNotify) g_free);
		/* Initial accept method is undefined, we do that lower down */
		g_object_set_data (G_OBJECT (session), "accept-method", GINT_TO_POINTER (-1));
		g_hash_table_destroy (dict);
	}

	/* Accept method changed */
	if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (session), "accept-method")) != accept_setting) {
		const char *bdaddr;

		bdaddr = g_object_get_data (G_OBJECT (session), "bdaddr");

		if (bdaddr == NULL) {
			g_warning ("Couldn't get the Bluetooth address for the device, rejecting the transfer");
			authorise = FALSE;
		} else if (accept_setting == ACCEPT_ALWAYS) {
			authorise = TRUE;
		} else if (accept_setting == ACCEPT_BONDED || accept_setting == ACCEPT_BONDED_AND_TRUSTED) {
			authorise = device_is_authorised (bdaddr);
		} else {
			//FIXME implement
			g_warning ("\"Ask\" authorisation method not implemented");
			authorise = FALSE;
		}
		g_object_set_data (G_OBJECT (session), "authorise", GINT_TO_POINTER (authorise));
	}

	g_message ("accept_setting: %d", accept_setting);

	authorise = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (session), "authorise"));
	if (authorise != FALSE) {
		if (dbus_g_proxy_call (session, "Accept", &error, G_TYPE_INVALID, G_TYPE_INVALID) == FALSE) {
			g_printerr ("Failed to accept file transfer: %s\n", error->message);
			g_error_free (error);
			return;
		}
		g_message ("authorised transfer");
	} else {
		if (dbus_g_proxy_call (session, "Reject", &error, G_TYPE_INVALID, G_TYPE_INVALID) == FALSE) {
			g_printerr ("Failed to reject file transfer: %s\n", error->message);
			g_error_free (error);
			return;
		}
		g_message ("rejected transfer");
	}
}

static void
transfer_completed_cb (DBusGProxy *session,
		       gpointer user_data)
{
	g_message ("file finish transfer: %s",
		   (char *) g_object_get_data (G_OBJECT (session), "filename"));
	//FIXME display a notification if "show_notifications" is TRUE
	g_object_set_data (G_OBJECT (session), "filename", NULL);

	//FIXME only hide it when the popup's been dismissed or timed out
	gtk_status_icon_set_visible (statusicon, FALSE);
}

static void
cancelled_cb (DBusGProxy *session,
	      gpointer user_data)
{
	//FIXME implement properly
	transfer_completed_cb (session, user_data);
}

static void
error_occurred_cb (DBusGProxy *session,
		   const char *error_name,
		   const char *error_message,
		   gpointer user_data)
{
	//FIXME implement properly
	g_message ("transfer error occurred: %s", error_message);
	transfer_completed_cb (session, user_data);
}

static void
session_created_cb (DBusGProxy *server, const char *session_path, gpointer user_data)
{
	DBusGProxy *session;

	session = dbus_g_proxy_new_for_name (connection,
					     "org.openobex",
					     session_path,
					     "org.openobex.ServerSession");

	dbus_g_proxy_add_signal (session, "TransferStarted",
				 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT64, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(session, "TransferStarted",
				    G_CALLBACK (transfer_started_cb), server, NULL);
	dbus_g_proxy_add_signal (session, "TransferCompleted",
				 G_TYPE_INVALID, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(session, "TransferCompleted",
				    G_CALLBACK (transfer_completed_cb), server, NULL);
	dbus_g_proxy_add_signal (session, "Cancelled",
				 G_TYPE_INVALID, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(session, "Cancelled",
				    G_CALLBACK (cancelled_cb), server, NULL);
	dbus_g_proxy_add_signal (session, "ErrorOccurred",
				 G_TYPE_INVALID, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(session, "ErrorOccurred",
				    G_CALLBACK (error_occurred_cb), server, NULL);
}

void
obexpush_up (void)
{
	GError *err = NULL;
	char *download_dir, *server;

	server = NULL;
	if (manager_proxy == NULL) {
		manager_proxy = dbus_g_proxy_new_for_name (connection,
							   "org.openobex",
							   "/org/openobex",
							   "org.openobex.Manager");
		if (dbus_g_proxy_call (manager_proxy, "CreateBluetoothServer",
				       &err, G_TYPE_STRING, "00:00:00:00:00:00", G_TYPE_STRING, "opp", G_TYPE_BOOLEAN, FALSE, G_TYPE_INVALID,
				       DBUS_TYPE_G_OBJECT_PATH, &server, G_TYPE_INVALID) == FALSE) {
			g_printerr ("Creating Bluetooth ObexPush server failed: %s\n",
				    err->message);
			g_error_free (err);
			g_object_unref (manager_proxy);
			manager_proxy = NULL;
			return;
		}
	}

	download_dir = lookup_download_dir ();

	if (server_proxy == NULL) {
		server_proxy = dbus_g_proxy_new_for_name (connection,
							   "org.openobex",
							   server,
							   "org.openobex.Server");
		g_free (server);

		dbus_g_proxy_add_signal (server_proxy, "SessionCreated",
					 DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
		dbus_g_proxy_connect_signal(server_proxy, "SessionCreated",
					    G_CALLBACK (session_created_cb), NULL, NULL);
	}

	if (dbus_g_proxy_call (server_proxy, "Start", &err,
			   G_TYPE_STRING, download_dir, G_TYPE_BOOLEAN, TRUE, G_TYPE_BOOLEAN, FALSE, G_TYPE_INVALID,
			   G_TYPE_INVALID) == FALSE) {
		g_printerr ("Starting Bluetooth ObexPush server failed: %s\n",
			    err->message);
		g_error_free (err);
		g_free (download_dir);
		g_object_unref (server_proxy);
		server_proxy = NULL;
		g_object_unref (manager_proxy);
		manager_proxy = NULL;
		return;
	}

	g_free (download_dir);
}

static void
obexpush_stop (gboolean stop_manager)
{
	GError *err = NULL;

	if (server_proxy == NULL)
		return;

	if (dbus_g_proxy_call (server_proxy, "Close", &err, G_TYPE_INVALID, G_TYPE_INVALID) == FALSE) {
		const gchar *error_name = NULL;

		if (err != NULL && err->code == DBUS_GERROR_REMOTE_EXCEPTION)
			error_name = dbus_g_error_get_name (err);
		if (error_name == NULL ||
		    (error_name != NULL && strcmp (error_name, "org.openobex.Error.NotStarted") != 0)) {
			g_printerr ("Stopping Bluetooth ObexPush server failed: %s\n",
				    err->message);
			g_error_free (err);
			return;
		}
		g_error_free (err);
	}

	if (stop_manager != FALSE) {
		g_object_unref (server_proxy);
		server_proxy = NULL;
		g_object_unref (manager_proxy);
		manager_proxy = NULL;
	}
}

void
obexpush_down (void)
{
	obexpush_stop (TRUE);
}

void
obexpush_restart (void)
{
	obexpush_stop (FALSE);
	obexpush_up ();
}

gboolean
obexpush_init (void)
{
	GError *err = NULL;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &err);
	if (connection == NULL) {
		g_printerr ("Connecting to session bus failed: %s\n",
			    err->message);
		g_error_free (err);
		return FALSE;
	}

	dbus_g_object_register_marshaller (marshal_VOID__STRING_STRING_UINT64,
					   G_TYPE_NONE,
					   G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT64, G_TYPE_INVALID);

	return TRUE;
}

void
obexpush_set_accept_files_policy (AcceptSetting setting)
{
	accept_setting = setting;
}

void
obexpush_set_notify (gboolean enabled)
{
	show_notifications = enabled;
}

