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

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <glib-unix.h>
#include <X11/Xlib.h>

#include "user_share-common.h"
#include "user_share-private.h"
#include "user_share-common.h"
#include "http.h"
#include "obexpush.h"

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
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

static void
obex_services_start (void)
{
	if (has_console == FALSE)
		return;

	if (g_settings_get_boolean (settings, FILE_SHARING_BLUETOOTH_OBEXPUSH_ENABLED) == TRUE) {
		obex_agent_up ();
	}
}

static void
obex_services_shutdown (void)
{
	obex_agent_down ();
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

		if (has_console)
			obex_services_start ();
		else
			obex_services_shutdown ();

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
	if (g_settings_get_boolean (settings,
				   FILE_SHARING_BLUETOOTH_OBEXPUSH_ENABLED) == FALSE) {
		obex_agent_down ();
		_exit (0);
	} else if (OBEX_ENABLED) {
		obex_agent_up ();
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
setttings_changed (GSettings *settings,
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
	gtk_main_quit ();
	return FALSE;
}

static int
x_io_error_handler (Display *xdisplay)
{
	obex_agent_down ();
	_exit (2);
}

int
main (int argc, char **argv)
{
	Display *xdisplay;
	G_GNUC_UNUSED int x_fd;
	Window selection_owner;
	Atom xatom;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	if (g_strcmp0 (g_get_real_name (), "root") == 0) {
		g_warning ("gnome-user-share cannot be started as root for security reasons.");
		return 1;
	}

	signal (SIGPIPE, SIG_IGN);
	g_unix_signal_add (SIGINT, signal_handler, NULL);
	g_unix_signal_add (SIGHUP, signal_handler, NULL);
	g_unix_signal_add (SIGTERM, signal_handler, NULL);

	xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	if (xdisplay == NULL) {
		g_warning ("Can't open display");
		return 1;
	}

	xatom = XInternAtom (xdisplay, "_GNOME_USER_SHARE", FALSE);
	selection_owner = XGetSelectionOwner (xdisplay, xatom);

	if (selection_owner != None) {
		/* There is an owner already, quit */
		return 1;
	}

	selection_owner = XCreateSimpleWindow (xdisplay,
					       RootWindow (xdisplay, 0),
					       0, 0, 1, 1,
					       0, 0, 0);
	XSetSelectionOwner (xdisplay, xatom, selection_owner, CurrentTime);

	if (XGetSelectionOwner (xdisplay, xatom) != selection_owner) {
		/* Didn't get the selection */
		return 1;
	}

	settings = g_settings_new (GNOME_USER_SHARE_SCHEMAS);
	if (g_settings_get_boolean (settings, FILE_SHARING_BLUETOOTH_OBEXPUSH_ENABLED) == FALSE)
		return 1;

	x_fd = ConnectionNumber (xdisplay);
	XSetIOErrorHandler (x_io_error_handler);

	if (obex_agent_up () == FALSE)
		return 1;

	g_signal_connect (settings, "changed", G_CALLBACK(setttings_changed), NULL);

	session_init ();

	/* Initial setting */
	file_sharing_bluetooth_obexpush_accept_files_changed ();
	file_sharing_bluetooth_obexpush_notify_changed ();
	file_sharing_bluetooth_obexpush_enabled_changed ();

	gtk_main ();

	g_object_unref (settings);
	obex_agent_down ();

	return 0;
}
