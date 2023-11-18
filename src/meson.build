project(
    'SRM',
    'c',
    version : run_command('cat', '../VERSION', check : false).stdout(),
    meson_version: '>= 0.59.0',
    default_options: [
        'warning_level=2',
        'buildtype=release'
    ]
)

little_endian = target_machine.endian() == 'little'
big_endian = target_machine.endian() == 'big'

add_project_arguments([
        '-DSRM_LITTLE_ENDIAN=@0@'.format(little_endian.to_int()),
        '-DSRM_BIG_ENDIAN=@0@'.format(big_endian.to_int()),
], language: 'c')

c = meson.get_compiler('c')

if get_option('buildtype') == 'custom'
    proj_args = ['-Ofast', '-g3', '-s', '-march=native', '-fno-strict-aliasing']
    add_project_arguments(proj_args, language : 'c')
endif

# ------------ INSTALL CONFIG ------------

# Install directories
headers_install_dir = '/usr/include/SRM'
examples_install_dir = '/usr/bin'

# Public and private API headers
api_headers = ['/', '/private', '/private/modes']

foreach subdir : api_headers
    install_headers(run_command('find', './lib' + subdir, '-type', 'f', '-name', '*.h', '-maxdepth', '1', check : false).stdout().strip().split('\n'),
        install_dir : headers_install_dir + subdir)
endforeach

# ------------ SETUP INCLUDE PATHS ------------

include_paths = []

include_paths_str = [
    './lib',
    '/usr/include/drm',
    '/usr/include/libdrm'
]

foreach p : include_paths_str
    if run_command('[', '-d', p, ']', check : false).returncode() == 0
      include_paths += [include_directories(p)]
    endif
endforeach

# ------------ DEPENDENCIES ------------

pkg              = import('pkgconfig')
egl_dep          = dependency('egl')
glesv2_dep       = dependency('glesv2')
udev_dep         = dependency('libudev')
pthread_dep      = c.find_library('pthread')
drm_dep          = dependency('libdrm')
gbm_dep          = dependency('gbm')
display_info_dep = dependency('libdisplay-info')

# ------------ SOURCE CODE FILES ------------

headers = run_command('find', './lib', '-type', 'f', '-name', '*.h', check : false).stdout().strip().split('\n')
sources = run_command('find', './lib', '-type', 'f', '-name', '*.c', check : false).stdout().strip().split('\n')

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
    install : true)

meson.add_install_script('../scripts/ld.sh', get_option('prefix') + '/' + get_option('libdir'))

if get_option('build_examples') or get_option('build_tests')
    srm_dep = declare_dependency(
        dependencies: [],
        include_directories : include_paths,
        link_with : SRM)
endif

if get_option('build_examples')
    m_dep = c.find_library('m')
    libinput_dep = dependency('libinput')
    libseat_dep = dependency('libseat')

    subdir('examples/srm-basic')
    subdir('examples/srm-display-info')
    subdir('examples/srm-all-connectors')
    subdir('examples/srm-multi-session')
endif

if get_option('build_tests')
    subdir('tests/srm-test-lists')
    subdir('tests/srm-test-memory')
    subdir('tests/srm-test-memcpy')
endif
