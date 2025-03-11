project(
  'gnome-user-share',
  'rust', 'c',
  version: '49.alpha',
  license: 'GPL2',
  meson_version: '>= 0.58.0',
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

cc = meson.get_compiler('c')

config_h = configuration_data()

# i18n
config_h.set_quoted('GETTEXT_PACKAGE', user_share_name)
config_h.set_quoted('GNOMELOCALEDIR', user_share_prefix / user_share_localedir)

config_h.set_quoted('HTTPD_CONFIG_TEMPLATE_PREFIX', user_share_prefix / user_share_pkgdatadir / 'dav_user_')

gio_dep = dependency('gio-2.0', version: '>= 2.74.0')
gio_unix_dep = dependency('gio-unix-2.0')
glib_dep = dependency('glib-2.0', version: '>= 2.74.0')
cargo = find_program('cargo')

# GIO schemas
gio_schemasdir = gio_dep.get_variable(
  pkgconfig: 'schemasdir',
  pkgconfig_define: ['datadir', user_share_prefix / user_share_datadir]
)

# systemd user unit directory
systemd_dep = dependency('systemd', required : false)
systemd_systemduserunitdir = get_option('systemduserunitdir')
if systemd_systemduserunitdir == ''
  if systemd_dep.found()
    systemd_systemduserunitdir = systemd_dep.get_variable(pkgconfig: 'systemduserunitdir')
  else
    systemd_systemduserunitdir = user_share_prefix / user_share_libdir / 'systemd' / 'user'
  endif
endif

httpd = find_program(get_option('httpd'), required: false)
httpd_path = ''

if httpd.found()
  httpd_version = run_command([httpd, '-v'], check: true)
  version = httpd_version.stdout().split()[2].split('/')[1]
  version_array = version.split('.')
  major = version_array[0].to_int()
  minor = version_array[1].to_int()
  req_version = '2.4.0'
  req_version_array = req_version.split('.')
  req_major = req_version_array[0].to_int()
  req_minor = req_version_array[1].to_int()

  if (major >= req_major and minor >= req_minor) or major > req_major
    httpd_path = httpd.full_path()
    message('httpd version @0@ >= @1@: YES'.format(version, req_version))
  else
    error('httpd version @0@ >= @1@: NO'.format(version, req_version))
  endif
endif

config_h.set_quoted('HTTPD_PROGRAM', httpd_path)


# Check for SELinux
libselinux_dep = dependency('libselinux', version: '>= 2.8', required: false)
have_selinux = libselinux_dep.found() and cc.has_function('is_selinux_enabled', dependencies: libselinux_dep)
if have_selinux
  clang = find_program('clang')
endif

# Check for the HTTPD modules path
modules_path = get_option('modules_path')
config_h.set_quoted('HTTPD_MODULES_PATH', modules_path)

subdir('data')
subdir('po')
subdir('src')

gnome.post_install(
  glib_compile_schemas: true,
)

#### Summary ####
summary('httpd location', httpd_path, section: 'Options')
summary('httpd modules path', modules_path, section: 'Options')

summary('Apache', '2.4', section: 'Requirements')
summary('mod_dnssd', '0.6', section: 'Requirements')

