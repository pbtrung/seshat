/* fs.c  -  Foundation library  -  Public Domain  -  2013 Mattias Jansson /
 * Rampant Pixels
 *
 * This library provides a cross-platform foundation library in C11 providing
 * basic support data types and functions to write applications and games in a
 * platform-independent fashion. The latest source code is always available at
 *
 * https://github.com/rampantpixels/foundation_lib
 *
 * This library is put in the public domain; you can redistribute it and/or
 * modify it without any restrictions.
 */

#include <foundation/foundation.h>
#include <foundation/internal.h>

#if FOUNDATION_PLATFORM_WINDOWS
#include <foundation/windows.h>
#include <sys/utime.h>
#elif FOUNDATION_PLATFORM_POSIX
#include <dirent.h>
#include <fcntl.h>
#include <foundation/posix.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <utime.h>
#endif

#if FOUNDATION_PLATFORM_LINUX || FOUNDATION_PLATFORM_ANDROID
#if FOUNDATION_COMPILER_GCC
#pragma GCC diagnostic push
#if FOUNDATION_GCC_VERSION > 40700
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#elif FOUNDATION_COMPILER_CLANG
#if __has_warning("-Wpedantic")
#pragma clang diagnostic ignored "-Wpedantic"
#endif
#endif
#include <sys/inotify.h>
#if FOUNDATION_COMPILER_GCC
#pragma GCC diagnostic pop
#endif
#endif

#if FOUNDATION_PLATFORM_PNACL
#include <foundation/pnacl.h>
#include <ppapi/c/pp_completion_callback.h>
#include <ppapi/c/pp_directory_entry.h>
#include <ppapi/c/pp_file_info.h>
#include <ppapi/c/ppb_core.h>
#include <ppapi/c/ppb_file_io.h>
#include <ppapi/c/ppb_file_ref.h>
#include <ppapi/c/ppb_file_system.h>
#include <ppapi/c/ppb_var.h>
#include <ppapi/c/ppp_instance.h>
static PP_Resource _pnacl_fs_temporary;
static PP_Resource _pnacl_fs_persistent;
static const PPB_FileSystem *_pnacl_file_system;
static const PPB_FileIO *_pnacl_file_io;
static const PPB_FileRef *_pnacl_file_ref;
static const PPB_Var *_pnacl_var;
static const PPB_Core *_pnacl_core;
#endif

#include <stdio.h>

#if FOUNDATION_PLATFORM_PNACL
typedef PP_Resource fs_file_descriptor;
#else
typedef FILE *fs_file_descriptor;
#endif

struct fs_monitor_t {
    string_t path;
    thread_t thread;
    bool inuse;
};

struct stream_file_t {
    /*lint -e830 -e754 It is used, through stream type */
    FOUNDATION_DECLARE_STREAM;

    fs_file_descriptor fd;

#if FOUNDATION_PLATFORM_PNACL
    size_t position;
    size_t size;
#endif
};

typedef FOUNDATION_ALIGN(16) struct fs_monitor_t fs_monitor_t;
typedef FOUNDATION_ALIGN(8) struct stream_file_t stream_file_t;

#define GET_FILE(s) ((stream_file_t *)(s))
#define GET_FILE_CONST(s) ((const stream_file_t *)(s))
#define GET_STREAM(f) ((stream_t *)(f))

static stream_vtable_t _fs_file_vtable;

static mutex_t *_fs_monitor_lock;
static fs_monitor_t *_fs_monitors;
static event_stream_t *_fs_event_stream;

#if FOUNDATION_PLATFORM_WINDOWS || FOUNDATION_PLATFORM_LINUX ||                \
    FOUNDATION_PLATFORM_ANDROID || FOUNDATION_PLATFORM_MACOS
#define FOUNDATION_HAVE_FS_MONITOR 1
static void *_fs_monitor(void *);
#else
#define FOUNDATION_HAVE_FS_MONITOR 0
#endif

static string_const_t _fs_strip_protocol(const char *path, size_t length) {
    string_const_t stripped = path_strip_protocol(path, length);
    ptrdiff_t diff = pointer_diff(stripped.str, path);
    if (!diff)
        return stripped;
    if (((diff == 6) || (diff == 7)) &&
        string_equal(path, 6, STRING_CONST("file:/")))
        return stripped;
    return string_empty();
}

#if FOUNDATION_PLATFORM_PNACL

static PP_Resource _fs_resolve_path(const char *path, size_t length,
                                    string_const_t *localpath) {
    static string_const_t rootpath = (string_const_t) {
        STRING_CONST("/")
    };
    if ((length > 3) && string_equal(path, 4, STRING_CONST("/tmp"))) {
        if (length == 4) {
            *localpath = rootpath;
            return _pnacl_fs_temporary;
        } else if (path[4] == '/') {
            *localpath = string_substr(path, length, 4, length - 4);
            return _pnacl_fs_temporary;
        }
    } else if ((length > 10) &&
               string_equal(path, 11, STRING_CONST("/persistent"))) {
        if (length == 11) {
            *localpath = rootpath;
            return _pnacl_fs_temporary;
        } else if (path[11] == '/') {
            *localpath = string_substr(path, length, 11, length - 11);
            return _pnacl_fs_persistent;
        }
    } else if ((length > 5) && string_equal(path, 6, STRING_CONST("/cache"))) {
        /* TODO: PNaCl implement
        if( path[6] == 0 )
        {
                *localpath = rootpath;
                return _pnacl_fs_cache;
        }
        else if( path[6] == '/' )
        {
                localpath = path + 6;
                return _pnacl_fs_cache;
        }*/
        return 0;
    } else if (length && (path[0] != '/')) {
        // Current working dir is always /tmp on PNaCl
        *localpath = string_const(path, length);
        return _pnacl_fs_temporary;
    }

    // log_warnf(HASH_PNACL, WARNING_INVALID_VALUE, STRING_CONST("Invalid file
    // path: %.*s"),
    //          (int)length, path);
    return 0;
}

#endif

bool fs_monitor(const char *path, size_t length) {
    bool ret = false;
#if FOUNDATION_HAVE_FS_MONITOR
    size_t mi;
    char buf[BUILD_MAX_PATHLEN];
    string_t path_clone;

    mutex_lock(_fs_monitor_lock);

    for (mi = 0; mi < foundation_config().fs_monitor_max; ++mi) {
        if (_fs_monitors[mi].inuse &&
            string_equal(STRING_ARGS(_fs_monitors[mi].path), path, length)) {
            mutex_unlock(_fs_monitor_lock);
            return true;
        }
    }

    memory_context_push(HASH_STREAM);

    path_clone = string_copy(buf, BUILD_MAX_PATHLEN, path, length);
    path_clone = path_clean(STRING_ARGS(path_clone), BUILD_MAX_PATHLEN);
    path_clone = path_absolute(STRING_ARGS(path_clone), BUILD_MAX_PATHLEN);
    path_clone = string_clone(STRING_ARGS(path_clone));

    for (mi = 0; mi < foundation_config().fs_monitor_max; ++mi) {
        if (!_fs_monitors[mi].inuse) {
            _fs_monitors[mi].inuse = true;
            _fs_monitors[mi].path = path_clone;
            thread_initialize(&_fs_monitors[mi].thread, _fs_monitor,
                              _fs_monitors + mi, STRING_CONST("fs_monitor"),
                              THREAD_PRIORITY_BELOWNORMAL, 0);
            thread_start(&_fs_monitors[mi].thread);
            ret = true;
            break;
        }
    }

    if (mi == foundation_config().fs_monitor_max) {
        string_deallocate(path_clone.str);
        log_errorf(
            0, ERROR_OUT_OF_MEMORY,
            STRING_CONST(
                "Unable to monitor file system, no free monitor slots: %.*s"),
            (int)length, path);
    }

    memory_context_pop();
    mutex_unlock(_fs_monitor_lock);

#else
    FOUNDATION_UNUSED(path);
    FOUNDATION_UNUSED(length);
#endif
    return ret;
}

static void _fs_stop_monitor(fs_monitor_t *monitor) {
    if (!monitor->inuse)
        return;

    thread_signal(&monitor->thread);
    thread_finalize(&monitor->thread);
    string_deallocate(monitor->path.str);
    monitor->inuse = false;
}

void fs_unmonitor(const char *path, size_t length) {
    size_t mi;

    mutex_lock(_fs_monitor_lock);

    if (_fs_monitors) {
        for (mi = 0; mi < foundation_config().fs_monitor_max; ++mi) {
            if (_fs_monitors[mi].inuse &&
                string_equal(STRING_ARGS(_fs_monitors[mi].path), path, length))
                _fs_stop_monitor(_fs_monitors + mi);
        }
    }

    mutex_unlock(_fs_monitor_lock);
}

