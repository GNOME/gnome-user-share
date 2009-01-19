SUBDIRS = icons

@INTLTOOL_SCHEMAS_RULE@
schemadir = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = desktop_gnome_file_sharing.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)
	
@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
desktop_in_files = gnome-user-share-properties.desktop.in

autostartdir = $(sysconfdir)/xdg/autostart/
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)
autostart_in_files = gnome-user-share.desktop.in

appdir = $(datadir)/gnome-user-share
app_DATA=dav_user_2.0.conf dav_groupfile file-share-properties.ui

appconfdir = $(datadir)/gnome-user-share
appconf_DATA = dav_user_2.2.conf

dav_user_2.2.conf: dav_user_2.2.conf.in
	sed "s,@DATADIR@,$(datadir)," dav_user_2.2.conf.in | \
	sed "s,@HTTP_MODULES_PATH@,$(MODULES_PATH)," > $@

install-data-local:
if GCONF_SCHEMAS_INSTALL
	-GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	$(GCONFTOOL) --makefile-install-rule $(schema_DATA)
endif
	ln -sf dav_user_$(HTTPD_VERSION).conf $(DESTDIR)$(appdir)/dav_user.conf

if GCONF_SCHEMAS_INSTALL
uninstall-local:
	-GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	$(GCONFTOOL) --makefile-uninstall-rule $(schema_DATA)
endif

EXTRA_DIST = $(schema_in_files) \
	$(app_DATA)		\
	$(desktop_in_files)	\
	dav_user_2.2.conf.in	\
	gnome-user-share.desktop.in.in \
	file-share-properties.ui

CLEANFILES = $(desktop_DATA) $(autostart_DATA) file-share-properties.ui dav_user_2.2.conf

DISTCLEANFILES = $(schema_DATA) dav_user_2.2.conf
