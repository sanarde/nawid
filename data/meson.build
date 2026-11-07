bindir = get_option('prefix') / get_option('bindir')

conf = configuration_data()
conf.set('bindir', bindir)

configure_file(
  input: 'org.nuros.sanar.Nawi.service.in',
  output: 'org.nuros.sanar.Nawi.service',
  configuration: conf,
  install_dir: get_option('datadir') / 'dbus-1' / 'services',
)

install_data(
  'org.nuros.sanar.nawi.schema',
  install_dir: get_option('datadir') / 'cfgd' / 'schemas',
)

install_data(
  'nawid.desktop',
  install_dir: get_option('sysconfdir') / 'xdg' / 'autostart',
)
