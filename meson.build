project(
  'gnome-user-share', 'c',
  version: '43.0',
  license: 'GPL2',
  default_options: 'buildtype=debugoptimized',
  meson_version: '>= 0.57.0',
)

user_share_name = meson.project_name()

user_share_prefix = get_option('prefix')
user_share_bindir = get_option('bindir')
user_share_datadir = get_option('datadir')
user_share_libdir = get_option('libdir')
user_share_libexecdir = get_option('libexecdir')
user_share_localedir = get_option('localedir')

user_share_pkgdatadir = user_share_datadir / user_share_name

i18n = import('i18n')

source_root = meson.current_source_dir()

po_dir = source_root / 'po'

top_inc = include_directories('.')

cc = meson.get_compiler('c')

config_h = configuration_data()

# i18n
config_h.set_quoted('GETTEXT_PACKAGE', user_share_name)

# compiler flags
common_flags = []
if get_option('buildtype').contains('debug')
  common_flags += cc.get_supported_arguments([
    '-Wmissing-declarations',
    '-Wmissing-prototypes',
    '-Wcast-align',
    '-Wnested-externs',
    '-Wstrict-prototypes',
    '-Werror=format=2',
    '-Werror=implicit-function-declaration',
    '-Werror=init-self',
    '-Werror=missing-include-dirs',
    '-Werror=missing-prototypes',
    '-Werror=pointer-arith',
    '-Werror=return-type',
    '-Wno-sign-compare',
    '-Wno-strict-aliasing',
  ])
endif

add_project_arguments(common_flags, language: 'c')

gio_dep = dependency('gio-2.0', version: '>= 2.71.3')
gio_unix_dep = dependency('gio-unix-2.0')
glib_dep = dependency('glib-2.0', version: '>= 2.71.3')

# GIO schemas
gio_schemasdir = gio_dep.get_variable(
  pkgconfig: 'schemasdir',
  pkgconfig_define: ['datadir', user_share_prefix / user_share_datadir]
)

# systemd user unit directory
systemd_systemduserunitdir = get_option('systemduserunitdir')
if systemd_systemduserunitdir == ''
  systemd_systemduserunitdir = dependency('systemd').get_variable(pkgconfig: 'systemduserunitdir')
endif

httpd = find_program(get_option('httpd'), required: false)
httpd_path = (httpd.found() ? httpd.full_path() : '')

sockets_deps = []
if not cc.has_function('socket')
  # socket is not in the default libraries.  See if it's in some other.
  foreach lib: ['bsd', 'socket', 'inet']
    socket_dep = cc.find_library(
      lib,
      has_function: 'socket',
      required: false,
    )

    if socket_dep.found()
      sockets_deps += socket_dep
      break
    endif
  endforeach
endif

# Check for SELinux
libselinux_dep = dependency('libselinux', required: false)
have_selinux = libselinux_dep.found() and cc.has_function('is_selinux_enabled', dependencies: libselinux_dep)
config_h.set('HAVE_SELINUX', have_selinux)

# Check for the HTTPD modules path
modules_path = get_option('modules_path')

subdir('data')
subdir('po')
subdir('src')

configure_file(
  output: 'config.h',
  configuration: config_h,
)

gnome.post_install(
  glib_compile_schemas: true,
)

output = user_share_name + ' was configured with the following options:\n'
output += '** httpd location: ' + httpd_path + '\n'
output += '** httpd modules_path: ' + modules_path + '\n'
output += 'Do note Apache 2.2 and mod_dnssd 0.6 are required to use ' + user_share_name + '.'
message(output)
