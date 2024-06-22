project(
  'nebula',
  ['c'],
  version: '0.1',
  default_options: ['warning_level=3', 'c_std=c23'],
)
sdl_dep = dependency('SDL2')
vulkan_dep = dependency('vulkan')
cglm_dep = dependency('cglm')

unity_subproject = subproject('Unity')
unity_dependency = unity_subproject.get_variable('unity_dep')
unity_gen_runner = unity_subproject.get_variable('gen_test_runner')

src = files(
  'src' / 'main.c',
  'src' / 'app.c'
)

inc = include_directories('src')

executable(
  'nebula',
  [src, 'src' / 'main.c'],
  dependencies: [sdl_dep,vulkan_dep,cglm_dep],
  include_directories: inc,
  install: false,
)

