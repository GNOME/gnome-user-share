/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */

/*
 *  Copyright (C) 2004 Red Hat, Inc.
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

#ifdef HAVE_AVAHI
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>
#endif /* HAVE_AVAHI */

#ifdef HAVE_HOWL
/* Workaround broken howl installing config.h */
#undef PACKAGE
#undef VERSION
#include <howl.h>
#endif /* HAVE_HOWL */

#include <gconf/gconf-client.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>

#ifdef HAVE_SELINUX
#include <selinux/selinux.h>
#endif

#define FILE_SHARING_DIR "/desktop/gnome/file_sharing"
#define FILE_SHARING_ENABLED "/desktop/gnome/file_sharing/enabled"
#define FILE_SHARING_REQUIRE_PASSWORD "/desktop/gnome/file_sharing/require_password"

static GMainLoop *loop = NULL;
static pid_t httpd_pid = 0;
static guint disabled_timeout_tag = 0;

static int
get_port (void)
{
    int sock;
    struct sockaddr_in addr;
    int reuse;
    socklen_t len;
    
    sock = socket (PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
		return -1;
    }
    
    memset (&addr, 0, sizeof (addr));
    addr.sin_port = 0;
    addr.sin_addr.s_addr = INADDR_ANY;
    
    reuse = 1;
    setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof (reuse));
    if (bind (sock, (struct sockaddr *)&addr, sizeof (addr)) == -1) {
		close (sock);
		return -1;
    }
    
    len = sizeof (addr);
    if (getsockname (sock, (struct sockaddr *)&addr, &len) == -1) {
		close (sock);
		return -1;
    }
    
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__APPLE__)
    /* XXX This exposes a potential race condition, but without this,
     * httpd will not start on the above listed platforms due to the fact
     * that SO_REUSEADDR is also needed when Apache binds to the listening
     * socket.  At this time, Apache does not support that socket option.
     */
    close (sock);
#endif
    return ntohs (addr.sin_port);
}


static char *
get_share_name (void)
{
	static char *name = NULL;
	if (name == NULL) {
		/* Translators: The %s will get filled in with the user name
		   of the user, to form a genitive. If this is difficult to
		   translate correctly so that it will work correctly in your
		   language, you may use something equivalent to
		   "Public files of %s", or leave out the %s altogether.
		   In the latter case, please put "%.0s" somewhere in the string,
		   which will match the user name string passed by the C code,
		   but not put the user name in the final string. This is to
		   avoid the warning that msgfmt might otherwise generate. */
		name = g_strdup_printf (_("%s's public files"), g_get_user_name ());
	}
	return name;
}


#ifdef HAVE_AVAHI

static AvahiClient *avahi_client = NULL;
static gboolean avahi_should_publish = FALSE;
static gboolean avahi_running = FALSE;
static int avahi_port = 0;
static AvahiEntryGroup *entry_group = NULL;
static char *avahi_name = NULL;


static gboolean
create_service (void) {
    int ret;

	if (avahi_name == NULL) {
		avahi_name = g_strdup (get_share_name ());
	}

	ret = avahi_entry_group_add_service (entry_group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 
										 AVAHI_PUBLISH_USE_MULTICAST,
										 avahi_name, "_webdav._tcp", NULL, NULL, 
										 avahi_port, "u=guest", NULL);
	if (ret < 0) {
		return FALSE;
	}

	ret = avahi_entry_group_commit (entry_group);

    if (ret < 0) {
        return FALSE;
    }

    return TRUE;
}

static void 
entry_group_callback (AvahiEntryGroup *g, 
					  AvahiEntryGroupState state, 
					  void *userdata) 
{
    if (state == AVAHI_ENTRY_GROUP_ESTABLISHED) {
    } else if (state == AVAHI_ENTRY_GROUP_COLLISION) {
        char *n;

        /* A service name collision happened. Let's pick a new name */
        n = avahi_alternative_service_name (avahi_name);
        g_free (avahi_name);
        avahi_name = n;

        fprintf (stderr, "Service name collision, renaming service to '%s'\n", avahi_name);

        /* And recreate the services */
        create_service();
    }
}

static void
avahi_client_callback (AvahiClient *client, AvahiClientState state, void *userdata)
{
    if (state == AVAHI_CLIENT_S_RUNNING) {
		avahi_running = TRUE;
		if (avahi_should_publish) {
			create_service ();
		}
	} else if (state == AVAHI_CLIENT_S_COLLISION) {
		avahi_entry_group_reset (entry_group);
    } else if (state == AVAHI_CLIENT_FAILURE) {
		avahi_running = FALSE;
    }
}

