check_inc = include_directories('.')

foreach name : ['profile', 'spec']
  test_exe = executable(
    'test_' + name,
    'test_' + name + '.c',
    include_directories: [nawid_core_inc, check_inc],
    link_with: nawid_core,
    dependencies: m_dep,
  )
  test('core-' + name, test_exe)
endforeach
