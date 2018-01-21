libseshat_sources = files('''
    utils/rc.c
    crypto/skein.c
    crypto/threefish.c
    crypto/key.c
    crypto/ctr.c
'''.split())

libseshat = static_library('seshat',
                            libseshat_sources,
                            include_directories : inc_dir)

subdir('vendor')

seshat_sources = files('''
    main.c
'''.split())