static gboolean
init_avahi (void)
{
	AvahiGLibPoll *poll;
	int error;

    avahi_set_allocator (avahi_glib_allocator ());

	poll = avahi_glib_poll_new (NULL, G_PRIORITY_DEFAULT);

	/* Create a new AvahiClient instance */
	avahi_client = avahi_client_new (avahi_glib_poll_get (poll),
									 AVAHI_CLIENT_NO_FAIL,
									 avahi_client_callback,
									 NULL,
									 &error);
	if (avahi_client == NULL) {
		return FALSE;
	}

	entry_group = avahi_entry_group_new (avahi_client, entry_group_callback, NULL);
	if (entry_group == NULL) {
		return FALSE;
	}
	
	return TRUE;
}


static gboolean
publish_service (int port)
{
	avahi_should_publish = TRUE;
	avahi_port = port;
	if (avahi_running) {
		create_service ();
	}
	return TRUE;
}

static void
stop_publishing (void)
{
	avahi_should_publish = FALSE;
	avahi_entry_group_reset (entry_group);
}
#endif

#ifdef HAVE_HOWL

static sw_discovery_publish_id published_id = 0;
static sw_discovery howl_session;

static gboolean
howl_input (GIOChannel  *io_channel,
			GIOCondition cond,
			gpointer     callback_data)
{
	sw_discovery session;
	session = callback_data;
	sw_salt salt;

	if (sw_discovery_salt (session, &salt) == SW_OKAY) {
		sw_salt_lock (salt);
		sw_discovery_read_socket (session);
		sw_salt_unlock (salt);
	}
	return TRUE;
}

static void
set_up_howl_session (sw_discovery session)
{
	int fd;
	GIOChannel *channel;

	fd = sw_discovery_socket (session);

	channel = g_io_channel_unix_new (fd);
	g_io_add_watch (channel,
					G_IO_IN,
					howl_input, session);
	g_io_channel_unref (channel);
}

static sw_result
publish_reply (sw_discovery			discovery,
			   sw_discovery_publish_status	status,
			   sw_discovery_oid			id,
			   sw_opaque			extra)
{
	return SW_OKAY;
}


static gboolean
publish_service (int port)
{
    sw_result result;

    result = sw_discovery_publish (howl_session, 0,
								   get_share_name (),
								   "_webdav._tcp",
								   NULL, NULL,
								   port,
								   /* TODO: should be u=guest */
								   /* text */ (unsigned char *) "", 0,
								   publish_reply, NULL, &published_id);
    if (result != SW_OKAY) {
		return FALSE;
    }
    return TRUE;
}

static void
stop_publishing (void)
{
    if (published_id != 0)
		sw_discovery_cancel (howl_session, published_id);
    published_id = 0;
}
#endif /* HAVE_HOWL */


static void
ensure_public_dir (void)
{
    char *dirname;

    dirname = g_build_filename (g_get_home_dir (), "Public", NULL);
    if (!g_file_test (dirname, G_FILE_TEST_IS_DIR)) {
		mkdir (dirname, 0755);
    }
    g_free (dirname);
}

static void
ensure_conf_dir (void)
{
    char *dirname;

    dirname = g_build_filename (g_get_home_dir (), ".gnome2", NULL);
    if (!g_file_test (dirname, G_FILE_TEST_IS_DIR)) {
		mkdir (dirname, 0755);
    }
    g_free (dirname);
    
    dirname = g_build_filename (g_get_home_dir (), ".gnome2", "user-share", NULL);
    if (!g_file_test (dirname, G_FILE_TEST_IS_DIR)) {
		mkdir (dirname, 0755);
    }
    g_free (dirname);
}

static void
httpd_child_setup (gpointer user_data)
{
#ifdef HAVE_SELINUX
	char *mycon;
	/* If selinux is enabled, avoid transitioning to the httpd_t context,
	   as this normally means you can't read the users homedir. */
	if (is_selinux_enabled()) {
		if (getcon (&mycon) < 0) {
			abort ();
		}
		if (setexeccon (mycon) < 0)
			abort ();
		freecon (mycon);
	}
#endif
}

