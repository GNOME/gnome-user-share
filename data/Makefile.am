SUBDIRS = icons

@INTLTOOL_DESKTOP_RULE@
autostartdir = $(sysconfdir)/xdg/autostart/
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)
autostart_in_files = gnome-user-share-obexftp.desktop.in gnome-user-share-obexpush.desktop.in gnome-user-share-webdav.desktop.in

appdir = $(datadir)/gnome-user-share
app_DATA=dav_user_2.0.conf dav_user_2.2.conf dav_user_2.4.conf dav_groupfile

gsettingsdir = $(datadir)/glib-2.0/schemas
gsettings_in_files = org.gnome.desktop.file-sharing.gschema.xml.in
gsettings_DATA = $(gsettings_in_files:.xml.in=.xml)
@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

convertdir = $(datadir)/GConf/gsettings
convert_DATA = gnome-user-share.convert

EXTRA_DIST = $(app_DATA)  \
	$(gsettings_in_files) \
	$(convert_DATA) \
	gnome-user-share-obexftp.desktop.in.in \
	gnome-user-share-obexpush.desktop.in.in \
	gnome-user-share-webdav.desktop.in.in

CLEANFILES = $(autostart_DATA) $(gsettings_DATA)

DISTCLEANFILES = $(gsettings_DATA)

-include $(top_srcdir)/git.mk
