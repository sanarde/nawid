executable(
  'nawi',
  'src/main.c',
  include_directories: nawid_core_inc,
  link_with: nawid_core,
  dependencies: [dependency('dbus-1'), dependency('libcfgd'), m_dep],
  install: true,
)