static gboolean
spawn_httpd (int port, pid_t *pid_out)
{
    char *free1, *free2;
    gboolean res;
    char *argv[10];
    char *env[10];
    int i;
    gint status;
    char *pid_filename;
    char *pidfile;
    GError *error;
    gboolean got_pidfile;
    GConfClient *client;
    char *str;

    ensure_public_dir ();
    ensure_conf_dir ();

    i = 0;
    argv[i++] = HTTPD_PROGRAM;
    argv[i++] = "-f";
    argv[i++] = HTTPD_CONFIG;
    argv[i++] = "-C";
    free1 = argv[i++] = g_strdup_printf ("Listen %d", port);

    client = gconf_client_get_default ();
    str = gconf_client_get_string (client,
				   FILE_SHARING_REQUIRE_PASSWORD, NULL);

    if (str && strcmp (str, "never") == 0) {
		/* Do nothing */
    } else if (str && strcmp (str, "on_write") == 0){
		argv[i++] = "-D";
		argv[i++] = "RequirePasswordOnWrite";
    } else {
		/* always, or safe fallback */
		argv[i++] = "-D";
		argv[i++] = "RequirePasswordAlways";
    }
    
    g_object_unref (client);
    
    argv[i] = NULL;

    i = 0;
    free2 = env[i++] = g_strdup_printf("HOME=%s", g_get_home_dir());
    env[i++] = "LANG=C";
    env[i] = NULL;

    pid_filename = g_build_filename (g_get_home_dir (), ".gnome2/user-share/pid", NULL);

    /* Remove pid file before spawning to avoid races with child and old pidfile */
    unlink (pid_filename);
    
    error = NULL;
    res = g_spawn_sync (g_get_home_dir(),
			argv, env, 0,
			httpd_child_setup, NULL,
			NULL, NULL,
			&status,
			&error);
    g_free (free1);
    g_free (free2);
    
    if (!res) {
		fprintf (stderr, "error spawning httpd: %s\n",
				 error->message);
		g_error_free (error);
		return FALSE;
    }

    if (status != 0) {
		g_free (pid_filename);
		return FALSE;
    }

    got_pidfile = FALSE;
    error = NULL;
    for (i = 0; i < 5; i++) {
	if (error != NULL) 
	    g_error_free (error); 
	error = NULL;
	if (g_file_get_contents (pid_filename, &pidfile, NULL, &error)) {
	    got_pidfile = TRUE;
	    *pid_out = atoi (pidfile);
	    g_free (pidfile);
	    break;
	}
	sleep (1);
    }
    
    g_free (pid_filename);
    
    if (!got_pidfile) {
		fprintf (stderr, "error opening httpd pidfile: %s\n", error->message);
		g_error_free (error);
		return FALSE;
    }
    return TRUE;
}

static void
kill_httpd (void)
{
    if (httpd_pid != 0) {
		kill (httpd_pid, SIGTERM);
	
		/* Allow child time to die, we can't waitpid, because its
		   not a direct child */
		sleep (1);
    }
    httpd_pid = 0;
}


static RETSIGTYPE
cleanup_handler (int sig)
{
    kill_httpd ();
    _exit (2);
}

/* File sharing was disabled for some time, exit now */
/* If we re-enable it in the ui, this will be restarted anyway */
static gboolean
disabled_timeout_callback (void)
{
    kill_httpd ();
    exit (0);
    
    return FALSE;
}

static void
up (void)
{
    guint port;
    
    port = get_port ();
    if (!spawn_httpd (port, &httpd_pid)) {
		fprintf (stderr, "spawning httpd failed\n");
    } else {
		if (!publish_service (port)) {
			fprintf (stderr, "publishing failed\n");
		}
    }
}

static void
down (void)
{
    kill_httpd ();
    stop_publishing ();
}

static void
require_password_changed (GConfClient* client,
						  guint cnxn_id,
						  GConfEntry *entry,
						  gpointer data)
{
    /* Need to restart to get new password setting */
    if (httpd_pid != 0) {
		down ();
		up ();
    }
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
		if (httpd_pid == 0) {
			up ();
		}
    } else {
		down ();
		disabled_timeout_tag = g_timeout_add (3*1000,
											  (GSourceFunc)disabled_timeout_callback,
											  NULL);
    }
}

static int
x_io_error_handler (Display *xdisplay)
{
    kill_httpd ();
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
    
    x_fd = ConnectionNumber (xdisplay);
    XSetIOErrorHandler (x_io_error_handler);
    
    channel = g_io_channel_unix_new (x_fd);
    g_io_add_watch (channel,
		    G_IO_IN,
		    x_input, xdisplay);
    g_io_channel_unref (channel);
	
#ifdef HAVE_AVAHI
	if (!init_avahi ()) {    
		/* Print out the error string */
		fprintf (stderr, "avahi init failed\n");
		return 1;
	}
#endif
#ifdef HAVE_HOWL
    if (sw_discovery_init (&howl_session) != SW_OKAY) {
		fprintf (stderr, "howl init failed\n");
		return 1;
    }
    set_up_howl_session (howl_session);
#endif

    client = gconf_client_get_default ();
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
    g_object_unref (client);

    /* Initial setting */
    file_sharing_enabled_changed (client, 0, NULL, NULL);

    g_main_loop_run (loop);
    
    return 0;
}
