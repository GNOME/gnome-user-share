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

if have_selinux
  deps += libselinux_dep
endif

executable(
  'gnome-user-share-webdav',
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
  install: true,
  install_dir: user_share_libexecdir,
)

deps = [
  gio_unix_dep,
  libuser_share_common_dep,
]
