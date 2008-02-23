schemadir   = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = \
	desktop_gnome_file_sharing.schemas.in

schema_DATA = $(schema_in_files:.schemas.in=.schemas)

SUBDIRS = icons po

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
desktop_in_files = gnome-user-share-properties.desktop.in

autostartdir = $(datadir)/gnome/autostart/
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)
autostart_in_files = gnome-user-share.desktop.in

MARSHALFILES = marshal.c marshal.h
BUILT_SOURCES = $(MARSHALFILES)

marshal.c: marshal.h
	 ( $(GLIB_GENMARSHAL) --prefix=marshal $(srcdir)/marshal.list --header --body > marshal.c )
marshal.h: marshal.list
	( $(GLIB_GENMARSHAL) --prefix=marshal $(srcdir)/marshal.list --header > marshal.h )


#@INTLTOOL_SCHEMAS_RULE@
%.schemas:   %.schemas.in
	cp $< $@

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
	-DHTTPD_CONFIG=\""$(datadir)/gnome-user-share/dav_user.conf"\"	\
	-DHTTPD_PROGRAM=\""$(HTTPD)"\"	\
	-I$(top_srcdir) 				\
	-I$(top_builddir) 				\
	$(AVAHI_CFLAGS)					\
	$(HOWL_CFLAGS)					\
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
	$(HOWL_LIBS)	\
	$(AVAHI_LIBS)	\
	$(USER_SHARE_LIBS)	\
	$(SELINUX_LIBS)	\
	$(X_LIBS) $(X_PRE_LIBS) -lX11 $(X_EXTRA_LIBS)

gnome_file_share_properties_SOURCES =	\
	file-share-properties.c		\
	user_share-private.h		\
	user_share-private.c

gnome_file_share_properties_LDADD = \
	$(USER_SHARE_CONFIG_LIBS)

appdir = $(datadir)/gnome-user-share
app_DATA=dav_user_2.0.conf dav_user_2.2.conf dav_groupfile file-share-properties.glade

# don't do this if we are building in eg. rpm
if GCONF_SCHEMAS_INSTALL
install-data-local: $(schema_DATA)
	if test -z "$(DESTDIR)" ; then \
		for p in $(schema_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p; \
		done \
	fi
else
install-data-local:
endif
	ln -sf dav_user_$(HTTPD_VERSION).conf $(DESTDIR)$(appdir)/dav_user.conf

EXTRA_DIST = $(schema_in_files) \
	$(app_DATA)		\
	$(desktop_in_files)	\
	gnome-user-share.spec 	\
	intltool-extract.in 	\
	intltool-merge.in 	\
	intltool-update.in	\
	gnome-user-share.desktop.in.in

CLEANFILES = $(desktop_DATA) $(autostart_DATA) $(BUILT_SOURCES)

DISTCLEANFILES = $(schema_DATA) \
	intltool-extract 	\
	intltool-merge 		\
	intltool-update
