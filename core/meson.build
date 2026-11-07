m_dep = meson.get_compiler('c').find_library('m')

nawid_core_inc = include_directories('src')

nawid_core = static_library(
  'nawid-core',
  'src/profile.c',
  'src/spec.c',
  include_directories: nawid_core_inc,
  gnu_symbol_visibility: 'hidden',
  pic: true,
  dependencies: m_dep,
)

subdir('tests')