bool fs_is_file(const char *path, size_t length) {
#if FOUNDATION_PLATFORM_WINDOWS

    string_const_t pathstr = _fs_strip_protocol(path, length);
    if (pathstr.length) {
        wchar_t *wpath =
            wstring_allocate_from_string(pathstr.str, pathstr.length);
        unsigned int attribs = GetFileAttributesW(wpath);
        wstring_deallocate(wpath);
        if ((attribs != 0xFFFFFFFF) && !(attribs & FILE_ATTRIBUTE_DIRECTORY))
            return true;
    }

#elif FOUNDATION_PLATFORM_POSIX

    char buffer[BUILD_MAX_PATHLEN];
    struct stat st;
    string_const_t pathstr = _fs_strip_protocol(path, length);
    if (pathstr.length) {
        string_t finalpath =
            string_copy(buffer, sizeof(buffer), STRING_ARGS(pathstr));
        memset(&st, 0, sizeof(st));
        stat(finalpath.str, &st);
        if (st.st_mode & S_IFREG)
            return true;
    }

#elif FOUNDATION_PLATFORM_PNACL

    bool is_file = false;
    string_const_t localpath;
    string_const_t pathstr = _fs_strip_protocol(path, length);
    PP_Resource fs = _fs_resolve_path(pathstr.str, pathstr.length, &localpath);
    if (!fs)
        return 0;

    char buffer[BUILD_MAX_PATHLEN + 1];
    string_t finalpath =
        string_copy(buffer + 1, sizeof(buffer) - 1, STRING_ARGS(localpath));
    if (finalpath.str[0] != '/') {
        *(--finalpath.str) = '/';
        finalpath.length++;
    }
    PP_Resource ref = _pnacl_file_ref->Create(fs, finalpath.str);
    if (!ref)
        return 0;

    struct PP_FileInfo info;
    if (_pnacl_file_ref->Query(ref, &info, PP_BlockUntilComplete()) == PP_OK)
        is_file = (info.type == PP_FILETYPE_REGULAR);

    _pnacl_core->ReleaseResource(ref);

    return is_file;

#else
#error Not implemented
#endif

    return false;
}

bool fs_is_directory(const char *path, size_t length) {
#if FOUNDATION_PLATFORM_WINDOWS

    string_const_t pathstr = _fs_strip_protocol(path, length);
    if (pathstr.length) {
        wchar_t *wpath =
            wstring_allocate_from_string(pathstr.str, pathstr.length);
        unsigned int attr = GetFileAttributesW(wpath);
        wstring_deallocate(wpath);
        if ((attr == 0xFFFFFFFF) || !(attr & FILE_ATTRIBUTE_DIRECTORY))
            return false;
    } else {
        return false;
    }

#elif FOUNDATION_PLATFORM_POSIX

    string_const_t pathstr = _fs_strip_protocol(path, length);
    if (pathstr.length) {
        char buffer[BUILD_MAX_PATHLEN];
        struct stat st;
        string_t finalpath =
            string_copy(buffer, sizeof(buffer), STRING_ARGS(pathstr));
        memset(&st, 0, sizeof(st));
        stat(finalpath.str, &st);
        if (!(st.st_mode & S_IFDIR))
            return false;
    } else {
        return false;
    }

#elif FOUNDATION_PLATFORM_PNACL

    bool is_dir = false;
    string_const_t localpath;
    string_const_t pathstr = _fs_strip_protocol(path, length);
    PP_Resource fs = _fs_resolve_path(pathstr.str, pathstr.length, &localpath);
    if (!fs)
        return false;

    char buffer[BUILD_MAX_PATHLEN + 1];
    string_t finalpath =
        string_copy(buffer + 1, sizeof(buffer) - 1, STRING_ARGS(localpath));
    if (finalpath.str[0] != '/') {
        *(--finalpath.str) = '/';
        finalpath.length++;
    }
    PP_Resource ref = _pnacl_file_ref->Create(fs, finalpath.str);
    if (!ref)
        return false;

    struct PP_FileInfo info;
    if (_pnacl_file_ref->Query(ref, &info, PP_BlockUntilComplete()) == PP_OK)
        is_dir = (info.type == PP_FILETYPE_DIRECTORY);

    _pnacl_core->ReleaseResource(ref);

    return is_dir;

#else
#error Not implemented
#endif

    return true;
}

string_t *fs_subdirs(const char *path, size_t length) {
    string_t *arr = 0;
#if FOUNDATION_PLATFORM_WINDOWS

    // Windows specific implementation of directory listing
    HANDLE find;
    WIN32_FIND_DATAW data;
    wchar_t *wpattern;
    size_t wsize = length;
    size_t capacity = length + 4;

    memory_context_push(HASH_STREAM);

    wpattern =
        memory_allocate(0, sizeof(wchar_t) * capacity, 0, MEMORY_TEMPORARY);
    wstring_from_string(wpattern, capacity, path, length);
    if (length && (path[length - 1] != '/'))
        wpattern[wsize++] = L'/';
    wpattern[wsize++] = L'*';
    wpattern[wsize] = 0;

    find = FindFirstFileW(wpattern, &data);
    if (find != INVALID_HANDLE_VALUE)
        do {
            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                string_t filename;
                if (data.cFileName[0] == L'.') {
                    if (!data.cFileName[1] || (data.cFileName[1] == L'.'))
                        continue; // Don't include . and .. directories
                }
                filename = string_allocate_from_wstring(
                               data.cFileName, wstring_length(data.cFileName));
                array_push(arr, filename);
            }
        } while (FindNextFileW(find, &data));
    FindClose(find);

    memory_deallocate(wpattern);
    memory_context_pop();

#elif FOUNDATION_PLATFORM_POSIX

    // POSIX specific implementation of directory listing
    char foundpath_buffer[BUILD_MAX_PATHLEN];
    string_t cleanpath =
        string_copy(foundpath_buffer, sizeof(foundpath_buffer), path, length);
    DIR *dir = opendir(cleanpath.str);
    if (dir) {
        struct dirent *entry = 0;
        struct stat st;

        memory_context_push(HASH_STREAM);

        while ((entry = readdir(dir)) != 0) {
            if (entry->d_name[0] == '.') {
                if (!entry->d_name[1] || (entry->d_name[1] == '.'))
                    continue; // Don't include . and .. directories
            }
            size_t entrylen = string_length(entry->d_name);
            string_t thispath =
                path_append(foundpath_buffer, length, sizeof(foundpath_buffer),
                            entry->d_name, entrylen);
            if (!stat(thispath.str, &st) && S_ISDIR(st.st_mode))
                array_push(arr, string_clone(entry->d_name, entrylen));
        }

        closedir(dir);

        memory_context_pop();
    }

#elif FOUNDATION_PLATFORM_PNACL

    memory_context_push(HASH_STREAM);

    string_const_t localpath;
    string_const_t pathstr = _fs_strip_protocol(path, length);
    PP_Resource fs = _fs_resolve_path(pathstr.str, pathstr.length, &localpath);
    if (!fs)
        return arr;

    char buffer[BUILD_MAX_PATHLEN + 1];
    string_t finalpath =
        string_copy(buffer + 1, sizeof(buffer) - 1, STRING_ARGS(localpath));
    if (finalpath.str[0] != '/') {
        *(--finalpath.str) = '/';
        finalpath.length++;
    }
    PP_Resource ref = _pnacl_file_ref->Create(fs, finalpath.str);
    if (!ref)
        return arr;

    pnacl_array_t entries = {0, 0};
    struct PP_ArrayOutput output = {&pnacl_array_output, &entries};
    if (_pnacl_file_ref->ReadDirectoryEntries(
            ref, output, PP_BlockUntilComplete()) == PP_OK) {
        struct PP_DirectoryEntry *entry = entries.data;
        for (unsigned int ient = 0; ient < entries.count; ++ient, ++entry) {
            if (entry->file_type == PP_FILETYPE_DIRECTORY) {
                uint32_t varlen = 0;
                const struct PP_Var namevar =
                    _pnacl_file_ref->GetName(entry->file_ref);
                const char *utfname = _pnacl_var->VarToUtf8(namevar, &varlen);
                string_t copyname = string_clone(utfname, varlen);
                array_push(arr, copyname);
            }
        }
    }

    _pnacl_core->ReleaseResource(ref);

    if (entries.data)
        memory_deallocate(entries.data);

    memory_context_pop();

#else
#error Not implemented
#endif

    return arr;
}

string_t *fs_files(const char *path, size_t length) {
    string_t *arr = 0;
#if FOUNDATION_PLATFORM_WINDOWS

    // Windows specific implementation of directory listing
    HANDLE find;
    WIN32_FIND_DATAW data;
    wchar_t *wpattern;
    size_t wsize = length;
    size_t capacity = length + 4;

    memory_context_push(HASH_STREAM);

    wpattern =
        memory_allocate(0, sizeof(wchar_t) * capacity, 0, MEMORY_TEMPORARY);
    wstring_from_string(wpattern, capacity, path, length);
    if (length && (path[length - 1] != '/'))
        wpattern[wsize++] = L'/';
    wpattern[wsize++] = L'*';
    wpattern[wsize] = 0;

    find = FindFirstFileW(wpattern, &data);
    if (find != INVALID_HANDLE_VALUE)
        do {
            if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                array_push(arr,
                           string_allocate_from_wstring(
                               data.cFileName, wstring_length(data.cFileName)));
        } while (FindNextFileW(find, &data));
    FindClose(find);

    memory_deallocate(wpattern);
    memory_context_pop();

#elif FOUNDATION_PLATFORM_POSIX

    // POSIX specific implementation of directory listing
    char localpath[BUILD_MAX_PATHLEN];
    string_copy(localpath, sizeof(localpath), path, length);
    DIR *dir = opendir(localpath);
    if (dir) {
        // We have a directory, parse and create virtual file system
        struct dirent *entry = 0;
        struct stat st;

        memory_context_push(HASH_STREAM);

        while ((entry = readdir(dir)) != 0) {
            size_t entrylen = string_length(entry->d_name);
            string_t thispath = path_append(
                                    localpath, length, sizeof(localpath), entry->d_name, entrylen);
            if (!stat(thispath.str, &st) && S_ISREG(st.st_mode))
                array_push(arr, string_clone(entry->d_name, entrylen));
        }

        closedir(dir);

        memory_context_pop();
    }

#elif FOUNDATION_PLATFORM_PNACL

    memory_context_push(HASH_STREAM);

    string_const_t localpath;
    string_const_t pathstr = _fs_strip_protocol(path, length);
    PP_Resource fs = _fs_resolve_path(pathstr.str, pathstr.length, &localpath);
    if (!fs)
        return arr;

    char buffer[BUILD_MAX_PATHLEN + 1];
    string_t finalpath =
        string_copy(buffer + 1, sizeof(buffer) - 1, STRING_ARGS(localpath));
    if (finalpath.str[0] != '/') {
        *(--finalpath.str) = '/';
        finalpath.length++;
    }
    PP_Resource ref = _pnacl_file_ref->Create(fs, finalpath.str);
    if (!ref)
        return arr;

    pnacl_array_t entries = {0, 0};
    struct PP_ArrayOutput output = {&pnacl_array_output, &entries};
    if (_pnacl_file_ref->ReadDirectoryEntries(
            ref, output, PP_BlockUntilComplete()) == PP_OK) {
        struct PP_DirectoryEntry *entry = entries.data;
        for (unsigned int ient = 0; ient < entries.count; ++ient, ++entry) {
            if (entry->file_type == PP_FILETYPE_REGULAR) {
                uint32_t varlen = 0;
                const struct PP_Var namevar =
                    _pnacl_file_ref->GetName(entry->file_ref);
                const char *utfname = _pnacl_var->VarToUtf8(namevar, &varlen);
                string_t copyname = string_clone(utfname, varlen);
                array_push(arr, copyname);
            }
        }
    }

    if (entries.data)
        memory_deallocate(entries.data);

    _pnacl_core->ReleaseResource(ref);

    memory_context_pop();

#else
#error Not implemented
#endif

    return arr;
}

