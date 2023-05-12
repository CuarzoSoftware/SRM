sources = run_command('find', '.', '-type', 'f', '-name', '*[.c,.h]').stdout().strip().split('\n')

m_dep = c.find_library('m', dirs: library_paths, required: true)

executable(
    'srm-all-connectors',
    sources : sources,
    dependencies : [
        srm_dep,
        glesv2_dep,
        m_dep
    ],
    install : true, 
    install_dir : EXAMPLES_PATH)
