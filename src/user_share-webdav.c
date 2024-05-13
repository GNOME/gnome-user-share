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
#include "http.h"

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

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

static void
setttings_changed (GSettings *settings,
		   gchar *path,
		   gpointer data)
{
	if (g_strcmp0 (FILE_SHARING_REQUIRE_PASSWORD, path) == 0)
		require_password_changed ();
}

static gboolean
signal_handler (gpointer user_data)
{
	GApplication *app = user_data;
	g_application_quit (app);
	return FALSE;
}

static void
user_share_webdav_bus_closed (GDBusConnection *connection,
			      gboolean remote_peer_vanished,
			      GError *error,
			      GApplication *app)
{
	http_down ();
	_exit (2);
}

static void
user_share_webdav_activate (GApplication *app)
{
	/* Initial setting */
	http_up ();
	g_application_hold (app);
}

static void
user_share_webdav_startup (GApplication *app)
{
	GDBusConnection *connection;
	GSettings *settings;

	signal (SIGPIPE, SIG_IGN);
	g_unix_signal_add (SIGINT, signal_handler, app);
	g_unix_signal_add (SIGHUP, signal_handler, app);
	g_unix_signal_add (SIGTERM, signal_handler, app);

	migrate_old_configuration ();

	connection = g_application_get_dbus_connection (app);
	g_signal_connect (connection, "closed",
			  G_CALLBACK (user_share_webdav_bus_closed), app);

	settings = g_settings_new (GNOME_USER_SHARE_SCHEMAS);
	g_signal_connect (settings, "changed", G_CALLBACK (setttings_changed), NULL);
	g_object_set_data_full (G_OBJECT (app), "user_share_settings", settings, g_object_unref);
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

	application = g_application_new ("org.gnome.user-share.webdav",
					 G_APPLICATION_FLAGS_NONE);
	g_signal_connect (application, "startup",
			  G_CALLBACK (user_share_webdav_startup), NULL);
	g_signal_connect (application, "activate",
			  G_CALLBACK (user_share_webdav_activate), NULL);
	res = g_application_run (application, argc, argv);

	http_down ();
	g_object_unref (application);

	return res;
}
