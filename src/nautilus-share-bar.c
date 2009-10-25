/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: William Jon McCann <mccann@jhu.edu>
 *
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "nautilus-share-bar.h"

static void nautilus_share_bar_finalize   (GObject *object);

#define NAUTILUS_SHARE_BAR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NAUTILUS_TYPE_SHARE_BAR, NautilusShareBarPrivate))

struct NautilusShareBarPrivate
{
        GtkWidget   *button;
        GtkWidget   *label;
        char        *str;
};

enum {
	PROP_0,
	PROP_LABEL
};

enum {
       ACTIVATE,
       LAST_SIGNAL
};

static guint           signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (NautilusShareBar, nautilus_share_bar, GTK_TYPE_HBOX)

GtkWidget *
nautilus_share_bar_get_button (NautilusShareBar *bar)
{
        GtkWidget *button;

        g_return_val_if_fail (bar != NULL, NULL);

        button = bar->priv->button;

        return button;
}

static void
nautilus_share_bar_set_property (GObject            *object,
                                guint               prop_id,
                                const GValue       *value,
                                GParamSpec         *pspec)
{
        NautilusShareBar *self;

        self = NAUTILUS_SHARE_BAR (object);

        switch (prop_id) {
	case PROP_LABEL: {
		char *str;
		g_free (self->priv->str);
		str = g_strdup_printf ("<i>%s</i>", g_value_get_string (value));
		gtk_label_set_markup (GTK_LABEL (self->priv->label), str);
		self->priv->str = g_value_dup_string (value);
		break;
	}
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
nautilus_share_bar_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
        NautilusShareBar *self;

        self = NAUTILUS_SHARE_BAR (object);

        switch (prop_id) {
	case PROP_LABEL:
		g_value_set_string (value, self->priv->str);
		break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
nautilus_share_bar_class_init (NautilusShareBarClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize     = nautilus_share_bar_finalize;
        object_class->get_property = nautilus_share_bar_get_property;
        object_class->set_property = nautilus_share_bar_set_property;

        g_type_class_add_private (klass, sizeof (NautilusShareBarPrivate));

        g_object_class_install_property (G_OBJECT_CLASS(klass),
					 PROP_LABEL, g_param_spec_string ("label",
									  "label", "The widget's main label", NULL, G_PARAM_READWRITE));


        signals [ACTIVATE] = g_signal_new ("activate",
                                           G_TYPE_FROM_CLASS (klass),
                                           G_SIGNAL_RUN_LAST,
                                           G_STRUCT_OFFSET (NautilusShareBarClass, activate),
                                           NULL, NULL,
                                           g_cclosure_marshal_VOID__VOID,
                                           G_TYPE_NONE, 0);

}

static void
button_clicked_cb (GtkWidget       *button,
                   NautilusShareBar *bar)
{
        g_signal_emit (bar, signals [ACTIVATE], 0);
}

static void
nautilus_share_bar_init (NautilusShareBar *bar)
{
	GtkWidget   *label;
        GtkWidget   *hbox;
        GtkWidget   *vbox;
        GtkWidget   *image;
        char        *hint;

        bar->priv = NAUTILUS_SHARE_BAR_GET_PRIVATE (bar);

        hbox = GTK_WIDGET (bar);

        vbox = gtk_vbox_new (FALSE, 6);
        gtk_widget_show (vbox);
        gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

        label = gtk_label_new (_("Personal File Sharing"));
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_widget_show (label);
        gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);

        bar->priv->label = gtk_label_new ("");
        hint = g_strdup_printf ("<i>%s</i>", "");
        gtk_label_set_markup (GTK_LABEL (bar->priv->label), hint);
        gtk_misc_set_alignment (GTK_MISC (bar->priv->label), 0.0, 0.5);
        gtk_widget_show (bar->priv->label);
        gtk_box_pack_start (GTK_BOX (vbox), bar->priv->label, TRUE, TRUE, 0);

        bar->priv->button = gtk_button_new_with_label (_("Launch Preferences"));
        gtk_widget_show (bar->priv->button);
        gtk_box_pack_end (GTK_BOX (hbox), bar->priv->button, FALSE, FALSE, 0);

        image = gtk_image_new_from_icon_name ("folder-remote", GTK_ICON_SIZE_BUTTON);
        gtk_widget_show (image);
        gtk_button_set_image (GTK_BUTTON (bar->priv->button), image);

        g_signal_connect (bar->priv->button, "clicked",
                          G_CALLBACK (button_clicked_cb),
                          bar);

        gtk_widget_set_tooltip_text (bar->priv->button,
                                     _("Launch Personal File Sharing Preferences"));
}

static void
nautilus_share_bar_finalize (GObject *object)
{
        NautilusShareBar *bar;

        g_return_if_fail (object != NULL);
        g_return_if_fail (NAUTILUS_IS_SHARE_BAR (object));

        bar = NAUTILUS_SHARE_BAR (object);

        g_return_if_fail (bar->priv != NULL);

        G_OBJECT_CLASS (nautilus_share_bar_parent_class)->finalize (object);
}

GtkWidget *
nautilus_share_bar_new (const char *label)
{
        GObject *result;

        result = g_object_new (NAUTILUS_TYPE_SHARE_BAR,
			       "label", label,
                               NULL);

        return GTK_WIDGET (result);
}
