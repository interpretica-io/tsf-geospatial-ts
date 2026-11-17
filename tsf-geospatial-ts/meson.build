# Copyright (C) 2025-2026 Interpretica Unipessoal Lda
project('tsf-geospatial-ts', 'c',
    version: '1.0.0',
    default_options: 'werror=true'
)
te_path = get_option('te_path')
te_libdir = get_option('te_libdir')
add_project_arguments(get_option('te_cflags').split(), language: 'c')
add_project_link_arguments(get_option('te_ldflags').split(), language: 'c')

cc = meson.get_compiler('c')

te_tests_info_sh = find_program(join_paths(te_path, 'te_tests_info.sh'))

test_deps = [
    dependency('threads'),
    cc.find_library('m'),
]

# The libraries under test come first: a dependant has to be named
# before what it depends on. tapi_geospatial is the engine side; it
# talks to ta_geospatial/rpcs_geospatial on the agent over the RPCs, so
# the engine needs no GEOS, PROJ or GDAL. tapi_geospatial_postgis builds
# on tsf-sql's tapi_sql.
te_libs = [
    'tapi_geospatial_postgis',
    'tapi_geospatial',
    'tapi_sql',
    'tapi_env',
    'tapi_rpc',
    'rcfrpc',
    'tapi',
    'confapi',
    'conf_oid',
    'rcfapi',
    'rpcxdr',
    'rpc_types',
    'logger_ten',
    'ipc',
    'tools',
    'logger_core',
]

foreach lib : te_libs
    test_deps += cc.find_library(lib, dirs: te_libdir)
endforeach

lib_dir = include_directories('lib')

# The line below would produce empty dependencies on systems
# having no tirpc - it is not a problem.
dep_tirpc = dependency('libtirpc', required: false)

test_deps += [ dep_tirpc ]

subdir('lib')

test_deps += declare_dependency(include_directories: lib_dir,
                                link_with: [ ts_lib ])


tests = [ 'prologue', 'epilogue' ]

package_dir = 'tsf-geospatial-ts'
package_tests_c = []

foreach test : tests
    test_exe = test
    test_c = test + '.c'
    package_tests_c += [ test_c ]
    executable(test_exe, test_c, install: true, install_dir: package_dir,
               dependencies: test_deps)
endforeach

install_data([ 'mainpage.dox', 'package.xml' ],
             install_dir: package_dir)

mydir = package_dir

# Unrolled rather than looped: subdir() takes a literal here, and each
# group resets package_dir and package_tests_c for the block below it.
package_dir = join_paths(mydir, 'geometry')
package_tests_c = []
subdir('geometry')

package_dir = join_paths(mydir, 'crs')
package_tests_c = []
subdir('crs')

package_dir = join_paths(mydir, 'vector')
package_tests_c = []
subdir('vector')

package_dir = join_paths(mydir, 'raster')
package_tests_c = []
subdir('raster')

package_dir = join_paths(mydir, 'tracking')
package_tests_c = []
subdir('tracking')

package_dir = join_paths(mydir, 'generation')
package_tests_c = []
subdir('generation')

package_dir = join_paths(mydir, 'services')
package_tests_c = []
subdir('services')

package_dir = join_paths(mydir, 'postgis')
package_tests_c = []
subdir('postgis')

package_dir = join_paths(mydir, 'robust')
package_tests_c = []
subdir('robust')

package_dir = join_paths(mydir, 'perf')
package_tests_c = []
subdir('perf')
