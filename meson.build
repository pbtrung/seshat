project('seshat', 'c', 
		version : '0.1.0',
        license : 'BSD-3-Clause',
        default_options : 'c_std=c11')

cc = meson.get_compiler('c')

c_args = '''
    -Wall 
    -Wextra 
    -Wpedantic
    -Wformat=2 
    -Wno-unused-parameter 
    -Wshadow
    -Wno-parentheses
    -Wwrite-strings 
    -Wstrict-prototypes
    -Wold-style-definition
    -Wnested-externs
    -Wmissing-include-dirs 
    -Wno-type-limits
    -DFOUNDATION_COMPILE=1
    -DBUILD_DEPLOY=1
    -DBUILD_ENABLE_LOG=1
    -DBUILD_ENABLE_ASSERT=1
'''.split()

foreach arg: c_args
    if cc.has_argument(arg)
        add_project_arguments(arg, language : 'c')
    endif
endforeach

libzstd = dependency('libzstd', version : '>= 1.3.0')
libcurl = dependency('libcurl', version : '>= 7.57.0')
libsodium = dependency('libsodium', version : '>= 1.0.16')
sqlite3 = dependency('sqlite3', version : '>= 3.21')
threads = dependency('threads')
dl = cc.find_library('dl', required: true)
m = cc.find_library('m', required: true)

inc_dir = include_directories(['include', 'include/vendor'])

subdir('src')
subdir('tests')

dependencies = [libzstd, libcurl, libsodium, sqlite3, threads, dl, m]

executable('seshat', seshat_sources,
		   link_with : [libseshat, libvendor],
		   dependencies : dependencies,
           install : false,
		   include_directories : inc_dir)

executable('test_seshat', seshat_test_sources,
		   link_with : [libseshat, libvendor],
		   dependencies : dependencies,
           install : false,
		   include_directories : inc_dir)

run_target('format', command : 'format.sh')