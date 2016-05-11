@INTLTOOL_DESKTOP_RULE@
applicationsdir = $(datadir)/applications/
applications_DATA = $(applications_in_files:.desktop.in=.desktop)
applications_in_files = gnome-user-share-webdav.desktop.in

appdir = $(datadir)/gnome-user-share
app_DATA=dav_user_2.0.conf dav_user_2.2.conf dav_user_2.4.conf dav_groupfile

gsettingsdir = $(datadir)/glib-2.0/schemas
gsettings_in_files = org.gnome.desktop.file-sharing.gschema.xml.in
gsettings_DATA = $(gsettings_in_files:.xml.in=.xml)
@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

systemdservicedir = $(systemduserunitdir)
systemdservice_in_files = gnome-user-share-webdav.service.in
systemdservice_DATA = gnome-user-share-webdav.service

$(systemdservice_DATA): gnome-user-share-webdav.service.in Makefile
	$(AM_V_GEN) $(SED) -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

convertdir = $(datadir)/GConf/gsettings
convert_DATA = gnome-user-share.convert

EXTRA_DIST = $(app_DATA)  \
	$(gsettings_in_files) \
	$(convert_DATA) \
	$(systemdservice_in_files) \
	gnome-user-share-webdav.desktop.in.in

CLEANFILES = $(applications_DATA) \
	$(autostart_DATA) \
	$(gsettings_DATA) \
	$(systemdservice_DATA)

DISTCLEANFILES = $(gsettings_DATA)

-include $(top_srcdir)/git.mk
