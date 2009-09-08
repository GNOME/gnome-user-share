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

#ifndef __NAUTILUS_SHARE_BAR_H
#define __NAUTILUS_SHARE_BAR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_SHARE_BAR         (nautilus_share_bar_get_type ())
#define NAUTILUS_SHARE_BAR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_SHARE_BAR, NautilusShareBar))
#define NAUTILUS_SHARE_BAR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_SHARE_BAR, NautilusShareBarClass))
#define NAUTILUS_IS_SHARE_BAR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_SHARE_BAR))
#define NAUTILUS_IS_SHARE_BAR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_SHARE_BAR))
#define NAUTILUS_SHARE_BAR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_SHARE_BAR, NautilusShareBarClass))

typedef struct NautilusShareBarPrivate NautilusShareBarPrivate;

typedef struct
{
        GtkHBox                 box;

        NautilusShareBarPrivate *priv;
} NautilusShareBar;

typedef struct
{
        GtkHBoxClass            parent_class;

	void (* activate) (NautilusShareBar *bar);

} NautilusShareBarClass;

GType       nautilus_share_bar_get_type          (void);
GtkWidget  *nautilus_share_bar_new               (const char *label);

GtkWidget  *nautilus_share_bar_get_button        (NautilusShareBar *bar);

G_END_DECLS

#endif /* __GS_SHARE_BAR_H */
