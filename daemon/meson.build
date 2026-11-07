fs = import('fs')

dbus_dep = dependency('dbus-1')
cfgd_dep = dependency('libcfgd')
wayland_dep = dependency('wayland-client')
xcb_dep = dependency('xcb')
xcb_randr_dep = dependency('xcb-randr')

protocol_xml = meson.project_source_root() / 'protocols/wlr-output-management-unstable-v1.xml'
protocol_name = fs.stem(protocol_xml)

protocol_sources = [
  custom_target(
    protocol_name + '-code',
    input: protocol_xml,
    output: protocol_name + '-protocol.c',
    command: [find_program('wayland-scanner'), 'private-code', '@INPUT@', '@OUTPUT@'],
  ),
  custom_target(
    protocol_name + '-header',
    input: protocol_xml,
    output: protocol_name + '-client-protocol.h',
    command: [find_program('wayland-scanner'), 'client-header', '@INPUT@', '@OUTPUT@'],
  ),
]

nawid_deps = [dbus_dep, cfgd_dep, wayland_dep, xcb_dep, xcb_randr_dep, m_dep]

nawid_src_inc = include_directories('src')

nawid_daemon = static_library(
  'nawid-daemon',
  'src/app.c',
  'src/bus.c',
  'src/settings.c',
  'src/wayland.c',
  'src/x11.c',
  protocol_sources,
  include_directories: [nawid_src_inc, nawid_core_inc],
  dependencies: nawid_deps,
)

executable(
  'nawid',
  'src/main.c',
  include_directories: [nawid_src_inc, nawid_core_inc],
  link_with: [nawid_daemon, nawid_core],
  dependencies: nawid_deps,
  install: true,
)