bool fs_remove_file(const char *path, size_t length) {
    bool result;
    string_const_t fspath;
#if FOUNDATION_PLATFORM_WINDOWS
    wchar_t *wpath;
#endif

    fspath = _fs_strip_protocol(path, length);
    if (!fspath.length)
        return false;

#if FOUNDATION_PLATFORM_WINDOWS

    wpath = wstring_allocate_from_string(fspath.str, fspath.length);
    result = DeleteFileW(wpath);
    wstring_deallocate(wpath);

#elif FOUNDATION_PLATFORM_POSIX

    char buffer[BUILD_MAX_PATHLEN];
    string_t finalpath =
        string_copy(buffer, sizeof(buffer), STRING_ARGS(fspath));
    result = (unlink(finalpath.str) == 0);

#elif FOUNDATION_PLATFORM_PNACL

    string_const_t localpath;
    PP_Resource fs = _fs_resolve_path(fspath.str, fspath.length,
                                      (string_const_t *)&localpath);
    if (!fs)
        return 0;

    char buffer[BUILD_MAX_PATHLEN + 1];
    string_t finalpath =
        string_copy(buffer + 1, sizeof(buffer) - 1, STRING_ARGS(localpath));
    if (finalpath.str[0] != '/') {
        *(--finalpath.str) = '/';
        finalpath.length++;
    }
    PP_Resource ref = _pnacl_file_ref->Create(fs, finalpath.str);
    if (!ref)
        return 0;

    result = (_pnacl_file_ref->Delete(ref, PP_BlockUntilComplete()) == PP_OK);
    _pnacl_core->ReleaseResource(ref);

#else
#error Not implemented
#endif

    return result;
}

bool fs_remove_directory(const char *path, size_t length) {
    /*lint --e{438,613,838} Lint gets seriously confused about the arrays here
     */
    bool result = false;
    string_t *subpaths;
    string_t *subfiles;
    string_const_t abspath, fspath;
    string_t localpath = {0, 0};
    char abspath_buffer[BUILD_MAX_PATHLEN];
    size_t fspathofs;
    size_t remain;
#if FOUNDATION_PLATFORM_WINDOWS
    wchar_t *wfpath = 0;
#endif
    size_t i, num;

    localpath = string_copy(abspath_buffer, BUILD_MAX_PATHLEN, path, length);
    abspath = string_to_const(localpath);

    fspath = _fs_strip_protocol(STRING_ARGS(abspath));
    if (!fs_is_directory(STRING_ARGS(fspath)))
        goto end;

    fspathofs = (size_t)pointer_diff(fspath.str, abspath_buffer);
    remain = BUILD_MAX_PATHLEN - fspathofs;
    subpaths = fs_subdirs(STRING_ARGS(fspath));
    for (i = 0, num = array_size(subpaths); i < num; ++i) {
        localpath = path_append(abspath_buffer + fspathofs, fspath.length,
                                remain, STRING_ARGS(subpaths[i]));
        fs_remove_directory(STRING_ARGS(localpath));
    }
    string_array_deallocate(subpaths);

    subfiles = fs_files(STRING_ARGS(fspath));
    for (i = 0, num = array_size(subfiles); i < num; ++i) {
        localpath = path_append(abspath_buffer + fspathofs, fspath.length,
                                remain, STRING_ARGS(subfiles[i]));
        fs_remove_file(STRING_ARGS(localpath));
    }
    string_array_deallocate(subfiles);

#if FOUNDATION_PLATFORM_WINDOWS

    wfpath = wstring_allocate_from_string(fspath.str, fspath.length);
    result = RemoveDirectoryW(wfpath);
    wstring_deallocate(wfpath);

#elif FOUNDATION_PLATFORM_POSIX

    // Re-terminate string at base path
    abspath_buffer[fspathofs + fspath.length] = 0;
    result = (rmdir(abspath_buffer) == 0);

#elif FOUNDATION_PLATFORM_PNACL

    PP_Resource fs = _fs_resolve_path(fspath.str, fspath.length,
                                      (string_const_t *)&localpath);
    if (!fs)
        return 0;

    char buffer[BUILD_MAX_PATHLEN + 1];
    string_t finalpath =
        string_copy(buffer + 1, sizeof(buffer) - 1, STRING_ARGS(localpath));
    if (finalpath.str[0] != '/') {
        *(--finalpath.str) = '/';
        finalpath.length++;
    }
    PP_Resource ref = _pnacl_file_ref->Create(fs, finalpath.str);
    if (!ref)
        return 0;

    result = (_pnacl_file_ref->Delete(ref, PP_BlockUntilComplete()) == PP_OK);
    _pnacl_core->ReleaseResource(ref);

#else
#error Not implemented
#endif

end:

    return result;
}

bool fs_make_directory(const char *path, size_t length) {
#if FOUNDATION_PLATFORM_PNACL

    string_const_t fspath;
    fspath = _fs_strip_protocol(path, length);

    string_const_t localpath;
    PP_Resource fs = _fs_resolve_path(fspath.str, fspath.length,
                                      (string_const_t *)&localpath);
    if (!fs)
        return false;

    char buffer[BUILD_MAX_PATHLEN + 1];
    string_t finalpath =
        string_copy(buffer + 1, sizeof(buffer) - 1, STRING_ARGS(localpath));
    if (finalpath.str[0] != '/') {
        *(--finalpath.str) = '/';
        finalpath.length++;
    }
    PP_Resource ref = _pnacl_file_ref->Create(fs, finalpath.str);
    if (!ref)
        return false;

    bool result = (_pnacl_file_ref->MakeDirectory(
                       ref, PP_MAKEDIRECTORYFLAG_WITH_ANCESTORS,
                       PP_BlockUntilComplete()) == PP_OK);
    _pnacl_core->ReleaseResource(ref);

    return result;

#else

    bool result = false;
    char abspath_buffer[BUILD_MAX_PATHLEN];
    size_t offset;
    string_t localpath;
    string_const_t fspath;

    localpath =
        string_copy(abspath_buffer, sizeof(abspath_buffer), path, length);
    fspath = _fs_strip_protocol(STRING_ARGS(localpath));
    if (!fspath.length)
        return false;
    localpath = (string_t) {
        localpath.str + pointer_diff(fspath.str, localpath.str), fspath.length
    };
    offset = 1;

#if FOUNDATION_PLATFORM_WINDOWS
    if ((localpath.length > 2) && (localpath.str[1] == ':'))
        offset += 2; // Drive letter
#endif

    do {
        offset = string_find(STRING_ARGS(localpath), '/', offset);
        if (offset != STRING_NPOS) {
            localpath.str[offset] = 0;
            localpath.length = offset;
        }
        if (!fs_is_directory(STRING_ARGS(localpath))) {
#if FOUNDATION_PLATFORM_WINDOWS
            wchar_t *wpath =
                wstring_allocate_from_string(STRING_ARGS(localpath));
            result = CreateDirectoryW(wpath, 0);
            wstring_deallocate(wpath);
#elif FOUNDATION_PLATFORM_POSIX
            mode_t mode = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP |
                          S_IXGRP | S_IROTH | S_IXOTH;
            result = (mkdir(localpath.str, mode) == 0);
#else
#error Not implemented
#endif
            if (!result) {
                int err = system_error();
                string_const_t errmsg = system_error_message(err);
                log_warnf(0, WARNING_SUSPICIOUS,
                          STRING_CONST(
                              "Failed to create directory '%.*s': %.*s (%d)"),
                          STRING_FORMAT(localpath), STRING_FORMAT(errmsg), err);
                goto end;
            }
        } else {
            result = true;
        }

        if (offset != STRING_NPOS) {
            localpath.str[offset] = '/';
            localpath.length = fspath.length;
            ++offset;
        }
    } while (offset < localpath.length);

end:

    return result;

#endif
}

bool fs_copy_file(const char *source, size_t srclen, const char *dest,
                  size_t destlen) {
    stream_t *infile;
    stream_t *outfile;
    void *buffer;
    string_const_t destpath;

    infile = fs_open_file(source, srclen, STREAM_IN | STREAM_BINARY);
    if (!infile)
        return false;

    destpath = path_directory_name(dest, destlen);
    if (destpath.length)
        fs_make_directory(STRING_ARGS(destpath));

    outfile = fs_open_file(dest, destlen,
                           STREAM_OUT | STREAM_BINARY | STREAM_CREATE |
                           STREAM_TRUNCATE);
    if (!outfile) {
        stream_deallocate(infile);
        return false;
    }

    buffer = memory_allocate(0, 64 * 1024, 0, MEMORY_TEMPORARY);

    while (!stream_eos(infile)) {
        size_t numread = stream_read(infile, buffer, 64 * 1024);
        if (numread > 0)
            stream_write(outfile, buffer, numread);
    }

    memory_deallocate(buffer);
    stream_deallocate(infile);
    stream_deallocate(outfile);

    return true;
}

