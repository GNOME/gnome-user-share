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

#include <string.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>

#include "md5.h"

#define FILE_SHARING_DIR "/desktop/gnome/file_sharing"
#define FILE_SHARING_ENABLED "/desktop/gnome/file_sharing/enabled"
#define FILE_SHARING_REQUIRE_PASSWORD "/desktop/gnome/file_sharing/require_password"

#define REALM "Please log in as the user guest"
#define USER "guest"

static GladeXML *ui;

typedef enum {
    PASSWORD_NEVER,
    PASSWORD_ON_WRITE,
    PASSWORD_ALWAYS
} PasswordSetting;

static char *password_setting_strings[] = {
    "never",
    "on_write",
    "always"
};

static const char *
password_string_from_setting (PasswordSetting setting)
{
    
    if (setting >= 0 && setting <= PASSWORD_ALWAYS)
	return password_setting_strings[setting];
    
    /* Fallback on secure pref */
    return "always";
}

static PasswordSetting
password_setting_from_string (const char *str)
{
    if (str != NULL) {
	if (strcmp (str, "never") == 0) {
	    return PASSWORD_NEVER;
	}
	if (strcmp (str, "always") == 0) {
	    return PASSWORD_ALWAYS;
	}
	if (strcmp (str, "on_write") == 0) {
	    return PASSWORD_ON_WRITE;
	}
    }
	
    /* Fallback on secure pref */
    return PASSWORD_ALWAYS;
}

static void
write_out_password (const char *password)
{
    char *to_hash;
    unsigned char digest[16];
    char *ascii_digest;
    char *line;
    char *filename;
    FILE *file;

    to_hash = g_strdup_printf ("%s:%s:%s", USER, REALM, password);
    gnome_user_share_md5_string (to_hash, digest);
    g_free (to_hash);
    
    ascii_digest = gnome_user_share_md5_digest_to_ascii (digest);
    line = g_strdup_printf ("%s:%s:%s\n", USER, REALM, ascii_digest);
    g_free (ascii_digest);

    filename = g_build_filename (g_get_home_dir (), ".gnome2/user-share/passwd", NULL);

    file = fopen (filename, "w");
    if (file != NULL) {
	fwrite (line, strlen (line), 1, file);
	fclose (file);
    }

    g_free (filename);
    g_free (line);
    
}

static void
flush_password (void)
{
    GtkWidget *password_entry;
    const char *password;

    password_entry = glade_xml_get_widget (ui, "password_entry");

    if (g_object_get_data (G_OBJECT( password_entry), "user_edited")) {
	password = gtk_entry_get_text (GTK_ENTRY (password_entry));
	if (password != NULL && password[0] != 0) 
	    write_out_password (password);
    }
}


static void
update_ui (void)
{
    GConfClient *client;
    gboolean enabled;
    char *str;
    PasswordSetting password_setting;
    GtkWidget *check;
    GtkWidget *password_combo;
    GtkWidget *password_entry;

    client = gconf_client_get_default ();

    enabled = gconf_client_get_bool (client,
				     FILE_SHARING_ENABLED,
				     NULL);
    str = gconf_client_get_string (client, FILE_SHARING_REQUIRE_PASSWORD, NULL);
    password_setting = password_setting_from_string (str);
    g_free (str);
    
    check = glade_xml_get_widget (ui, "enable_check");
    password_combo = glade_xml_get_widget (ui, "password_combo");
    password_entry = glade_xml_get_widget (ui, "password_entry");
    
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
				  enabled);
    gtk_widget_set_sensitive (password_combo, enabled);
    gtk_widget_set_sensitive (password_entry, enabled);

    gtk_combo_box_set_active (GTK_COMBO_BOX (password_combo),
			      password_setting);
    
    g_object_unref (client);
}

static void
file_sharing_enabled_changed (GConfClient* client,
			      guint cnxn_id,
			      GConfEntry *entry,
			      gpointer data)
{
    update_ui ();
}

static void
password_required_changed (GConfClient* client,
			   guint cnxn_id,
			   GConfEntry *entry,
			   gpointer data)
{
    update_ui ();
}


