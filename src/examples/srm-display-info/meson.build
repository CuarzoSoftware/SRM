sources = run_command('find', '.', '-type', 'f', '-name', '*[.c,.h]').stdout().strip().split('\n')

executable(
    'srm-display-info',
    sources : sources,
    dependencies : [
        srm_dep
    ],
    install : true, 
    install_dir : examples_install_dir)
