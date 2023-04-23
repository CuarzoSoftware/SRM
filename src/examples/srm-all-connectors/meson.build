sources = run_command('find', '.', '-type', 'f', '-name', '*[.c,.cpp,.h,.hpp]').stdout().strip().split('\n')

executable(
    'srm-all-connectors',
    sources : sources,
    dependencies : [
        srm_dep,
        glesv2_dep,
    ],
    install : true, 
    install_dir : EXAMPLES_PATH)
