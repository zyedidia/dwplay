project('quickjs', 'c',
  version: '0.1.0',
  default_options: ['c_std=c11'])

quickjs_sources = files(
  'quickjs.c',
  'libunicode.c', 
  'libregexp.c',
  'cutils.c',
  'dtoa.c',
)

quickjs_lib = static_library('quickjs',
  quickjs_sources,
  c_args: ['-DCONFIG_VERSION="0.1.0"', '-DCONFIG_BIGNUM', '-D_GNU_SOURCE'],
  include_directories: include_directories('.'),
)

quickjs_dep = declare_dependency(
  link_with: quickjs_lib,
  include_directories: include_directories('.'),
)
