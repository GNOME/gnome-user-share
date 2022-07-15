common_c_flags = ['-DGNOMELOCALEDIR="@0@"'.format(user_share_prefix / user_share_localedir)]

gnome = import('gnome')

deps = [
  gio_dep,
  glib_dep,
]

libuser_share_common = static_library(
  'user-share-common',
  sources: 'user_share-common.c',
  dependencies: deps,
)

libuser_share_common_dep = declare_dependency(
  include_directories: '.',
  dependencies: deps,
  link_with: libuser_share_common,
)

sources = files(
  'http.c',
  'user_share-webdav.c',
)

deps = sockets_deps + [
  libuser_share_common_dep,
]

c_flags = common_c_flags + [
  '-DHTTPD_CONFIG_TEMPLATE="@0@"'.format(user_share_prefix / user_share_pkgdatadir / 'dav_user_%s.conf'),
  '-DHTTPD_PROGRAM="@0@"'.format(httpd_path),
  '-DHTTPD_MODULES_PATH="@0@"'.format(modules_path),
]

if have_selinux
  deps += libselinux_dep
endif

executable(
  'gnome-user-share-webdav',
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: c_flags,
  install: true,
  install_dir: user_share_libexecdir,
)

sources = files(
  'nautilus-share-bar.c',
  'share-extension.c',
)

deps = [
  gtk_dep,
  gio_unix_dep,
  libnautilus_extension_dep,
  libuser_share_common_dep,
]

shared_module(
  'nautilus-share-extension',
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: common_c_flags,
  install: true,
  install_dir: libnautilus_extension_extensiondir,
)
