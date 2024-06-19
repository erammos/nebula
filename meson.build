project(
  'nebula',
  ['c'],
  version: '0.1',
  default_options: ['warning_level=3', 'c_std=gnu2x'],
)
cmake = import('cmake')
sdl_dep = dependency('SDL2')
vulkan_dep = dependency('vulkan')
cglm_dep = dependency('cglm')

unity_subproject = subproject('Unity')
unity_dependency = unity_subproject.get_variable('unity_dep')
unity_gen_runner = unity_subproject.get_variable('gen_test_runner')

src = files(
  'src' / 'main.c',
)

#test1 = files(
#  'test' / 'test_sdl.c',
#  )
#  test2 = files(
#  'test' / 'test_entries.c',
#  )
inc = include_directories('src')

executable(
  'nebula',
  [src, 'src' / 'main.c'],
  dependencies: [sdl_dep,vulkan_dep,cglm_dep],
  # c_args: '-DVLT_COMPILATION',
  include_directories: inc,
  install: false,
)

#test1 = executable(
#  'test1',
#  [src, test1, unity_gen_runner.process( 'test' / 'test_encryption.c')],
#  dependencies: [openslldeps, unity_dependency],
#  c_args: '-DVLT_COMPILATION',
#  include_directories: inc,
#)

#test('test1',test1)
