project(
  'dwplay',
  ['c'],
  default_options: ['warning_level=1', 'buildtype=release']
)

cc = meson.get_compiler('c')
quickjs_dep = subproject('quickjs').get_variable('quickjs_dep')
sdl2_dep = dependency('sdl2')
m_dep = cc.find_library('m')

srcs = files(
  'src/dwplay.c',
  'src/gfx_sdl.c',
)

executable('dwplay',
  srcs,
  dependencies: [quickjs_dep, sdl2_dep, m_dep],
)

