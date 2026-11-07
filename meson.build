project(
  'nawid',
  'c',
  version: '0.1.0',
  license: 'GPL-3.0-only',
  meson_version: '>=1.0',
  default_options: [
    'c_std=c17',
    'warning_level=3',
    'buildtype=debugoptimized',
  ],
)

add_project_arguments('-D_GNU_SOURCE', language: 'c')

subdir('core')
subdir('daemon')
subdir('cli')
subdir('data')