tick_t fs_last_modified(const char *path, size_t length) {
#if FOUNDATION_PLATFORM_WINDOWS

    // This is retarded beyond belief, Microsoft decided that "100-nanosecond
    // intervals since 1 Jan 1601" was  a nice basis for a timestamp... wtf?
    // Anyway, number of such intervals to base date for unix time, 1 Jan 1970,
    // is 116444736000000000
    const int64_t ms_offset_time = 116444736000000000LL;
    tick_t last_write_time;
    uint64_t high_time, low_time;
    wchar_t *wpath;
    WIN32_FILE_ATTRIBUTE_DATA attrib;
    BOOL success = 0;
    string_const_t cpath;
    memset(&attrib, 0, sizeof(attrib));

    cpath = _fs_strip_protocol(path, length);
    if (cpath.length) {
        wpath = wstring_allocate_from_string(STRING_ARGS(cpath));
        success = GetFileAttributesExW(wpath, GetFileExInfoStandard, &attrib);
        wstring_deallocate(wpath);
    }

    /*SYSTEMTIME stime;
    memset( &stime, 0, sizeof( stime ) );
    stime.wYear  = 1970;
    stime.wDay   = 1;
    stime.wMonth = 1;
    SystemTimeToFileTime( &stime, &basetime );
    int64_t ms_offset_time = (*(int64_t*)&basetime);*/

    if (!success)
        return 0;

    high_time = (uint64_t)attrib.ftLastWriteTime.dwHighDateTime;
    low_time = (uint64_t)attrib.ftLastWriteTime.dwLowDateTime;
    last_write_time = (tick_t)((high_time << 32ULL) + low_time);

    return (last_write_time > ms_offset_time)
           ? ((last_write_time - ms_offset_time) / 10000LL)
           : 0;

#elif FOUNDATION_PLATFORM_POSIX

    tick_t tstamp = 0;
    char buffer[BUILD_MAX_PATHLEN];
    struct stat st;
    string_const_t fspath = _fs_strip_protocol(path, length);
    if (fspath.length) {
        memset(&st, 0, sizeof(st));
        string_t finalpath =
            string_copy(buffer, sizeof(buffer), STRING_ARGS(fspath));
        if (stat(finalpath.str, &st) >= 0)
            tstamp = (tick_t)st.st_mtime * 1000LL;
    }
    return tstamp;

#elif FOUNDATION_PLATFORM_PNACL

    tick_t tstamp = 0;
    string_const_t localpath;
    string_const_t pathstr = _fs_strip_protocol(path, length);
    PP_Resource fs = _fs_resolve_path(pathstr.str, pathstr.length, &localpath);
    if (fs) {
        char buffer[BUILD_MAX_PATHLEN + 1];
        string_t finalpath =
            string_copy(buffer + 1, sizeof(buffer) - 1, STRING_ARGS(localpath));
        if (finalpath.str[0] != '/') {
            *(--finalpath.str) = '/';
            finalpath.length++;
        }
        PP_Resource ref = _pnacl_file_ref->Create(fs, finalpath.str);
        if (ref) {
            struct PP_FileInfo info;
            if (_pnacl_file_ref->Query(ref, &info, PP_BlockUntilComplete()) ==
                PP_OK)
                tstamp = (tick_t)info.last_modified_time * 1000LL;

            _pnacl_core->ReleaseResource(ref);
        }
    }

    return tstamp;

#else
#error Not implemented
#endif
}

size_t fs_size(const char *path, size_t length) {
    size_t size = 0;
    stream_t *file = fs_open_file(path, length, STREAM_IN | STREAM_BINARY);
    if (file) {
        size = stream_size(file);
        stream_deallocate(file);
    }
    return size;
}

uint128_t fs_md5(const char *path, size_t length) {
    uint128_t digest = uint128_null();
    stream_t *file = fs_open_file(path, length, STREAM_IN | STREAM_BINARY);
    if (file) {
        digest = stream_md5(file);
        stream_deallocate(file);
    }
    return digest;
}

void fs_touch(const char *path, size_t length) {
#if FOUNDATION_PLATFORM_WINDOWS
    string_const_t cpath = _fs_strip_protocol(path, length);
    if (cpath.length) {
        wchar_t *wpath = wstring_allocate_from_string(STRING_ARGS(cpath));
        _wutime(wpath, 0);
        wstring_deallocate(wpath);
    }
#elif FOUNDATION_PLATFORM_POSIX
    string_const_t fspath = _fs_strip_protocol(path, length);
    if (fspath.length) {
        char buffer[BUILD_MAX_PATHLEN];
        string_t finalpath =
            string_copy(buffer, sizeof(buffer), STRING_ARGS(fspath));
        utime(finalpath.str, 0);
    }
#elif FOUNDATION_PLATFORM_PNACL

    string_const_t localpath;
    string_const_t pathstr = _fs_strip_protocol(path, length);
    PP_Resource fs = _fs_resolve_path(pathstr.str, pathstr.length, &localpath);
    if (fs) {
        char buffer[BUILD_MAX_PATHLEN + 1];
        string_t finalpath =
            string_copy(buffer + 1, sizeof(buffer) - 1, STRING_ARGS(localpath));
        if (finalpath.str[0] != '/') {
            *(--finalpath.str) = '/';
            finalpath.length++;
        }
        PP_Resource ref = _pnacl_file_ref->Create(fs, finalpath.str);
        if (ref) {
            PP_Time tstamp = (PP_Time)(time_system() / 1000LL);
            _pnacl_file_ref->Touch(ref, tstamp, tstamp,
                                   PP_BlockUntilComplete());
            _pnacl_core->ReleaseResource(ref);
        }
    }

#else
#error Not implemented
#endif
}

stream_t *fs_temporary_file(void) {
    char buf[BUILD_MAX_PATHLEN];
    string_t filename = path_make_temporary(buf, BUILD_MAX_PATHLEN);
    string_const_t directory =
        path_directory_name(filename.str, filename.length);

    fs_make_directory(directory.str, directory.length);
    return fs_open_file(filename.str, filename.length,
                        STREAM_IN | STREAM_OUT | STREAM_BINARY | STREAM_CREATE |
                        STREAM_TRUNCATE);
}

string_t *fs_matching_files_regex(const char *path, size_t length,
                                  regex_t *pattern, bool recurse) {
    string_t *names = 0;
    string_t *subdirs;
    string_t localpath;
    size_t id, dsize, in, nsize, capacity;

#if FOUNDATION_PLATFORM_WINDOWS

    // Windows specific implementation of directory listing
    WIN32_FIND_DATAW data;
    char filename[BUILD_MAX_PATHLEN];
    wchar_t *wpattern;
    size_t wsize = length;
    capacity = length + 4;

    memory_context_push(HASH_STREAM);

    wpattern =
        memory_allocate(0, sizeof(wchar_t) * capacity, 0, MEMORY_TEMPORARY);
    wstring_from_string(wpattern, capacity, path, length);
    if (length && (path[length - 1] != '/'))
        wpattern[wsize++] = L'/';
    wpattern[wsize++] = L'*';
    wpattern[wsize] = 0;

    HANDLE find = FindFirstFileW(wpattern, &data);

    if (find != INVALID_HANDLE_VALUE)
        do {
            if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                string_t filestr =
                    string_convert_utf16(filename, BUILD_MAX_PATHLEN,
                                         (const uint16_t *)data.cFileName,
                                         wstring_length(data.cFileName));
                if (regex_match(pattern, STRING_ARGS(filestr), 0, 0)) {
                    filestr = string_clone(STRING_ARGS(filestr));
                    array_push(names, path_clean(STRING_ARGS(filestr), false));
                }
            }
        } while (FindNextFileW(find, &data));

    memory_deallocate(wpattern);
    memory_context_pop();

    FindClose(find);

#else

    string_t *fnames = fs_files(path, length);

    memory_context_push(HASH_STREAM);

    for (in = 0, nsize = array_size(fnames); in < nsize; ++in) {
        if (regex_match(pattern, STRING_ARGS(fnames[in]), 0, 0)) {
            array_push(names, fnames[in]);
            fnames[in] = (string_t) {
                0, 0
            };
        }
    }

    memory_context_pop();

    string_array_deallocate(fnames);

#endif

    if (!recurse)
        return names;

    subdirs = fs_subdirs(path, length);

    memory_context_push(HASH_STREAM);

    capacity = BUILD_MAX_PATHLEN;
    localpath = string_allocate(0, capacity);
    localpath = string_copy(localpath.str, BUILD_MAX_PATHLEN, path, length);

    /*lint --e{438,613,838} Lint gets seriously confused about the arrays here
     */
    for (id = 0, dsize = array_size(subdirs); id < dsize; ++id) {
        string_t *subnames;
        localpath = path_append(localpath.str, length, capacity,
                                STRING_ARGS(subdirs[id]));
        subnames =
            fs_matching_files_regex(STRING_ARGS(localpath), pattern, true);

        for (in = 0, nsize = array_size(subnames); in < nsize; ++in)
            array_push(names, path_allocate_concat(STRING_ARGS(subdirs[id]),
                                                   STRING_ARGS(subnames[in])));

        string_array_deallocate(subnames);
    }

    string_deallocate(localpath.str);
    string_array_deallocate(subdirs);

    memory_context_pop();

    return names;
}

