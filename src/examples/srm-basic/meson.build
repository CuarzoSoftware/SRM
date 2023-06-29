sources = run_command('find', '.', '-type', 'f', '-name', '*[.c,.h]').stdout().strip().split('\n')

executable(
    'srm-basic',
    sources : sources,
    dependencies : [
        srm_dep,
        glesv2_dep,
        m_dep
    ],
    install : true, 
    install_dir : examples_install_dir)
