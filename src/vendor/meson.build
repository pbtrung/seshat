libfoundation_sources = files('''
    foundation/android.c
    foundation/array.c
    foundation/assert.c
    foundation/assetstream.c
    foundation/atomic.c
    foundation/base64.c
    foundation/beacon.c
    foundation/bitbuffer.c
    foundation/blowfish.c
    foundation/bufferstream.c
    foundation/environment.c
    foundation/error.c
    foundation/event.c
    foundation/exception.c
    foundation/foundation.c
    foundation/fs.c
    foundation/hash.c
    foundation/hashmap.c
    foundation/hashtable.c
    foundation/json.c
    foundation/library.c
    foundation/log.c
    foundation/main.c
    foundation/md5.c
    foundation/memory.c
    foundation/mutex.c
    foundation/objectmap.c
    foundation/path.c
    foundation/pipe.c
    foundation/pnacl.c
    foundation/process.c
    foundation/profile.c
    foundation/radixsort.c
    foundation/random.c
    foundation/regex.c
    foundation/ringbuffer.c
    foundation/semaphore.c
    foundation/sha.c
    foundation/stacktrace.c
    foundation/stream.c
    foundation/string.c
    foundation/system.c
    foundation/thread.c
    foundation/time.c
    foundation/tizen.c
    foundation/uuid.c
    foundation/version.c
'''.split())

misc_sources = files('''
    argon2/argon2.c
    argon2/core.c
    argon2/encoding.c
    argon2/opt.c
    argon2/thread.c
    argon2/blake2/blake2b.c
    hc256/hc256.c
    skein3fish/skein.c
    skein3fish/skeinApi.c
    skein3fish/skeinBlockNo3F.c
    skein3fish/threefish1024Block.c
    skein3fish/threefish256Block.c
    skein3fish/threefish512Block.c
    skein3fish/threefishApi.c
    zbase32/zbase32.c
    rabin/rabin.c
'''.split())

libvendor_sources = libfoundation_sources + misc_sources

libvendor = static_library('vendor',
                            libvendor_sources,
                            include_directories : inc_dir)