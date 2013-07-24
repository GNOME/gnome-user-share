/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8 -*-
 *
 * Copyright (C) 2003 Novell, Inc.
 * Copyright (C) 2003-2004 Red Hat, Inc.
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>
#include <libnautilus-extension/nautilus-menu-provider.h>
#include <libnautilus-extension/nautilus-location-widget-provider.h>

#ifdef HAVE_BLUETOOTH
#include <bluetooth-client.h>
#endif

#include "nautilus-share-bar.h"
#include "user_share-common.h"

#define NAUTILUS_TYPE_USER_SHARE  (nautilus_user_share_get_type ())
#define NAUTILUS_USER_SHARE(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_USER_SHARE, NautilusUserShare))
#define NAUTILUS_IS_USER_SHARE(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_USER_SHARE))

typedef struct
{
        GObject              parent_slot;
} NautilusUserShare;

typedef struct
{
        GObjectClass parent_slot;
} NautilusUserShareClass;

static GType nautilus_user_share_get_type      (void);
static void  nautilus_user_share_register_type (GTypeModule *module);

static void
launch_prefs_on_window (void)
{
	GDesktopAppInfo *app_info;
	GError *error = NULL;
	GdkAppLaunchContext *launch_context;

	app_info = g_desktop_app_info_new ("gnome-sharing-panel.desktop");
	if (app_info == NULL) {
		g_warning ("Could not launch Sharing settings: gnome-sharing-panel.desktop missing");
		return;
	}

	/* setup the launch context so the startup notification is correct */
	launch_context = gdk_display_get_app_launch_context (gdk_display_get_default ());
	gdk_app_launch_context_set_timestamp (launch_context, GDK_CURRENT_TIME);

	if (!g_app_info_launch (G_APP_INFO (app_info), NULL, G_APP_LAUNCH_CONTEXT (launch_context), &error)) {
		g_warning ("Could not launch '%s': %s",
			   g_app_info_get_commandline (G_APP_INFO (app_info)),
			   error->message);
		g_error_free (error);
	}

	g_object_unref (launch_context);
	g_object_unref (app_info);
}

static void
bar_response_cb (NautilusShareBar *bar,
                 gint response,
                 gpointer         data)
{
        if (response == NAUTILUS_SHARE_BAR_RESPONSE_PREFERENCES)
                launch_prefs_on_window ();
}

#ifdef HAVE_BLUETOOTH
static void
downloads_bar_set_from_bluetooth_status (GtkWidget *bar)
{
	BluetoothClient *client;
	gboolean bt_powered;

	client = g_object_get_data (G_OBJECT (bar), "bluetooth-client");
	g_object_get (G_OBJECT (client),
		      "default-adapter-powered", &bt_powered,
		      NULL);
	gtk_widget_set_visible (bar, bt_powered);
}

static void
default_adapter_powered_cb (GObject    *gobject,
			    GParamSpec *pspec,
			    GtkWidget  *bar)
{
	downloads_bar_set_from_bluetooth_status (bar);
}
#endif /* HAVE_BLUETOOTH */

static GtkWidget *
nautilus_user_share_get_location_widget (NautilusLocationWidgetProvider *iface,
                                         const char                     *uri,
                                         GtkWidget                      *window)
{
	GFile             *file;
	GtkWidget         *bar;
	guint              i;
	gboolean           enable = FALSE;
	GFile             *home;
	const GUserDirectory special_dirs[] = { G_USER_DIRECTORY_PUBLIC_SHARE, G_USER_DIRECTORY_DOWNLOAD };
	gboolean is_dir[] = { FALSE, FALSE };

	file = g_file_new_for_uri (uri);
	home = g_file_new_for_path (g_get_home_dir ());

	/* We don't show anything in $HOME */
	if (g_file_equal (home, file)) {
		g_object_unref (home);
		g_object_unref (file);
		return NULL;
	}

	g_object_unref (home);

	for (i = 0; i < G_N_ELEMENTS (special_dirs); i++) {
		GFile *dir;
		dir = lookup_dir_with_fallback (special_dirs[i]);
		if (g_file_equal (dir, file)) {
			enable = TRUE;
			is_dir[i] = TRUE;
		}
		g_object_unref (dir);
	}

	if (enable == FALSE)
		return NULL;

	if (is_dir[0] != FALSE && is_dir[1] != FALSE) {
		bar = nautilus_share_bar_new (_("May be used to share or receive files"));
	} else if (is_dir[0] != FALSE) {
#ifndef HAVE_BLUETOOTH
		bar = nautilus_share_bar_new (_("May be shared over the network or Bluetooth"));
#else
		bar = nautilus_share_bar_new (_("May be shared over the network"));
#endif /* !HAVE_BLUETOOTH */
	} else {
#ifdef HAVE_BLUETOOTH
		BluetoothClient *client;

		bar = nautilus_share_bar_new (_("May be used to receive files over Bluetooth"));
		gtk_widget_set_no_show_all (bar, TRUE);
		client = bluetooth_client_new ();
		g_object_set_data_full (G_OBJECT (bar), "bluetooth-client", client, g_object_unref);
		g_signal_connect (G_OBJECT (client), "notify::default-adapter-powered",
				  G_CALLBACK (default_adapter_powered_cb), bar);
		downloads_bar_set_from_bluetooth_status (bar);
#endif /* HAVE_BLUETOOTH */
	}

	g_signal_connect (bar, "response",
			  G_CALLBACK (bar_response_cb),
			  window);

	gtk_widget_show_all (bar);

	g_object_unref (file);

        return bar;
}

static void
nautilus_user_share_location_widget_provider_iface_init (NautilusLocationWidgetProviderIface *iface)
{
        iface->get_widget = nautilus_user_share_get_location_widget;
}

static void
nautilus_user_share_instance_init (NautilusUserShare *share)
{

}

static void
nautilus_user_share_class_init (NautilusUserShareClass *klass)
{

}

static GType share_type = 0;

static GType
nautilus_user_share_get_type (void)
{
        return share_type;
}

static void
nautilus_user_share_register_type (GTypeModule *module)
{
        static const GTypeInfo info = {
                sizeof (NautilusUserShareClass),
                (GBaseInitFunc) NULL,
                (GBaseFinalizeFunc) NULL,
                (GClassInitFunc) nautilus_user_share_class_init,
                NULL,
                NULL,
                sizeof (NautilusUserShare),
                0,
                (GInstanceInitFunc) nautilus_user_share_instance_init,
        };

        static const GInterfaceInfo location_widget_provider_iface_info = {
                (GInterfaceInitFunc) nautilus_user_share_location_widget_provider_iface_init,
                NULL,
                NULL
        };

        share_type = g_type_module_register_type (module,
                                                 G_TYPE_OBJECT,
                                                 "NautilusUserShare",
                                                 &info, 0);

        g_type_module_add_interface (module,
                                     share_type,
                                     NAUTILUS_TYPE_LOCATION_WIDGET_PROVIDER,
                                     &location_widget_provider_iface_info);
}

void
nautilus_module_initialize (GTypeModule *module)
{
        nautilus_user_share_register_type (module);
        bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
}

void
nautilus_module_shutdown (void)
{
}

void
nautilus_module_list_types (const GType **types,
                            int          *num_types)
{
        static GType type_list [1];

        type_list[0] = NAUTILUS_TYPE_USER_SHARE;

        *types = type_list;
        *num_types = 1;
}