string_t *fs_matching_files(const char *path, size_t length,
                            const char *pattern, size_t pattern_length,
                            bool recurse) {
    regex_t *regex = regex_compile(pattern, pattern_length);
    string_t *names = fs_matching_files_regex(path, length, regex, recurse);
    regex_deallocate(regex);
    return names;
}

void fs_event_post(foundation_event_id id, const char *path, size_t pathlen) {
    event_post_varg(fs_event_stream(), id, 0, 0, &pathlen, sizeof(pathlen),
                    path, pathlen, nullptr);
}

string_const_t fs_event_path(const event_t *event) {
    return string_const(pointer_offset_const(event->payload, sizeof(size_t)),
                        event->payload[0]);
}

event_stream_t *fs_event_stream(void) {
    return _fs_event_stream;
}

#if FOUNDATION_PLATFORM_LINUX || FOUNDATION_PLATFORM_ANDROID

struct fs_watch_t {
    int fd;
    string_t path;
};

typedef struct fs_watch_t fs_watch_t;

static void _fs_send_creations(char *path, size_t length, size_t capacity) {
    size_t ifile, isub, fsize, subsize;

    string_t *files = fs_files(path, length);
    for (ifile = 0, fsize = array_size(files); ifile < fsize; ++ifile) {
        string_t filepath =
            path_append(path, length, capacity, STRING_ARGS(files[ifile]));
        fs_event_post(FOUNDATIONEVENT_FILE_CREATED, STRING_ARGS(filepath));
    }
    string_array_deallocate(files);

    string_t *subdirs = fs_subdirs(path, length);
    for (isub = 0, subsize = array_size(subdirs); isub < subsize; ++isub) {
        string_t subpath =
            path_append(path, length, capacity, STRING_ARGS(subdirs[isub]));
        _fs_send_creations(STRING_ARGS(subpath), capacity);
    }
    string_array_deallocate(subdirs);
}

static void _fs_add_notify_subdir(int notify_fd, char *path, size_t length,
                                  size_t capacity, fs_watch_t **watch_arr,
                                  string_t **path_arr, bool send_create) {
    string_t *subdirs = 0;
    string_t local_path;
    string_t stored_path;
    int fd = inotify_add_watch(notify_fd, path,
                               IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVE);
    if (fd < 0) {
        log_warnf(0, WARNING_SYSTEM_CALL_FAIL,
                  STRING_CONST("Failed watching subdir: %.*s (%d)"),
                  (int)length, path, fd);
        return;
    }

    if (send_create)
        _fs_send_creations(path, length, capacity);

    // Include terminating / in paths stored in path_arr/watch_arr
    local_path = string_append(path, length, capacity, STRING_CONST("/"));
    stored_path = string_clone(STRING_ARGS(local_path));
    array_push((*path_arr), stored_path);

    fs_watch_t watch;
    watch.fd = fd;
    watch.path = stored_path;
    array_push((*watch_arr), watch);

    // Recurse
    subdirs = fs_subdirs(STRING_ARGS(local_path));
    for (size_t i = 0, size = array_size(subdirs); i < size; ++i) {
        string_t subpath = string_append(STRING_ARGS(local_path), capacity,
                                         STRING_ARGS(subdirs[i]));
        _fs_add_notify_subdir(notify_fd, STRING_ARGS(subpath), capacity,
                              watch_arr, path_arr, send_create);
    }
    string_array_deallocate(subdirs);
}

static fs_watch_t *_fs_lookup_watch(fs_watch_t *watch_arr, int fd) {
    // TODO: If array is kept sorted on fd, this could be made faster
    for (size_t i = 0, size = array_size(watch_arr); i < size; ++i) {
        if (watch_arr[i].fd == fd)
            return watch_arr + i;
    }
    return 0;
}

#elif FOUNDATION_PLATFORM_MACOS

extern void *_fs_event_stream_create(const char *path, size_t length);

extern void _fs_event_stream_destroy(void *stream);

extern void _fs_event_stream_flush(void *stream);

#endif

#if FOUNDATION_HAVE_FS_MONITOR

static void *_fs_monitor(void *monitorptr) {
    fs_monitor_t *monitor = monitorptr;
    bool keep_running = true;

#if FOUNDATION_PLATFORM_WINDOWS

    DWORD buffer_size = 63 * 1024;
    DWORD out_size = 0;
    OVERLAPPED overlap;
    BOOL success = FALSE;
    HANDLE dir = 0;
    wchar_t *wfpath = 0;
    void *buffer;
    HANDLE handle;
    int event;
    int wait_status;
    beacon_t *beacon = &thread_self()->beacon;

    memory_context_push(HASH_STREAM);

    buffer = memory_allocate(0, buffer_size, 8, MEMORY_PERSISTENT);
    handle = CreateEvent(0, FALSE, FALSE, 0);
    if (handle == INVALID_HANDLE_VALUE) {
        string_const_t errstr = system_error_message(0);
        log_warnf(
            0, WARNING_SUSPICIOUS,
            STRING_CONST("Unable to create event to monitor path: %.*s : %.*s"),
            STRING_FORMAT(monitor->path), STRING_FORMAT(errstr));
        goto exit_thread;
    }

    event = beacon_add_handle(beacon, handle);

#elif FOUNDATION_PLATFORM_LINUX || FOUNDATION_PLATFORM_ANDROID

    char pathbuffer[BUILD_MAX_PATHLEN];
    string_t local_path;
    int notify_fd = inotify_init();
    fs_watch_t *watch = 0;
    string_t *paths = 0;
    beacon_t *beacon = &thread_self()->beacon;

    memory_context_push(HASH_STREAM);

    array_reserve(watch, 1024);

    // Recurse and add all subdirs
    local_path =
        string_copy(pathbuffer, sizeof(pathbuffer), STRING_ARGS(monitor->path));
    _fs_add_notify_subdir(notify_fd, STRING_ARGS(local_path),
                          sizeof(pathbuffer), &watch, &paths, false);

    beacon_add_fd(beacon, notify_fd);

#elif FOUNDATION_PLATFORM_MACOS

    memory_context_push(HASH_STREAM);

    void *event_stream = _fs_event_stream_create(STRING_ARGS(monitor->path));

#else

    memory_context_push(HASH_STREAM);

#endif

    // log_debugf(0, STRING_CONST("Monitoring file system: %.*s"),
    //           STRING_FORMAT(monitor->path));

#if FOUNDATION_PLATFORM_WINDOWS
    {
        wfpath = wstring_allocate_from_string(STRING_ARGS(monitor->path));
        dir =
            CreateFileW(wfpath, FILE_LIST_DIRECTORY,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        0, OPEN_EXISTING,
                        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, 0);
        wstring_deallocate(wfpath);
    }
    if (dir == INVALID_HANDLE_VALUE) {
        string_const_t errstr = system_error_message(0);
        log_warnf(0, WARNING_SUSPICIOUS,
                  STRING_CONST("Unable to open handle for path: %.*s : %.*s"),
                  STRING_FORMAT(monitor->path), STRING_FORMAT(errstr));
        goto exit_thread;
    }
#endif

    while (keep_running) {
#if FOUNDATION_PLATFORM_WINDOWS
        DWORD transferred;

        memset(&overlap, 0, sizeof(overlap));
        overlap.hEvent = handle;

        out_size = 0;
        success = ReadDirectoryChangesW(dir, buffer, buffer_size, TRUE,
                                        FILE_NOTIFY_CHANGE_FILE_NAME |
                                        FILE_NOTIFY_CHANGE_SIZE |
                                        FILE_NOTIFY_CHANGE_LAST_WRITE,
                                        &out_size, &overlap, 0);
        if (!success) {
            string_const_t errstr = system_error_message(0);
            log_warnf(
                0, WARNING_SUSPICIOUS,
                STRING_CONST(
                    "Unable to read directory changes for path: %.*s : %.*s"),
                STRING_FORMAT(monitor->path), STRING_FORMAT(errstr));
            goto exit_thread;
        }

        wait_status = beacon_wait(beacon);

        if (wait_status <= 0) {
            // Thread signalled or error
            keep_running = false;
        } else if (wait_status == event) {
            // File system change
            transferred = 0;
            success = GetOverlappedResult(dir, &overlap, &transferred, FALSE);
            if (!success) {
                string_const_t errstr = system_error_message(0);
                log_warnf(0, WARNING_SUSPICIOUS,
                          STRING_CONST("Unable to read directory changes for "
                                       "path: %.*s : %.*s"),
                          STRING_FORMAT(monitor->path), STRING_FORMAT(errstr));
            } else {
                PFILE_NOTIFY_INFORMATION info = buffer;
                do {
                    size_t numchars = info->FileNameLength / sizeof(wchar_t);
                    wchar_t term = info->FileName[numchars];
                    string_t utfstr;
                    string_t fullpath;

                    info->FileName[numchars] = 0;
                    utfstr = string_allocate_from_wstring(
                                 info->FileName, wstring_length(info->FileName));
                    utfstr = path_clean(STRING_ARGS_CAPACITY(utfstr));
                    fullpath = path_allocate_concat(STRING_ARGS(monitor->path),
                                                    STRING_ARGS(utfstr));

                    if (fs_is_directory(STRING_ARGS(fullpath))) {
                        // Ignore directory changes
                    } else {
                        foundation_event_id fsevent = FOUNDATIONEVENT_NOEVENT;
                        switch (info->Action) {
                        case FILE_ACTION_ADDED:
                            fsevent = FOUNDATIONEVENT_FILE_CREATED;
                            break;
                        case FILE_ACTION_REMOVED:
                            fsevent = FOUNDATIONEVENT_FILE_DELETED;
                            break;
                        case FILE_ACTION_MODIFIED:
                            if (fs_is_file(STRING_ARGS(fullpath)))
                                fsevent = FOUNDATIONEVENT_FILE_MODIFIED;
                            break;

                        // Treat rename as delete/add pair
                        case FILE_ACTION_RENAMED_OLD_NAME:
                            fsevent = FOUNDATIONEVENT_FILE_DELETED;
                            break;
                        case FILE_ACTION_RENAMED_NEW_NAME:
                            fsevent = FOUNDATIONEVENT_FILE_CREATED;
                            break;

                        default:
                            break;
                        }

                        if (fsevent)
                            fs_event_post(fsevent, STRING_ARGS(fullpath));
                    }
                    string_deallocate(utfstr.str);
                    string_deallocate(fullpath.str);

                    info->FileName[numchars] = term;

                    info = info->NextEntryOffset
                           ? (PFILE_NOTIFY_INFORMATION)(pointer_offset(
                                                            info, info->NextEntryOffset))
                           : nullptr;
                } while (info);
            }
        }

#elif FOUNDATION_PLATFORM_LINUX || FOUNDATION_PLATFORM_ANDROID

        int avail = 0;
        if (beacon_wait(beacon) == 0)
            keep_running = false;
        else
            ioctl(notify_fd, FIONREAD, &avail);
        if (avail > 0) {
            void *buffer =
                memory_allocate(HASH_STREAM, (size_t)avail + 4, 8,
                                MEMORY_PERSISTENT | MEMORY_ZERO_INITIALIZED);
            int offset = 0;
            ssize_t avail_read = read(notify_fd, buffer, (size_t)avail);
            struct inotify_event *event = (struct inotify_event *)buffer;
            while (offset < avail_read) {
                fs_watch_t *curwatch = _fs_lookup_watch(watch, event->wd);
                if (!curwatch) {
                    log_warnf(0, WARNING_SUSPICIOUS,
                              STRING_CONST(
                                  "inotify watch not found: %d %x %x %" PRIsize
                                  " bytes: %.*s"),
                              event->wd, event->mask, event->cookie,
                              (size_t)event->len, (int)event->len,
                              (const char *)event->name);
                    goto skipwatch;
                }

                string_t curpath = string_copy(pathbuffer, sizeof(pathbuffer),
                                               STRING_ARGS(curwatch->path));
                curpath =
                    string_append(STRING_ARGS(curpath), sizeof(pathbuffer),
                                  event->name, string_length(event->name));

                bool is_dir = ((event->mask & IN_ISDIR) != 0);

                if ((event->mask & IN_CREATE) || (event->mask & IN_MOVED_TO)) {
                    if (is_dir)
                        _fs_add_notify_subdir(notify_fd, STRING_ARGS(curpath),
                                              sizeof(pathbuffer), &watch,
                                              &paths, true);
                    else
                        fs_event_post(FOUNDATIONEVENT_FILE_CREATED,
                                      STRING_ARGS(curpath));
                }
                if ((event->mask & IN_DELETE) ||
                    (event->mask & IN_MOVED_FROM)) {
                    if (!is_dir)
                        fs_event_post(FOUNDATIONEVENT_FILE_DELETED,
                                      STRING_ARGS(curpath));
                }
                if (event->mask & IN_MODIFY) {
                    if (!is_dir)
                        fs_event_post(FOUNDATIONEVENT_FILE_MODIFIED,
                                      STRING_ARGS(curpath));
                }
                /* Moved events are also notified as CREATE/DELETE with cookies,
                so ignore for now if (event->mask & IN_MOVED_FROM) if
                (event->mask & IN_MOVED_TO)*/

skipwatch:
                offset += event->len + sizeof(struct inotify_event);
                event = (struct inotify_event *)pointer_offset(buffer, offset);
            }
            memory_deallocate(buffer);
        }

#elif FOUNDATION_PLATFORM_MACOS

        if (event_stream)
            _fs_event_stream_flush(event_stream);

        if (thread_try_wait(100))
            keep_running = false;

        //#elif FOUNDATION_PLATFORM_BSD
        //  TODO: Implement using kqueue and directory watching using open
        //  with O_EVTONLY
        //        https://github.com/emcrisostomo/fswatch/blob/master/libfswatch/src/libfswatch/c%2B%2B/kqueue_monitor.cpp

#else
        // log_debug(0, STRING_CONST("Filesystem watcher not implemented on this
        // platform"));  Not implemented yet, just wait for signal to simulate
        // thread
        thread_wait();
        keep_running = false;
#endif
    }

    // log_debugf(0, STRING_CONST("Stopped monitoring file system: %.*s"),
    //           STRING_FORMAT(monitor->path));

#if FOUNDATION_PLATFORM_WINDOWS

exit_thread:

    CloseHandle(dir);

    if (handle != INVALID_HANDLE_VALUE)
        CloseHandle(handle);

    memory_deallocate(buffer);

#elif FOUNDATION_PLATFORM_LINUX || FOUNDATION_PLATFORM_ANDROID

    close(notify_fd);
    string_array_deallocate(paths);
    array_deallocate(watch);

#elif FOUNDATION_PLATFORM_MACOS

    _fs_event_stream_destroy(event_stream);

#endif

    memory_context_pop();

    return 0;
}

