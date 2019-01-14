applicationsdir = $(datadir)/applications/
applications_DATA = $(applications_in_files:.desktop.in=.desktop)
applications_in_files = gnome-user-share-webdav.desktop.in
$(applications_DATA): $(applications_in_files)
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

gnome-user-share-webdav.desktop.in: gnome-user-share-webdav.desktop.in.in Makefile
	$(AM_V_GEN) $(SED) -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

appdir = $(datadir)/gnome-user-share
app_DATA=dav_user_2.0.conf dav_user_2.2.conf dav_user_2.4.conf dav_groupfile

gsettings_SCHEMAS = org.gnome.desktop.file-sharing.gschema.xml
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
	$(systemdservice_DATA) \
	$(applications_in_files)

-include $(top_srcdir)/git.mk
