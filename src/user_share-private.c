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
 *  Bastien Nocera <hadess@hadess.net>
 *
 */

#include "config.h"

#include <string.h>

#include "user_share-private.h"

static char *password_setting_strings[] = {
    "never",
    "on_write",
    "always"
};

const char *
password_string_from_setting (PasswordSetting setting)
{
    
    if (setting >= 0 && setting <= PASSWORD_ALWAYS)
	return password_setting_strings[setting];
    
    /* Fallback on secure pref */
    return password_setting_strings[PASSWORD_ALWAYS];
}

PasswordSetting
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