#endif

static fs_file_descriptor _fs_file_fopen(const char *path, size_t length,
        unsigned int mode, bool *dotrunc) {
    fs_file_descriptor fd = 0;

#if FOUNDATION_PLATFORM_PNACL
    FOUNDATION_UNUSED(dotrunc);

    if (!(mode & (STREAM_IN | STREAM_OUT)))
        return 0;

    string_const_t localpath;
    string_const_t pathstr = _fs_strip_protocol(path, length);
    PP_Resource fs = _fs_resolve_path(pathstr.str, pathstr.length, &localpath);
    if (!fs)
        return 0;

    char buffer[BUILD_MAX_PATHLEN + 1];
    string_t finalpath =
        string_copy(buffer + 1, sizeof(buffer) - 1, STRING_ARGS(localpath));
    if (finalpath.str[0] != '/') {
        *(--finalpath.str) = '/';
        finalpath.length++;
    }
    PP_Resource ref = _pnacl_file_ref->Create(fs, finalpath.str);
    if (!ref)
        return 0;

    fd = _pnacl_file_io->Create(pnacl_instance());
    if (fd) {
        int flags = 0;

        if (mode & STREAM_IN)
            flags |= PP_FILEOPENFLAG_READ;
        if (mode & STREAM_OUT) {
            flags |= PP_FILEOPENFLAG_WRITE;
            if (mode & STREAM_TRUNCATE)
                flags |= PP_FILEOPENFLAG_TRUNCATE;
        }
        if (mode & STREAM_CREATE)
            flags |= PP_FILEOPENFLAG_CREATE;

        int res = _pnacl_file_io->Open(fd, ref, flags, PP_BlockUntilComplete());
        if (res != PP_OK) {
            _pnacl_file_io->Close(fd);
            _pnacl_core->ReleaseResource(fd);
            fd = 0;
        }
    }

    _pnacl_core->ReleaseResource(ref);

#else

#if FOUNDATION_PLATFORM_WINDOWS
#define MODESTRING(x) L##x
    const wchar_t *modestr;
    wchar_t *wpath;
#elif FOUNDATION_PLATFORM_LINUX
#define MODESTRING(x) x "e"
    const char *modestr;
#else
#define MODESTRING(x) x
    const char *modestr;
#endif
    int retry = 0;

    if (mode & STREAM_IN) {
        if (mode & STREAM_OUT) {
            if (mode & STREAM_CREATE) {
                if (mode & STREAM_TRUNCATE)
                    modestr = MODESTRING("w+b");
                else {
                    modestr = MODESTRING("r+b");
                    retry = 1;
                }
            } else {
                modestr = MODESTRING("r+b");
                if ((mode & STREAM_TRUNCATE) && dotrunc)
                    *dotrunc = true;
            }
        } else {
            // truncate is ignored for read-only files
            if (mode & STREAM_CREATE) {
                modestr = MODESTRING("r+b");
                retry = 1;
            } else
                modestr = MODESTRING("rb");
        }
    } else if (mode & STREAM_OUT) {
        if (mode & STREAM_TRUNCATE) {
            if (mode & STREAM_CREATE)
                modestr = MODESTRING("w+b");
            else {
                modestr = MODESTRING("r+b");
                if (dotrunc)
                    *dotrunc = true;
            }
        } else {
            modestr = MODESTRING("r+b");
            if (mode & STREAM_CREATE)
                retry = 1;
        }
    } else
        return 0;

    do {
#if FOUNDATION_PLATFORM_WINDOWS
        wpath = wstring_allocate_from_string(path, length);
        fd = _wfsopen(wpath, modestr,
                      (mode & STREAM_OUT) ? _SH_DENYWR : _SH_DENYNO);
        wstring_deallocate(wpath);
#elif FOUNDATION_PLATFORM_POSIX
        FOUNDATION_UNUSED(length);
        fd = fopen(path, modestr);
#else
#error Not implemented
#endif
        // In case retry is set, it's because we want to create a file if it
        // does not exist,  but not truncate existing file, while still not using
        // append mode since that fixes  writing to end of file. Try first with
        // r+b to avoid truncation, then if it fails  i.e file does not exist,
        // create it with w+b
        modestr = MODESTRING("w+b");
    } while (!fd && (retry-- > 0));

    if (fd && (mode & STREAM_ATEND)) {
        if (fseek(fd, 0, SEEK_END))
            log_warnf(0, WARNING_SYSTEM_CALL_FAIL,
                      STRING_CONST("Unable to seek to end of stream '%.*s'"),
                      (int)length, path);
    }

#endif

    return fd;
}

