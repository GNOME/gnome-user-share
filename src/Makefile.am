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
	$(X_CFLAGS)					\
	$(WARN_CFLAGS)

gnome_user_share_SOURCES =	\
	user_share.c		\
	user_share.h		\
	user_share-private.h	\
	user_share-private.c	\
	http.c			\
	http.h			\
	$(NULL)

if USE_BLUETOOTH
gnome_user_share_SOURCES +=	\
	obexpush.c		\
	obexpush.h
else
EXTRA_DIST = obexpush.c obexpush.h
endif

gnome_user_share_LDADD =	\
	libuser-share-common.la	\
	$(USER_SHARE_LIBS)	\
	$(SELINUX_LIBS)	\
	$(X_LIBS) $(X_PRE_LIBS) -lX11 $(X_EXTRA_LIBS)

nautilus_extensiondir = $(NAUTILUSDIR)
nautilus_extension_LTLIBRARIES = libnautilus-share-extension.la

libnautilus_share_extension_la_SOURCES =	\
	nautilus-share-bar.c			\
	nautilus-share-bar.h			\
	share-extension.c			\
	$(NULL)

libnautilus_share_extension_la_LIBADD = libuser-share-common.la $(EXTENSION_LIBS)
libnautilus_share_extension_la_LDFLAGS = -avoid-version -module -no-undefined

-include $(top_srcdir)/git.mk
