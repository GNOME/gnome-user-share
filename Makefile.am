schemadir   = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = \
	desktop_gnome_file_sharing.schemas.in

schema_DATA = $(schema_in_files:.schemas.in=.schemas)

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
	$(USER_SHARE_CFLAGS)				\
	$(USER_SHARE_CONFIG_CFLAGS)			\
	$(X_CFLAGS)

gnome_user_share_SOURCES = \
	user_share.c

gnome_user_share_LDADD = \
	$(USER_SHARE_LIBS)	\
	$(X_LIBS) $(X_PRE_LIBS) -lX11 $(X_EXTRA_LIBS)

gnome_file_share_properties_SOURCES = \
	file-share-properties.c md5.c md5.h

gnome_file_share_properties_LDADD = \
	$(USER_SHARE_CONFIG_LIBS)

appdir = $(datadir)/gnome-user-share
app_DATA=dav_user.conf file-share-properties.glade

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

EXTRA_DIST = $(schema_in_files) \
	$(app_DATA)		\
	gnome-user-share.spec 		

DISTCLEANFILES = $(schema_DATA)
