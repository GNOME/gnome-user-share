#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <X11/Xlib.h>

/* Workaround broken howl installing config.h */
#undef PACKAGE
#undef VERSION
#include <howl.h>
#include <gconf/gconf-client.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define FILE_SHARING_DIR "/desktop/gnome/file_sharing"
#define FILE_SHARING_ENABLED "/desktop/gnome/file_sharing/enabled"

static GMainLoop *loop = NULL;
static pid_t httpd_pid = 0;
static sw_discovery_publish_id published_id = 0;
static sw_discovery howl_session;

int
get_port (void)
{
    int sock, res;
    struct sockaddr_in addr;
    int reuse;
    socklen_t len;
    
    sock = socket (PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
	return -11;
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
    
    return ntohs (addr.sin_port);
}

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


gboolean
publish_service (sw_discovery session, int port)
{
    sw_result result;
    char *share_name;

    share_name = g_strdup_printf (_("%s's public files"), g_get_user_name ());

    result = sw_discovery_publish (session, 0,
				   share_name,
				   "_webdav._tcp",
				   NULL, NULL,
				   port,
				   /* text */"", 0,
				   publish_reply, NULL, &published_id);
    g_free (share_name);
    if (result != SW_OKAY) {
	return FALSE;
    }
    return TRUE;
}

void
stop_publishing (void)
{
    if (published_id != 0)
	sw_discovery_cancel (howl_session, published_id);
    published_id = 0;
}

void
ensure_public_dir (void)
{
    char *dirname;

    dirname = g_build_filename (g_get_home_dir (), "Public", NULL);
    if (!g_file_test (dirname, G_FILE_TEST_IS_DIR)) {
	mkdir (dirname, 0755);
    }
    g_free (dirname);
}

gboolean
spawn_httpd (int port, pid_t *pid_out)
{
    char *free1, *free2;
    gboolean res;
    char *argv[10];
    char *env[10];;
    int i;
    gint status;
    char *filename;
    char *pidfile;
    GError *error;

    ensure_public_dir ();
    
    i = 0;
    argv[i++] = HTTPD_PROGRAM;
    argv[i++] = "-f";
    argv[i++] = HTTPD_CONFIG;
    argv[i++] = "-C";
    free1 = argv[i++] = g_strdup_printf ("Listen %d", port);
    argv[i] = NULL;

    i = 0;
    free2 = env[i++] = g_strdup_printf("HOME=%s", g_get_home_dir());
    env[i++] = "LANG=C";
    env[i] = NULL;
    
    
    res = g_spawn_sync (g_get_home_dir(),
			argv, env, 0,
			NULL, NULL,
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

    if (status != 0)
      return FALSE;
    
    filename = g_build_filename (g_get_home_dir (), ".userdavpid", NULL);
    error = NULL;
    if (g_file_get_contents (filename, &pidfile, NULL, error)) {
	*pid_out = atoi (pidfile);
	g_free (pidfile);
    } else {
	fprintf (stderr, "error opening httpd pidfile: %s\n", error->message);
	g_error_free (error);
    }
    
    return TRUE;
}

void
kill_httpd (void)
{
    if (httpd_pid != 0)
	kill (httpd_pid, SIGTERM);
    httpd_pid = 0;
}


static RETSIGTYPE
cleanup_handler (int sig)
{
    kill_httpd ();
    _exit (2);
}

static void
file_sharing_enabled_changed (GConfClient* client,
			      guint cnxn_id,
			      GConfEntry *entry,
			      gpointer data)
{
    gboolean enabled;
    int port;
    
    enabled = gconf_client_get_bool (client,
				     FILE_SHARING_ENABLED, NULL);
    if (enabled) {
	if (httpd_pid == 0) {
	    port = get_port ();
	    if (!spawn_httpd (port, &httpd_pid)) {
		fprintf (stderr, "spawning httpd failed\n");
	    }
	    if (!publish_service (howl_session, port)) {
		fprintf (stderr, "publishing failed\n");
	    }
	}
    } else {
	kill_httpd ();
	stop_publishing ();
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
    
    g_type_init ();
    loop = g_main_loop_new (NULL, FALSE);
    
    signal (SIGPIPE, SIG_IGN);
    signal (SIGINT, cleanup_handler);
    signal (SIGHUP, cleanup_handler);
    signal (SIGTERM, cleanup_handler);
    
    xdisplay = XOpenDisplay (NULL);
    if (xdisplay == NULL) {
	g_print ("Can't open display\n");
	return 1;
    }
    
    x_fd = ConnectionNumber (xdisplay);
    XSetIOErrorHandler (x_io_error_handler);
    
    channel = g_io_channel_unix_new (x_fd);
    g_io_add_watch (channel,
		    G_IO_IN,
		    x_input, xdisplay);
    g_io_channel_unref (channel);
	
    if (sw_discovery_init (&howl_session) != SW_OKAY) {
	g_print ("howl init failed\n");
	return 1;
    }
    set_up_howl_session (howl_session);

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

    /* Initial setting */
    file_sharing_enabled_changed (client, 0, NULL, NULL);

    g_main_loop_run (loop);
    
    return 0;
}
