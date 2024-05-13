/*
 *  Copyright (C) 2004-2009 Red Hat, Inc.
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
 *  Bastien Nocera <hadess@hadess.net>
 *
 */

#include <string.h>

#include "user_share-common.h"

static char *
lookup_special_dir (GUserDirectory directory,
		    const char *name,
		    gboolean create_dir)
{
	const char *special_dir;
	char *dir;

	special_dir = g_get_user_special_dir (directory);
	if (special_dir != NULL && strcmp (special_dir, g_get_home_dir ()) != 0) {
		if (create_dir != FALSE)
			g_mkdir_with_parents (special_dir, 0755);
		return g_strdup (special_dir);
	}

	dir = g_build_filename (g_get_home_dir (), name, NULL);
	if (create_dir != FALSE)
		g_mkdir_with_parents (dir, 0755);
	return dir;
}

char *
lookup_public_dir (void)
{
	return lookup_special_dir (G_USER_DIRECTORY_PUBLIC_SHARE,
				   "Public",
				   TRUE);
}
