
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

noinst_LTLIBRARIES = libuser-share-common.la
libuser_share_common_la_SOURCES = user_share-common.c user_share-common.h

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
	$(EXTENSION_CFLAGS)				\
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

gnome_user_share_LDADD =	\
	libuser-share-common.la	\
	$(USER_SHARE_LIBS)	\
	$(SELINUX_LIBS)	\
	$(X_LIBS) $(X_PRE_LIBS) -lX11 $(X_EXTRA_LIBS)

gnome_file_share_properties_SOURCES =	\
	file-share-properties.c		\
	user_share-private.h		\
	user_share-private.c

gnome_file_share_properties_LDADD = \
	$(USER_SHARE_CONFIG_LIBS)

nautilus_extensiondir = $(NAUTILUSDIR)
nautilus_extension_LTLIBRARIES = libnautilus-share-extension.la

libnautilus_share_extension_la_SOURCES =	\
	nautilus-share-bar.c			\
	nautilus-share-bar.h			\
	share-extension.c			\
	$(NULL)

libnautilus_share_extension_la_LIBADD = libuser-share-common.la $(EXTENSION_LIBS)
libnautilus_share_extension_la_LDFLAGS = -avoid-version -module -no-undefined

EXTRA_DIST = marshal.list

CLEANFILES = $(BUILT_SOURCES)


-include $(top_srcdir)/git.mk
