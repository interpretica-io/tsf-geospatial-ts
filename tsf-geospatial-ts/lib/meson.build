# Copyright (C) 2026 Interpretica Unipessoal Lda

# The agent-side scripts (the control OGC server, the server launcher)
# are embedded into the helpers as C strings, so a test can put them
# onto any agent, local or remote.
py3 = find_program('python3')
py_embed = custom_target('tsapi_py',
    input: files('py/ogcserver.py', 'py/servers.sh'),
    output: 'tsapi_py.h',
    command: [ py3, files('py/embed.py'), '@OUTPUT@', '@INPUT@' ])

sources = [
    'tsapi_geo.c',
    py_embed,
]

ts_lib = static_library('ts_lib', sources,
                        dependencies: test_deps,
                        include_directories: lib_dir)
