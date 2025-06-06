gnome = import('gnome')

configure_file(
  input: 'config.rs.in',
  output: 'config.rs',
  configuration: config_h,
)

# Copy the config.rs output to the source directory.
run_command(
  'cp',
  meson.project_build_root() / 'src' / 'config.rs',
  meson.project_source_root() / 'src' / 'config.rs',
  check: true
)

cargo_options = [ '--manifest-path', meson.project_source_root() / 'Cargo.toml' ]
cargo_options += [ '--target-dir', meson.project_build_root() / 'src' ]

if get_option('buildtype') == 'release'
  cargo_options += [ '--release' ]
  rust_target = 'release'
  message('Building in release mode')
else
  rust_target = 'debug'
  message('Building in debug mode')
endif

if have_selinux
  cargo_options += ['--features', 'selinux']
endif

cargo_env = [
]

custom_target(
  'cargo-build',
  build_by_default: true,
  build_always_stale: true,
  output: 'gnome-user-share-webdav',
  console: true,
  install: true,
  install_dir: user_share_libexecdir,
  command: [
    'env',
    cargo_env,
    cargo, 'build',
    cargo_options,
    '&&',
    'cp', 'src' / rust_target / meson.project_name(), '@OUTPUT@',
  ]
)
