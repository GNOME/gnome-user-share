#ifndef MD5_H
#define MD5_H

#include <glib.h>

struct GnomeUserShareMD5Context {
	guint32 buf[4];
	guint32 bits[2];
	unsigned char in[64];
};

char *gnome_user_share_md5_digest_to_ascii (unsigned char                  digest[16]);
void  gnome_user_share_md5_string          (const char                    *string,
					    unsigned char                  digest[16]);
void  gnome_user_share_md5_init            (struct GnomeUserShareMD5Context *ctx);
void  gnome_user_share_md5_update          (struct GnomeUserShareMD5Context *ctx,
					    unsigned char const           *buf,
					    unsigned                       len);
void  gnome_user_share_md5_final           (unsigned char                  digest[16],
					    struct GnomeUserShareMD5Context *ctx);

#endif /* MD5_h */
