sources = run_command('find', '.', '-type', 'f', '-name', '*[.c,.h]').stdout().strip().split('\n')

executable(
    'srm-tests-memory',
    sources : sources,
    dependencies : [
        srm_dep,
    ],
    install : false)
