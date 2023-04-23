project(
    'SRM',
    'c','cpp', 
    version : '0.1.0',
    meson_version: '>= 0.56.0',
	  default_options: [
		'warning_level=2',
		'buildtype=debug',
		'cpp_std=c++11',
	]
)

LIBRARY_PATH = '/usr/lib'
INCLUDES_PATH = '/usr/include/SRM'
EXAMPLES_PATH = '/usr/bin'
 
cpp = meson.get_compiler('cpp')

include_paths = [
    include_directories('./src/lib')
]

include_paths_sys = [
    '/usr/local/include',
    '/usr/include/drm',
    '/usr/include/libdrm',
]

foreach p : include_paths_sys
    if run_command('[', '-d', p, ']').returncode() == 0
      include_paths += [include_directories(p)]
    endif
endforeach

library_paths_all = [
    '/lib',
    '/lib64',
    '/usr/lib',
    '/usr/lib64',
    '/usr/local/lib',
    '/usr/local/lib64',

    # Multiarch support
    '/lib/x86_64-linux-gnu',
    '/usr/lib/x86_64-linux-gnu',
    '/usr/local/lib/x86_64-linux-gnu',
]

library_paths = []

foreach p : library_paths_all
    if run_command('[', '-d', p, ']').returncode() == 0
      library_paths += p
    endif
endforeach

# All headers
headers = run_command('find', './src/lib', '-type', 'f', '-name', '*.h').stdout().strip().split('\n')

# Public and private API headers
headers_classes = run_command('find', './src/lib', '-type', 'f', '-name', '*.h', '-maxdepth', '1').stdout().strip().split('\n')
headers_classes_private = run_command('find', './src/lib/private', '-type', 'f', '-name', '*.h', '-maxdepth', '1').stdout().strip().split('\n')
# headers_other = run_command('find', './lib/other', '-type', 'f', '-name', '*.h', '-maxdepth', '1').stdout().strip().split('\n')
install_headers(headers_classes, install_dir : INCLUDES_PATH)
install_headers(headers_classes_private, install_dir : INCLUDES_PATH+'/private')
# install_headers(headers_other, install_dir : INCLUDES_PATH+'/other')

egl_dep             = cpp.find_library('EGL', dirs: library_paths, required: true)
glesv2_dep          = cpp.find_library('GLESv2', dirs: library_paths, required: true)
udev_dep            = cpp.find_library('udev', dirs: library_paths, required: true)
pthread_dep         = cpp.find_library('pthread', dirs: library_paths, required: true)
drm_dep             = cpp.find_library('drm', dirs: library_paths, required: true)
gbm_dep             = cpp.find_library('gbm', dirs: library_paths, required: true)
display_info_dep    = cpp.find_library('display-info', dirs: library_paths, required: true)

SRM = library(
    'SRM',
    sources : run_command('find', './src/lib', '-type', 'f', '-name', '*[.cpp,.c]').stdout().strip().split('\n'),
    include_directories : include_paths,
    dependencies : [
        display_info_dep,
        egl_dep,
        glesv2_dep,
        udev_dep,
        pthread_dep,
        drm_dep,
        gbm_dep
    ],
    install : true, 
    install_dir : LIBRARY_PATH)

srm_dep = declare_dependency(
    dependencies: [], 
    include_directories : include_paths,
    link_with : SRM)

if get_option('build_examples')
    subdir('src/examples/srm-display-info')
    subdir('src/examples/srm-all-connectors')
endif

