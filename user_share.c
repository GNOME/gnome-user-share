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

#include <glib.h>
#include <glib/gi18n.h>
#include <X11/Xlib.h>

#include "user_share.h"
#include "user_share-private.h"
#include "http.h"
#include "obexftp.h"

#include <gconf/gconf-client.h>

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

static GMainLoop *loop = NULL;
static guint disabled_timeout_tag = 0;

char *
lookup_public_dir (void)
{
	const char *public_dir;
	char *dir;

	public_dir = g_get_user_special_dir (G_USER_DIRECTORY_PUBLIC_SHARE);
	if (public_dir != NULL && strcmp (public_dir, g_get_home_dir ()) != 0) {
		g_mkdir_with_parents (public_dir, 0755);
		return g_strdup (public_dir);
	}

	dir = g_build_filename (g_get_home_dir (), "Public", NULL);
	g_mkdir_with_parents (dir, 0755);
	return dir;
}

char *
lookup_download_dir (void)
{
	const char *download_dir;
	char *dir;

	download_dir = g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD);
	if (download_dir != NULL && strcmp (download_dir, g_get_home_dir ()) != 0) {
		g_mkdir_with_parents (download_dir, 0755);
		return g_strdup (download_dir);
	}

	dir = g_build_filename (g_get_home_dir (), "Download", NULL);
	g_mkdir_with_parents (dir, 0755);
	return dir;
}

static void
require_password_changed (GConfClient* client,
			  guint cnxn_id,
			  GConfEntry *entry,
			  gpointer data)
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
disabled_timeout_callback (gpointer user_data)
{
	GConfClient* client = (GConfClient *) user_data;
	http_down ();

	if (gconf_client_get_bool (client,
				   FILE_SHARING_BLUETOOTH_ENABLED, NULL) == FALSE)
		_exit (0);
	return FALSE;
}

static void
file_sharing_enabled_changed (GConfClient* client,
			      guint cnxn_id,
			      GConfEntry *entry,
			      gpointer data)
{
	gboolean enabled;

	if (disabled_timeout_tag != 0) {
		g_source_remove (disabled_timeout_tag);
		disabled_timeout_tag = 0;
	}

	enabled = gconf_client_get_bool (client,
					 FILE_SHARING_ENABLED, NULL);
	if (enabled) {
		if (http_get_pid () == 0) {
			http_up ();
		}
	} else {
		http_down ();
		disabled_timeout_tag = g_timeout_add (3*1000,
						      (GSourceFunc)disabled_timeout_callback,
						      client);
	}
}

static void
file_sharing_bluetooth_allow_write_changed (GConfClient* client,
					    guint cnxn_id,
					    GConfEntry *entry,
					    gpointer data)
{
	obexftp_restart ();
}

static void
file_sharing_bluetooth_require_pairing_changed (GConfClient* client,
						guint cnxn_id,
						GConfEntry *entry,
						gpointer data)
{
	/* We need to fully reset the session,
	 * otherwise the new setting isn't taken into account */
	obexftp_down ();
	obexftp_up ();
}

static void
file_sharing_bluetooth_enabled_changed (GConfClient* client,
					guint cnxn_id,
					GConfEntry *entry,
					gpointer data)
{
	if (gconf_client_get_bool (client,
				   FILE_SHARING_BLUETOOTH_ENABLED, NULL) == FALSE) {
		obexftp_down ();
		if (gconf_client_get_bool (client,
					   FILE_SHARING_ENABLED, NULL) == FALSE) {
			_exit (0);
		}
	} else {
		obexftp_up ();
	}
}

static RETSIGTYPE
cleanup_handler (int sig)
{
	http_down ();
	obexftp_down ();
	_exit (2);
}

static int
x_io_error_handler (Display *xdisplay)
{
	http_down ();
	obexftp_down ();
	_exit (2);
}

static gboolean
x_input (GIOChannel  *io_channel,
	 GIOCondition cond,
	 gpointer     callback_data)
{
	Display *xdisplay;
	XEvent ignored;

	xdisplay = callback_data;
	while (XPending (xdisplay)) {
		XNextEvent (xdisplay, &ignored);
	}
	return TRUE;
}

int
main (int argc, char **argv)
{
	GConfClient *client;
	Display *xdisplay;
	int x_fd;
	GIOChannel *channel;
	Window selection_owner;
	Atom xatom;

	g_type_init ();
	loop = g_main_loop_new (NULL, FALSE);

	signal (SIGPIPE, SIG_IGN);
	signal (SIGINT, cleanup_handler);
	signal (SIGHUP, cleanup_handler);
	signal (SIGTERM, cleanup_handler);

	xdisplay = XOpenDisplay (NULL);
	if (xdisplay == NULL) {
		fprintf (stderr, "Can't open display\n");
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

	client = gconf_client_get_default ();
	if (gconf_client_get_bool (client, FILE_SHARING_ENABLED, NULL) == FALSE &&
	    gconf_client_get_bool (client, FILE_SHARING_BLUETOOTH_ENABLED, NULL) == FALSE)
		return 1;

	x_fd = ConnectionNumber (xdisplay);
	XSetIOErrorHandler (x_io_error_handler);

	channel = g_io_channel_unix_new (x_fd);
	g_io_add_watch (channel,
			G_IO_IN,
			x_input, xdisplay);
	g_io_channel_unref (channel);

	if (http_init () == FALSE)
		return 1;
	if (obexftp_init () == FALSE)
		return 1;

	gconf_client_add_dir (client,
			      FILE_SHARING_DIR,
			      GCONF_CLIENT_PRELOAD_RECURSIVE,
			      NULL);

	gconf_client_notify_add (client,
				 FILE_SHARING_ENABLED,
				 file_sharing_enabled_changed,
				 NULL,
				 NULL,
				 NULL);
	gconf_client_notify_add (client,
				 FILE_SHARING_REQUIRE_PASSWORD,
				 require_password_changed,
				 NULL,
				 NULL,
				 NULL);
	gconf_client_notify_add (client,
				 FILE_SHARING_BLUETOOTH_ENABLED,
				 file_sharing_bluetooth_enabled_changed,
				 NULL,
				 NULL,
				 NULL);
	gconf_client_notify_add (client,
				 FILE_SHARING_BLUETOOTH_ALLOW_WRITE,
				 file_sharing_bluetooth_allow_write_changed,
				 NULL,
				 NULL,
				 NULL);
	gconf_client_notify_add (client,
				 FILE_SHARING_BLUETOOTH_REQUIRE_PAIRING,
				 file_sharing_bluetooth_require_pairing_changed,
				 NULL,
				 NULL,
				 NULL);

	g_object_unref (client);

	/* Initial setting */
	file_sharing_enabled_changed (client, 0, NULL, NULL);
	file_sharing_bluetooth_enabled_changed (client, 0, NULL, NULL);

	g_main_loop_run (loop);

	return 0;
}

