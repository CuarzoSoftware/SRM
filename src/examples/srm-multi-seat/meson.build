sources = run_command('find', '.', '-type', 'f', '-name', '*[.c,.h]').stdout().strip().split('\n')

executable(
    'srm-multi-seat',
    sources : sources,
    dependencies : [
        srm_dep,
        glesv2_dep,
        m_dep,
        udev_dep,
        libinput_dep,
        libseat_dep
    ],
    install : true, 
    install_dir : examples_install_dir)
