project(
    'SRM',
    'c', 
    version : '0.1.0',
    meson_version: '>= 0.56.0',
	  default_options: [
		'warning_level=2',
		'buildtype=debug'
	]
)

little_endian = target_machine.endian() == 'little'
big_endian = target_machine.endian() == 'big'

add_project_arguments([
        '-DSRM_LITTLE_ENDIAN=@0@'.format(little_endian.to_int()),
        '-DSRM_BIG_ENDIAN=@0@'.format(big_endian.to_int()),
], language: 'c')

c = meson.get_compiler('c')

# ------------ INSTALL CONFIG ------------

# Install directories
library_install_dir = '/usr/lib'
headers_install_dir = '/usr/include/SRM'
examples_install_dir = '/usr/bin'

# Public and private API headers
api_headers = ['/', '/private', '/private/modes']

foreach subdir : api_headers
    install_headers(run_command('find', './lib' + subdir, '-type', 'f', '-name', '*.h', '-maxdepth', '1').stdout().strip().split('\n'),
        install_dir : headers_install_dir + subdir)
endforeach

# ------------ SETUP INCLUDE PATHS ------------

include_paths = []

include_paths_str = [
    './lib',
    '/usr/local/include',
    '/usr/include/drm',
    '/usr/include/libdrm'
]

foreach p : include_paths_str
    if run_command('[', '-d', p, ']').returncode() == 0
      include_paths += [include_directories(p)]
    endif
endforeach

# ------------ DEPENDENCIES ------------

library_paths_str = [
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

foreach p : library_paths_str
    if run_command('[', '-d', p, ']').returncode() == 0
      library_paths += p
    endif
endforeach

egl_dep             = c.find_library('EGL', dirs: library_paths, required: true)
glesv2_dep          = c.find_library('GLESv2', dirs: library_paths, required: true)
udev_dep            = c.find_library('udev', dirs: library_paths, required: true)
pthread_dep         = c.find_library('pthread', dirs: library_paths, required: true)
drm_dep             = c.find_library('drm', dirs: library_paths, required: true)
gbm_dep             = c.find_library('gbm', dirs: library_paths, required: true)
display_info_dep    = c.find_library('display-info', dirs: library_paths, required: true)


# ------------ SOURCE CODE FILES ------------

headers = run_command('find', './lib', '-type', 'f', '-name', '*.h').stdout().strip().split('\n')
sources = run_command('find', './lib', '-type', 'f', '-name', '*.c').stdout().strip().split('\n')

SRM = library(
    'SRM',
    sources : sources,
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
    install_dir : library_install_dir)

srm_dep = declare_dependency(
    dependencies: [], 
    include_directories : include_paths,
    link_with : SRM)

if get_option('build_examples')

    m_dep = c.find_library('m', dirs: library_paths, required: true)

    subdir('examples/srm-basic')
    subdir('examples/srm-display-info')
    subdir('examples/srm-all-connectors')
endif

if get_option('build_tests')
    subdir('tests/srm-test-lists')
endif

