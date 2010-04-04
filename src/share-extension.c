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
#include <gtk/gtk.h>
#include <libnautilus-extension/nautilus-menu-provider.h>
#include <libnautilus-extension/nautilus-location-widget-provider.h>

#include "nautilus-share-bar.h"
#include "user_share-common.h"

#define NAUTILUS_TYPE_USER_SHARE  (nautilus_user_share_get_type ())
#define NAUTILUS_USER_SHARE(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_USER_SHARE, NautilusUserShare))
#define NAUTILUS_IS_USER_SHARE(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_USER_SHARE))

typedef struct _NautilusUserSharePrivate NautilusUserSharePrivate;

typedef struct
{
        GObject              parent_slot;
        NautilusUserSharePrivate *priv;
} NautilusUserShare;

typedef struct
{
        GObjectClass parent_slot;
} NautilusUserShareClass;

#define NAUTILUS_USER_SHARE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NAUTILUS_TYPE_USER_SHARE, NautilusUserSharePrivate))

struct _NautilusUserSharePrivate
{
        GSList       *widget_list;
};

static GType nautilus_user_share_get_type      (void);
static void  nautilus_user_share_register_type (GTypeModule *module);

static GObjectClass *parent_class;


static void
launch_process (char **argv, GtkWindow *parent)
{
        GError *error;
        GtkWidget *dialog;

        error = NULL;
        if (!g_spawn_async (NULL,
                            argv, NULL,
                            0,
                            NULL, NULL,
                            NULL,
                            &error)) {


                dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_OK, _("Unable to launch the Personal File Sharing preferences"));

                gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);

                gtk_dialog_run (GTK_DIALOG (dialog));
                gtk_widget_destroy (dialog);

                g_error_free (error);
        }
}

static void
launch_prefs_on_window (GtkWindow *window)
{
        char *argv [2];

        argv [0] = g_build_filename (BINDIR, "gnome-file-share-properties", NULL);
        argv [1] = NULL;

        launch_process (argv, window);

        g_free (argv [0]);
}

static void
bar_activated_cb (NautilusShareBar *bar,
                  gpointer         data)
{
        launch_prefs_on_window (GTK_WINDOW (data));
}

static void
destroyed_callback (GtkWidget    *widget,
                    NautilusUserShare *share)
{
        share->priv->widget_list = g_slist_remove (share->priv->widget_list, widget);
}

static void
add_widget (NautilusUserShare *share,
            GtkWidget         *widget)
{
        share->priv->widget_list = g_slist_prepend (share->priv->widget_list, widget);

        g_signal_connect (widget, "destroy",
                          G_CALLBACK (destroyed_callback),
                          share);
}

static GtkWidget *
nautilus_user_share_get_location_widget (NautilusLocationWidgetProvider *iface,
                                         const char                     *uri,
                                         GtkWidget                      *window)
{
	GFile             *file;
	GtkWidget         *bar;
	NautilusUserShare *share;
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

	share = NAUTILUS_USER_SHARE (iface);

	if (is_dir[0] != FALSE && is_dir[1] != FALSE) {
		bar = nautilus_share_bar_new (_("You can share files from this folder and receive files to it"));
	} else if (is_dir[0] != FALSE) {
		bar = nautilus_share_bar_new (_("You can share files from this folder over the network and Bluetooth"));
	} else {
		bar = nautilus_share_bar_new (_("You can receive files over Bluetooth into this folder"));
	}

	add_widget (share, nautilus_share_bar_get_button (NAUTILUS_SHARE_BAR (bar)));

	g_signal_connect (bar, "activate",
			  G_CALLBACK (bar_activated_cb),
			  window);

	gtk_widget_show (bar);

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
        share->priv = NAUTILUS_USER_SHARE_GET_PRIVATE (share);
}

static void
nautilus_user_share_finalize (GObject *object)
{
        NautilusUserShare *share;

        g_return_if_fail (object != NULL);
        g_return_if_fail (NAUTILUS_IS_USER_SHARE (object));

        share = NAUTILUS_USER_SHARE (object);

        g_return_if_fail (share->priv != NULL);

        if (share->priv->widget_list != NULL) {
                g_slist_free (share->priv->widget_list);
        }

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nautilus_user_share_class_init (NautilusUserShareClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = nautilus_user_share_finalize;

        g_type_class_add_private (klass, sizeof (NautilusUserSharePrivate));
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