static size_t _fs_file_tell(stream_t *stream) {
    if (GET_FILE(stream)->fd == 0)
        return 0;
#if FOUNDATION_PLATFORM_PNACL
    return GET_FILE(stream)->position;
#elif FOUNDATION_PLATFORM_WINDOWS &&                                           \
    (FOUNDATION_COMPILER_MSVC || FOUNDATION_COMPILER_INTEL)
    int64_t pos = _ftelli64(GET_FILE(stream)->fd);
    return (size_t)(pos < 0 ? 0 : pos);
#elif FOUNDATION_PLATFORM_WINDOWS
    fpos_t pos;
    if (fgetpos(GET_FILE(stream)->fd, &pos))
        return 0;
    return (size_t)(pos < 0 ? 0 : pos);
#else
    off_t pos = ftello(GET_FILE(stream)->fd);
    return (size_t)(pos < 0 ? 0 : pos);
#endif
}

static void _fs_file_seek(stream_t *stream, ssize_t offset,
                          stream_seek_mode_t direction) {
#if FOUNDATION_PLATFORM_PNACL
    stream_file_t *file = GET_FILE(stream);
    if (direction == STREAM_SEEK_BEGIN)
        file->position = offset > 0 ? (size_t)offset : 0;
    else if (direction == STREAM_SEEK_END)
        file->position = file->size - (offset < 0 ? (size_t) - offset : 0);
    else {
        if (offset > 0)
            file->position += (size_t)offset;
        else
            file->position -= (size_t) - offset;
    }
    if (file->position > file->size) {
        if (((stream->mode & STREAM_OUT) != 0) &&
            (_pnacl_file_io->SetLength(file->fd, file->position,
                                       PP_BlockUntilComplete()) == PP_OK)) {
            file->size = file->position;
            _pnacl_file_io->Flush(file->fd, PP_BlockUntilComplete());
        } else {
            file->position = file->size;
        }
    }
#else
    /*lint -esym(970,long) */
    if (fseek(GET_FILE(stream)->fd, (long)offset,
              (direction == STREAM_SEEK_BEGIN)
              ? SEEK_SET
              : ((direction == STREAM_SEEK_END) ? SEEK_END : SEEK_CUR)))
        log_warnf(0, WARNING_SYSTEM_CALL_FAIL,
                  STRING_CONST("Unable to seek to %d:%d in stream '%.*s'"),
                  (int)offset, (int)direction, STRING_FORMAT(stream->path));
#endif
}

static bool _fs_file_eos(stream_t *stream) {
#if FOUNDATION_PLATFORM_PNACL
    stream_file_t *file = GET_FILE(stream);
    return (file->position >= file->size);
#else
    return (GET_FILE(stream)->fd == 0) || (feof(GET_FILE(stream)->fd) != 0);
#endif
}

static size_t _fs_file_size(stream_t *stream) {
#if FOUNDATION_PLATFORM_PNACL
    return GET_FILE(stream)->size;
#else
    size_t cur, size;

    cur = _fs_file_tell(stream);
    _fs_file_seek(stream, 0, STREAM_SEEK_END);
    size = _fs_file_tell(stream);
    _fs_file_seek(stream, (ssize_t)cur, STREAM_SEEK_BEGIN);

    return size;
#endif
}

static void _fs_file_truncate(stream_t *stream, size_t length) {
    stream_file_t *file;
    string_const_t fspath;
    size_t cur;
#if FOUNDATION_PLATFORM_WINDOWS
    HANDLE fd;
    wchar_t *wpath;
    bool success = false;
#endif

    if (!(stream->mode & STREAM_OUT) || (GET_FILE(stream)->fd == 0))
        return;

    if (length >= _fs_file_size(stream))
        return;

    cur = _fs_file_tell(stream);
    if (cur > length)
        cur = length;

#if FOUNDATION_PLATFORM_PNACL
    FOUNDATION_UNUSED(length);
    FOUNDATION_UNUSED(fspath);
    file = GET_FILE(stream);

    if (_pnacl_file_io->SetLength(file->fd, 0, PP_BlockUntilComplete()) ==
        PP_OK) {
        file->size = 0;
        file->position = 0;
        _pnacl_file_io->Flush(file->fd, PP_BlockUntilComplete());
    }

    file->size = length;
    file->position = cur;

#else

    file = GET_FILE(stream);
    fspath = _fs_strip_protocol(STRING_ARGS(file->path));
    if (!fspath.length)
        return;

    fclose(file->fd);
    file->fd = 0;

#if FOUNDATION_PLATFORM_WINDOWS
    wpath = wstring_allocate_from_string(STRING_ARGS(fspath));
    fd = CreateFileW(wpath, GENERIC_WRITE, FILE_SHARE_DELETE, 0, OPEN_EXISTING,
                     0, 0);
    wstring_deallocate(wpath);
    if (fd != INVALID_HANDLE_VALUE) {
#if FOUNDATION_ARCH_X86_64
        if (length < 0xFFFFFFFF)
#endif
        {
            success = (SetFilePointer(fd, (LONG)length, 0, FILE_BEGIN) !=
                       INVALID_SET_FILE_POINTER);
        }
#if FOUNDATION_ARCH_X86_64
        else {
            LONG high = (LONG)(length >> 32LL);
            success = (SetFilePointer(fd, (LONG)length, &high, FILE_BEGIN) !=
                       INVALID_SET_FILE_POINTER);
        }
#endif
        if (success)
            success = (SetEndOfFile(fd) != 0);
        CloseHandle(fd);
    }
    if (!success) {
        string_const_t errstr = system_error_message(0);
        log_warnf(0, WARNING_SUSPICIOUS,
                  STRING_CONST("Unable to truncate real file %.*s (%" PRIsize
                               " bytes): %.*s"),
                  STRING_FORMAT(fspath), length, STRING_FORMAT(errstr));
    }
#elif FOUNDATION_PLATFORM_POSIX
    int fd = open(fspath.str, O_RDWR);
    if (ftruncate(fd, (ssize_t)length) < 0) {
        int err = system_error();
        string_const_t errmsg = system_error_message(err);
        log_warnf(0, WARNING_SUSPICIOUS,
                  STRING_CONST("Unable to truncate real file %.*s (%" PRIsize
                               " bytes): %.*s (%d)"),
                  STRING_FORMAT(fspath), length, STRING_FORMAT(errmsg), err);
    }
    close(fd);
#else
#error Not implemented
#endif

    file->fd = _fs_file_fopen(fspath.str, fspath.length, stream->mode, 0);
    _fs_file_seek(stream, (ssize_t)cur, STREAM_SEEK_BEGIN);

    // FOUNDATION_ASSERT( file_size( file ) == length );
#endif
}

static void _fs_file_flush(stream_t *stream) {
    if (GET_FILE(stream)->fd == 0)
        return;

#if FOUNDATION_PLATFORM_PNACL
    _pnacl_file_io->Flush(GET_FILE(stream)->fd, PP_BlockUntilComplete());
#else
    fflush(GET_FILE(stream)->fd);
#endif
}

static size_t _fs_file_read(stream_t *stream, void *buffer, size_t num_bytes) {
    stream_file_t *file;
    size_t was_read;
#if !FOUNDATION_PLATFORM_PNACL
    size_t beforepos;
#endif

    if (!(stream->mode & STREAM_IN) || (GET_FILE(stream)->fd == 0))
        return 0;

    file = GET_FILE(stream);

#if FOUNDATION_PLATFORM_PNACL

    size_t available = file->size - file->position;
    if (!available || !num_bytes)
        return 0;
    if (available > 0x7FFFFFFFULL)
        available = 0x7FFFFFFFULL;

    int32_t read = _pnacl_file_io->Read(
                       file->fd, file->position, buffer,
                       (available < num_bytes) ? (int32_t)available : (int32_t)num_bytes,
                       PP_BlockUntilComplete());
    if (read == 0) {
        was_read = (file->size - file->position);
        file->position = file->size;
    } else if (read > 0) {
        was_read = (size_t)read;
        file->position += was_read;
    } else
        was_read = 0;

    return was_read;

#else

    beforepos = _fs_file_tell(stream);
    was_read = fread(buffer, 1, num_bytes, file->fd);
    if (was_read > 0)
        return was_read;

    if (feof(file->fd)) {
        size_t newpos = _fs_file_tell(stream);
        if (newpos > beforepos)
            return (newpos - beforepos);
    }

    return 0;

#endif
}

static size_t _fs_file_write(stream_t *stream, const void *buffer,
                             size_t num_bytes) {
    stream_file_t *file;
    size_t was_written;
#if !FOUNDATION_PLATFORM_PNACL
    size_t beforepos;
#endif

    if (!(stream->mode & STREAM_OUT) || (GET_FILE(stream)->fd == 0))
        return 0;

    file = GET_FILE(stream);

#if FOUNDATION_PLATFORM_PNACL

    if (num_bytes > 0x7FFFFFFFULL)
        num_bytes = 0x7FFFFFFFULL;

    if (file->position + num_bytes > file->size) {
        if (_pnacl_file_io->SetLength(file->fd, file->size,
                                      PP_BlockUntilComplete()) == PP_OK)
            file->size = file->position + num_bytes;
    }

    int32_t written =
        _pnacl_file_io->Write(file->fd, file->position, buffer,
                              (int32_t)num_bytes, PP_BlockUntilComplete());
    if (written == 0) {
        was_written = (file->size - file->position);
        file->position = file->size;
    } else if (written > 0) {
        was_written = (size_t)written;
        file->position += was_written;
    } else
        was_written = 0;

    return was_written;

#else

    beforepos = _fs_file_tell(stream);
    was_written = fwrite(buffer, 1, num_bytes, file->fd);
    if (was_written > 0)
        return was_written;

    if (feof(file->fd)) {
        size_t newpos = _fs_file_tell(stream);
        if (newpos > beforepos)
            return (newpos - beforepos);
    }

    return 0;

#endif
}

