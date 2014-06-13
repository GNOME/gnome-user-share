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

/* GNOME Session */
#define GNOME_SESSION_DBUS_NAME      "org.gnome.SessionManager"
#define GNOME_SESSION_DBUS_OBJECT    "/org/gnome/SessionManager"
#define GNOME_SESSION_DBUS_INTERFACE "org.gnome.SessionManager"

static GSettings *settings = NULL;

static guint disabled_timeout_tag = 0;

static void
migrate_old_configuration (void)
{
	const char *old_config_dir;
	const char *new_config_dir;

	old_config_dir = g_build_filename (g_get_home_dir (), ".gnome2", "user-share", NULL);
	new_config_dir = g_build_filename (g_get_user_config_dir (), "user-share", NULL);
	if (g_file_test (old_config_dir, G_FILE_TEST_IS_DIR))
		g_rename (old_config_dir, new_config_dir);

}

static void
require_password_changed (void)
{
	/* Need to restart to get new password setting */
	if (http_get_pid () != 0) {
		http_down ();
		http_up ();
	}
}

/* File sharing was disabled for some time, exit now */
/* If we re-enable it in the ui, this will be restarted anyway */
static gboolean
disabled_timeout_callback (void)
{
	http_down ();
	return FALSE;
}

static void
file_sharing_enabled_changed (void)
{
	gboolean enabled;

	if (disabled_timeout_tag != 0) {
		g_source_remove (disabled_timeout_tag);
		disabled_timeout_tag = 0;
	}

	enabled = g_settings_get_boolean (settings,
					 FILE_SHARING_ENABLED);
	if (enabled) {
		if (http_get_pid () == 0) {
			http_up ();
		}
	} else {
		http_down ();
		disabled_timeout_tag = g_timeout_add_seconds (3,
							      (GSourceFunc) disabled_timeout_callback,
							      NULL);
	}
}

static void
setttings_changed (GSettings *settings,
		   gchar *path,
		   gpointer data)
{
	if (g_strcmp0 (FILE_SHARING_ENABLED, path) == 0)
		file_sharing_enabled_changed ();
	else if (g_strcmp0 (FILE_SHARING_REQUIRE_PASSWORD, path) == 0)
		require_password_changed ();
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
	http_down ();
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

	migrate_old_configuration ();

	settings = g_settings_new (GNOME_USER_SHARE_SCHEMAS);
	if (g_settings_get_boolean (settings, FILE_SHARING_ENABLED) == FALSE)
		return 1;

	x_fd = ConnectionNumber (xdisplay);
	XSetIOErrorHandler (x_io_error_handler);

	if (http_init () == FALSE)
		return 1;

	g_signal_connect (settings, "changed", G_CALLBACK(setttings_changed), NULL);

	/* Initial setting */
	file_sharing_enabled_changed ();

	gtk_main ();

	g_object_unref (settings);
	http_down ();

	return 0;
}