static void
password_combo_changed (GtkComboBox *combo_box)
{
    GConfClient *client;
    guint setting;

    setting = gtk_combo_box_get_active (combo_box);
    
    client = gconf_client_get_default ();

    gconf_client_set_string (client,
			     FILE_SHARING_REQUIRE_PASSWORD,
			     password_string_from_setting (setting),
			     NULL);
    g_object_unref (client);
}

static void
enable_check_toggled (GtkWidget *check)
{
    GConfClient *client;
    gboolean enabled;
    char *argv[2];
    int i;

    enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check));
    
    client = gconf_client_get_default ();

    gconf_client_set_bool (client,
			   FILE_SHARING_ENABLED,
			   enabled,
			   NULL);
    
    g_object_unref (client);

    if (enabled) {
	i = 0;
	argv[i++] = USER_SHARE_PROGRAM;
	argv[i++] = NULL;

	if (!g_spawn_async (NULL,
			    argv,
			    NULL,
			    0, /* G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL */
			    NULL,
			    NULL,
			    NULL,
			    NULL)) {
	    g_warning ("Unable to start gnome-user-share program");
	}
    }
}

static void
password_entry_changed (GtkEditable *editable)
{
    g_object_set_data (G_OBJECT (editable),
		       "user_edited", GINT_TO_POINTER (1));
    flush_password ();
}


int
main (int argc, char *argv[])
{
    GConfClient *client;
    GtkWidget *check;
    GtkWidget *password_combo;
    GtkWidget *password_entry;
    GtkListStore *store;
    GtkCellRenderer *cell;
    GtkTreeIter iter;
    
    gtk_init (&argc, &argv);

    ui = glade_xml_new (DATADIR"file-share-properties.glade",
			NULL,
			PACKAGE);

    client = gconf_client_get_default ();
    gconf_client_add_dir (client,
			  FILE_SHARING_DIR,
			  GCONF_CLIENT_PRELOAD_RECURSIVE,
			  NULL);

    check = glade_xml_get_widget (ui, "enable_check");
    password_combo = glade_xml_get_widget (ui, "password_combo");
    password_entry = glade_xml_get_widget (ui, "password_entry");

    store = gtk_list_store_new (1, G_TYPE_STRING);
    gtk_combo_box_set_model (GTK_COMBO_BOX (password_combo),
			     GTK_TREE_MODEL (store));
    cell = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (password_combo), cell, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (password_combo), cell,
				    "text", 0,
				    NULL);

    /* Keep in same order as enum */
    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter, 0,
			_("Never"), -1);
    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter, 0,
			_("When writing files"), -1);
    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter, 0,
			_("Always"), -1);
    g_object_unref (store);

    /* We can't read the password from the text, just set it to something */
    gtk_entry_set_text (GTK_ENTRY (password_entry), "none");
    g_object_set_data (G_OBJECT (password_entry),
		       "user_edited", GINT_TO_POINTER (0));
    g_signal_connect (password_entry,
		      "changed", G_CALLBACK (password_entry_changed), NULL);

    update_ui ();

    g_signal_connect (check,
		      "toggled", G_CALLBACK (enable_check_toggled), NULL);
    g_signal_connect (password_combo,
		      "changed", G_CALLBACK (password_combo_changed), NULL);
    g_signal_connect (glade_xml_get_widget (ui, "close_button"),
		      "clicked", G_CALLBACK (gtk_main_quit), NULL);

    
    gconf_client_notify_add (client,
			     FILE_SHARING_ENABLED,
			     file_sharing_enabled_changed,
			     NULL,
			     NULL,
			     NULL);
    gconf_client_notify_add (client,
			     FILE_SHARING_REQUIRE_PASSWORD,
			     password_required_changed,
			     NULL,
			     NULL,
			     NULL);

    g_object_unref (client);

    gtk_widget_show (glade_xml_get_widget (ui, "user_share_dialog"));
    
    gtk_main ();

    return 0;
}