static tick_t _fs_file_last_modified(const stream_t *stream) {
    const stream_file_t *fstream = GET_FILE_CONST(stream);
#if FOUNDATION_PLATFORM_PNACL
    struct PP_FileInfo info;
    if (_pnacl_file_io->Query(fstream->fd, &info, PP_BlockUntilComplete()) ==
        PP_OK)
        return (tick_t)info.last_modified_time * 1000LL;
    return 0;
#else
    return fs_last_modified(fstream->path.str, fstream->path.length);
#endif
}

static size_t _fs_file_available_read(stream_t *stream) {
    size_t size = _fs_file_size(stream);
    size_t cur = _fs_file_tell(stream);

    if (size > cur)
        return size - cur;

    return 0;
}

static stream_t *_fs_file_clone(stream_t *stream) {
    stream_file_t *file = GET_FILE(stream);
    return fs_open_file(file->path.str, file->path.length, file->mode);
}

static void _fs_file_finalize(stream_t *stream) {
    stream_file_t *file = GET_FILE(stream);
    if (file->fd == 0)
        return;

    if (file->mode & STREAM_SYNC) {
        _fs_file_flush(stream);
        if (file->fd) {
#if FOUNDATION_PLATFORM_WINDOWS
            _commit(_fileno(file->fd));
#elif FOUNDATION_PLATFORM_MACOS
            fcntl(fileno(file->fd), F_FULLFSYNC, 0);
#elif FOUNDATION_PLATFORM_POSIX
            fsync(fileno(file->fd));
#elif FOUNDATION_PLATFORM_PNACL
            _pnacl_file_io->Flush(file->fd, PP_BlockUntilComplete());
#else
#error Not implemented
#endif
        }
    }

    if (file->fd) {
#if FOUNDATION_PLATFORM_PNACL
        _pnacl_file_io->Close(file->fd);
        _pnacl_core->ReleaseResource(file->fd);
#else
        fclose(file->fd);
#endif
    }
    file->fd = 0;
}

stream_t *fs_open_file(const char *path, size_t length, unsigned int mode) {
    stream_file_t *file;
    fs_file_descriptor fd;
    stream_t *stream;
    string_const_t fspath;
    string_t localpath;
    string_t finalpath;
    size_t capacity;
    bool dotrunc;
    char buffer[BUILD_MAX_PATHLEN];

    capacity = sizeof(buffer);
    localpath = string_copy(buffer, capacity, path, length);
    localpath = path_clean(STRING_ARGS(localpath), capacity);
    if (!path_is_absolute(STRING_ARGS(localpath)))
        localpath = path_absolute(STRING_ARGS(localpath), capacity);

    capacity = localpath.length + 8;
    finalpath = string_allocate(0, capacity);
    finalpath = string_copy(finalpath.str, capacity, STRING_ARGS(localpath));
    if (string_find_string(STRING_ARGS(finalpath), STRING_CONST("://"), 0) ==
        STRING_NPOS) {
        if (finalpath.str[0] == '/')
            finalpath = string_prepend(STRING_ARGS(finalpath), capacity,
                                       STRING_CONST("file:/"));
        else
            finalpath = string_prepend(STRING_ARGS(finalpath), capacity,
                                       STRING_CONST("file://"));
    }

    dotrunc = false;

    fspath = _fs_strip_protocol(STRING_ARGS(finalpath));
    fd = _fs_file_fopen(STRING_ARGS(fspath), mode, &dotrunc);
    if (!fd) {
        string_deallocate(finalpath.str);
        return 0;
    }

    file = memory_allocate(HASH_STREAM, sizeof(stream_file_t), 8,
                           MEMORY_PERSISTENT | MEMORY_ZERO_INITIALIZED);
    stream = GET_STREAM(file);
    stream_initialize(stream, BUILD_DEFAULT_STREAM_BYTEORDER);

    file->fd = fd;
    file->type = STREAMTYPE_FILE;
    file->mode = mode & (STREAM_OUT | STREAM_IN | STREAM_BINARY | STREAM_SYNC);
    file->path = finalpath;
    file->vtable = &_fs_file_vtable;

#if FOUNDATION_PLATFORM_PNACL
    struct PP_FileInfo fileinfo;
    if (_pnacl_file_io->Query(file->fd, &fileinfo, PP_BlockUntilComplete()) ==
        PP_OK)
        file->size = (fileinfo.size > 0) ? (size_t)fileinfo.size : 0;
    else
        file->size = 0;
#endif

    if (dotrunc)
        _fs_file_truncate(stream, 0);
    else if (mode & STREAM_ATEND)
        stream_seek(stream, 0, STREAM_SEEK_END);

    return stream;
}

int _fs_initialize(void) {
#if FOUNDATION_HAVE_FS_MONITOR
    _fs_monitors = memory_allocate(
                       HASH_STREAM, sizeof(fs_monitor_t) * foundation_config().fs_monitor_max,
                       0, MEMORY_PERSISTENT | MEMORY_ZERO_INITIALIZED);
    _fs_monitor_lock = mutex_allocate(STRING_CONST("fs_monitors"));
#else
    _fs_monitors = 0;
    _fs_monitor_lock = 0;
#endif

    _fs_event_stream = event_stream_allocate(512);

    _fs_file_vtable.read = _fs_file_read;
    _fs_file_vtable.write = _fs_file_write;
    _fs_file_vtable.eos = _fs_file_eos;
    _fs_file_vtable.flush = _fs_file_flush;
    _fs_file_vtable.truncate = _fs_file_truncate;
    _fs_file_vtable.size = _fs_file_size;
    _fs_file_vtable.seek = _fs_file_seek;
    _fs_file_vtable.tell = _fs_file_tell;
    _fs_file_vtable.lastmod = _fs_file_last_modified;
    _fs_file_vtable.buffer_read = 0;
    _fs_file_vtable.available_read = _fs_file_available_read;
    _fs_file_vtable.finalize = _fs_file_finalize;
    _fs_file_vtable.clone = _fs_file_clone;

    _ringbuffer_stream_initialize();
    _buffer_stream_initialize();
#if FOUNDATION_PLATFORM_ANDROID
    _asset_stream_initialize();
#endif
    _pipe_stream_initialize();

#if FOUNDATION_PLATFORM_PNACL

    int ret;
    PP_Instance instance = pnacl_instance();

    _pnacl_file_system =
        pnacl_interface(STRING_CONST(PPB_FILESYSTEM_INTERFACE));
    _pnacl_file_io = pnacl_interface(STRING_CONST(PPB_FILEIO_INTERFACE));
    _pnacl_file_ref = pnacl_interface(STRING_CONST(PPB_FILEREF_INTERFACE));
    _pnacl_var = pnacl_interface(STRING_CONST(PPB_VAR_INTERFACE));
    _pnacl_core = pnacl_interface(STRING_CONST(PPB_CORE_INTERFACE));

    if (!_pnacl_file_system)
        log_warn(0, WARNING_SYSTEM_CALL_FAIL,
                 STRING_CONST("Unable to get file system interface"));
    if (!_pnacl_file_io)
        log_warn(0, WARNING_SYSTEM_CALL_FAIL,
                 STRING_CONST("Unable to get file I/O interface"));
    if (!_pnacl_file_ref)
        log_warn(0, WARNING_SYSTEM_CALL_FAIL,
                 STRING_CONST("Unable to get file ref interface"));
    if (!_pnacl_var)
        log_warn(0, WARNING_SYSTEM_CALL_FAIL,
                 STRING_CONST("Unable to get var interface"));
    if (!_pnacl_core)
        log_warn(0, WARNING_SYSTEM_CALL_FAIL,
                 STRING_CONST("Unable to get core interface"));

    if (_pnacl_file_system && _pnacl_file_io && _pnacl_file_ref && _pnacl_var &&
        _pnacl_core) {
        _pnacl_fs_temporary = _pnacl_file_system->Create(
                                  instance, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
        _pnacl_fs_persistent = _pnacl_file_system->Create(
                                   instance, PP_FILESYSTEMTYPE_LOCALPERSISTENT);

        if ((ret = _pnacl_file_system->Open(_pnacl_fs_temporary, 100000,
                                            PP_BlockUntilComplete())) !=
            PP_OK) {
            string_const_t errmsg = pnacl_error_message(ret);
            log_warnf(
                0, WARNING_SYSTEM_CALL_FAIL,
                STRING_CONST("Unable to open temporary file system: %.*s (%d)"),
                STRING_FORMAT(errmsg), ret);
        }

        if ((ret =
                 _pnacl_file_system->Open(_pnacl_fs_persistent, 100000,
                                          PP_BlockUntilComplete()) != PP_OK)) {
            string_const_t errmsg = pnacl_error_message(ret);
            log_warnf(0, WARNING_SYSTEM_CALL_FAIL,
                      STRING_CONST(
                          "Unable to open persistent file system: %.*s (%d)"),
                      STRING_FORMAT(errmsg), ret);
        }
    }

#endif

    return 0;
}

void _fs_finalize(void) {
    size_t mi;
    if (_fs_monitors) {
        for (mi = 0; mi < foundation_config().fs_monitor_max; ++mi)
            _fs_stop_monitor(_fs_monitors + mi);
    }
    mutex_deallocate(_fs_monitor_lock);

    event_stream_deallocate(_fs_event_stream);
    _fs_event_stream = 0;

#if FOUNDATION_PLATFORM_PNACL
    _pnacl_core->ReleaseResource(_pnacl_fs_persistent);
    _pnacl_core->ReleaseResource(_pnacl_fs_temporary);

    _pnacl_fs_temporary = 0;
    _pnacl_fs_persistent = 0;
    _pnacl_file_system = 0;
    _pnacl_file_io = 0;
    _pnacl_file_ref = 0;
    _pnacl_var = 0;
    _pnacl_core = 0;
#endif

    memory_deallocate(_fs_monitors);
    _fs_monitors = 0;
}
