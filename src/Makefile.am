
MARSHALFILES = marshal.c marshal.h
BUILT_SOURCES = $(MARSHALFILES)

marshal.c: marshal.h
	 $(AM_V_GEN) ( $(GLIB_GENMARSHAL) --prefix=marshal $(srcdir)/marshal.list --header --body > marshal.c )
marshal.h: marshal.list
	$(AM_V_GEN) ( $(GLIB_GENMARSHAL) --prefix=marshal $(srcdir)/marshal.list --header > marshal.h )


bin_PROGRAMS= \
	gnome-file-share-properties

libexec_PROGRAMS= \
	gnome-user-share

INCLUDES=	\
	-DPREFIX=\""$(prefix)"\" 			\
	-DBINDIR=\""$(bindir)"\" 			\
	-DLIBEXECDIR=\""$(libexecdir)"\"		\
	-DUSER_SHARE_PROGRAM=\""$(libexecdir)/gnome-user-share"\" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DDATADIR=\""$(datadir)/gnome-user-share/"\"	\
	-DHTTPD_CONFIG_TEMPLATE=\""$(datadir)/gnome-user-share/dav_user_%s.conf"\"	\
	-DHTTPD_PROGRAM=\""$(HTTPD)"\"	\
	-DHTTPD_MODULES_PATH=\""$(MODULES_PATH)"\"	\
	-I$(top_srcdir) 				\
	-I$(top_builddir) 				\
	$(USER_SHARE_CFLAGS)				\
	$(USER_SHARE_CONFIG_CFLAGS)			\
	$(X_CFLAGS)

gnome_user_share_SOURCES =	\
	user_share.c		\
	user_share.h		\
	user_share-private.h	\
	user_share-private.c	\
	http.c			\
	http.h			\
	obexftp.c		\
	obexftp.h		\
	obexpush.c		\
	obexpush.h		\
	$(MARSHALFILES)

gnome_user_share_LDADD = \
	$(USER_SHARE_LIBS)	\
	$(SELINUX_LIBS)	\
	$(X_LIBS) $(X_PRE_LIBS) -lX11 $(X_EXTRA_LIBS)

gnome_file_share_properties_SOURCES =	\
	file-share-properties.c		\
	user_share-private.h		\
	user_share-private.c

gnome_file_share_properties_LDADD = \
	$(USER_SHARE_CONFIG_LIBS)


EXTRA_DIST = marshal.list

CLEANFILES = $(BUILT_SOURCES)


-include $(top_srcdir)/git.mk
