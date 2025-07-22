project(
    'SRM',
    'c', 'cpp',
    version : run_command('cat', 'VERSION', check : false).stdout(),
    meson_version: '>= 0.59.0',
    default_options: [
        'warning_level=2',
        'buildtype=release',
        'cpp_std=c++23']
)

version_major = meson.project_version().split('.')[0]
version_minor = meson.project_version().split('.')[1]
version_patch = meson.project_version().split('.')[2]
version_build = run_command('cat', 'BUILD', check : false).stdout()

HEADERS_INSTALL_PATH = join_paths(get_option('prefix'), get_option('includedir'), 'CZ/SRM')

little_endian = target_machine.endian() == 'little'
big_endian = target_machine.endian() == 'big'

add_project_arguments([
    '-DSRM_LITTLE_ENDIAN=@0@'.format(little_endian.to_int()),
    '-DSRM_BIG_ENDIAN=@0@'.format(big_endian.to_int())
], language: 'cpp')

# -------------- HEADERS --------------

header_dirs = [
    ['src/CZ/SRM', '']
]

foreach header_dir : header_dirs
    header_files = run_command('find', header_dir[0], '-type', 'f', '-name', '*.h', '-maxdepth', '1', check : false).stdout().strip().split('\n')
    install_headers(header_files, install_dir : join_paths(HEADERS_INSTALL_PATH, header_dir[1]))
endforeach

# ------------ DEPENDENCIES ------------

cpp              = meson.get_compiler('cpp')
pkg              = import('pkgconfig')
gl_dep           = dependency('gl', version: '>= 1.2')
egl_dep          = dependency('egl', version: '>= 1.5')
glesv2_dep       = dependency('glesv2', version: '>= 3.2')
udev_dep         = dependency('libudev', version: '>= 249')
drm_dep          = dependency('libdrm', version: '>= 2.4.113')
gbm_dep          = dependency('gbm', version: '>= 23.2.1')
display_info_dep = dependency('libdisplay-info')
drm_headers_dep  = drm_dep.partial_dependency(compile_args: true, includes: true)
cz_ream_dep      = dependency('cz-ream')

# ------------ SOURCE CODE FILES ------------

cz_srm = library(
    'cz-srm',
    sources : run_command('find', './src/CZ', '-type', 'f', '-name', '*[.cpp,.c]', check : false).stdout().strip().split('\n'),
    include_directories : ['src'],
    dependencies : [
        cz_ream_dep,
        display_info_dep,
        egl_dep,
        gl_dep,
        glesv2_dep,
        udev_dep,
        drm_dep,
        gbm_dep
    ],
    soversion: version_major,
    install : true)

cz_srm_dep = declare_dependency(
    dependencies: [],
    include_directories : ['src', 'src/CZ/SRM'],
    link_with : cz_srm)

pkg.generate(
    cz_srm,
    name: 'Simple Rendering Manager',
    description: 'C++ library for building DRM/KMS applications',
    version: meson.project_version(),
    subdirs: ['CZ/SRM'],
    filebase: 'cz-srm')

subdir('src/examples/srm-basic')

#if get_option('build_examples') or get_option('build_tests')
#    srm_dep = declare_dependency(
#        dependencies: [],
#        include_directories : include_paths,
#        link_with : SRM)
#endif

#if get_option('build_examples')
#    libinput_dep = dependency('libinput', version: '>= 1.20.0')
#    libseat_dep = dependency('libseat', version: '>= 0.6.4')
#    m_dep = c.find_library('m')
#
#    subdir('examples/srm-basic')
#    subdir('examples/srm-display-info')
#    subdir('examples/srm-all-connectors')
#    subdir('examples/srm-multi-session')
#    subdir('examples/srm-direct-scanout')
#endif

#if get_option('build_tests')
#    subdir('tests/srm-test-lists')
#    subdir('tests/srm-test-memory')
#    subdir('tests/srm-test-memcpy')
#endif
