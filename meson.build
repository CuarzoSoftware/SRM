project(
    'SRM',
    'c', 'cpp',
    version : run_command('cat', 'VERSION', check : false).stdout(),
    meson_version: '>= 0.59.0',
    default_options: [
        'warning_level=2',
        'buildtype=release',
        'cpp_std=c++20']
)

VERSION_MAJOR = meson.project_version().split('.')[0]
VERSION_MINOR = meson.project_version().split('.')[1]
VERSION_PATCH = meson.project_version().split('.')[2]
VERSION_BUILD = run_command('cat', './BUILD', check : false).stdout()
HEADERS_INSTALL_PATH = join_paths(get_option('prefix'), get_option('includedir'), 'CZ/', meson.project_name())
CONF_FILE_NAME = 'CZ' + meson.project_name() + 'Version.h'

conf = configuration_data()
conf.set('VERSION_MAJOR', VERSION_MAJOR)
conf.set('VERSION_MINOR', VERSION_MINOR)
conf.set('VERSION_PATCH', VERSION_PATCH)
conf_file = configure_file(
    input : 'src/CZ/' + meson.project_name() + '/' + CONF_FILE_NAME +'.in',
    output : CONF_FILE_NAME,
    configuration : conf)

install_data(
    conf_file,
    install_dir: HEADERS_INSTALL_PATH)

# -------------- HEADERS --------------

header_dirs = [
    ['src/CZ/SRM', '']
]

foreach header_dir : header_dirs
    header_files = run_command('find', header_dir[0], '-type', 'f', '-name', '*.h', '-maxdepth', '1', check : false).stdout().strip().split('\n')
    install_headers(header_files, install_dir : join_paths(HEADERS_INSTALL_PATH, header_dir[1]))
endforeach

# ------------ DEPENDENCIES ------------

pkg              = import('pkgconfig')
cz_core_dep      = dependency('cz-ream')
cz_ream_dep      = dependency('cz-ream')
udev_dep         = dependency('libudev', version: '>= 249')
drm_dep          = dependency('libdrm', version: '>= 2.4.113')
gbm_dep          = dependency('gbm', version: '>= 23.2.1')
display_info_dep = dependency('libdisplay-info')
drm_headers_dep  = drm_dep.partial_dependency(compile_args: true, includes: true)

# ------------ SOURCE CODE FILES ------------

cz_srm = library(
    'cz-srm',
    sources : run_command('find', './src/CZ', '-type', 'f', '-name', '*[.cpp,.c]', check : false).stdout().strip().split('\n'),
    include_directories : ['src'],
    dependencies : [
        cz_core_dep,
        cz_ream_dep,
        display_info_dep,
        udev_dep,
        drm_dep,
        gbm_dep
    ],
    soversion: VERSION_MAJOR,
    install : true)

cz_srm_dep = declare_dependency(
    dependencies: [cz_ream_dep, drm_headers_dep],
    include_directories : ['src', 'src/CZ/SRM'],
    link_with : cz_srm)

pkg.generate(
    cz_srm,
    name: 'Simple Rendering Manager',
    description: 'C++ library for building DRM/KMS applications',
    version: meson.project_version(),
    subdirs: ['CZ/SRM'],
    filebase: 'cz-srm')

if get_option('build_examples')
    libseat_dep = dependency('libseat', version: '>= 0.6.4')
    subdir('src/examples/srm-basic')

#    libinput_dep = dependency('libinput', version: '>= 1.20.0')
#    m_dep = c.find_library('m')
#
#    subdir('examples/srm-display-info')
#    subdir('examples/srm-all-connectors')
#    subdir('examples/srm-multi-session')
#    subdir('examples/srm-direct-scanout')
endif

