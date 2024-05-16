desktop = 'gnome-user-share-webdav.desktop'

desktop_in = configure_file(
  input: desktop + '.in.in',
  output: '@BASENAME@',
  configuration: {'libexecdir': user_share_prefix / user_share_libexecdir},
)

i18n.merge_file(
  type: 'desktop',
  input: desktop_in,
  output: '@BASENAME@',
  po_dir: po_dir,
  install: true,
  install_dir: user_share_datadir / 'applications',
)

configure_file(
  input: 'gnome-user-share-webdav.service.in',
  output: '@BASENAME@',
  configuration: {'libexecdir': user_share_prefix / user_share_libexecdir},
  install: true,
  install_dir: systemd_systemduserunitdir,
)

app_data = files(
  'dav_user_2.4.conf',
  'dav_groupfile',
)

install_data(
  app_data,
  install_dir: user_share_pkgdatadir,
)

install_data(
  'org.gnome.desktop.file-sharing.gschema.xml',
  install_dir: gio_schemasdir,
)

install_data(
  'gnome-user-share.convert',
  install_dir: user_share_datadir / 'GConf/gsettings',
)
