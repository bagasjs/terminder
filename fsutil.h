/*

   `fsutil.h` - filesystem utilities (platform: Windows, Linux)

    NOTES:
    - Filepath must be POSIX path (including on Windows i.e. "/c/Program Files/Windows/"
    - Filepath only support ASCII character

*/
#ifndef FSUTIL_H_
#define FSUTIL_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifndef FS_ASSERT
#include <assert.h>
#define FS_ASSERT assert
#endif

#ifndef FS_MALLOC
#include <stdlib.h>
#define FS_MALLOC malloc
#define FS_FREE   free
#endif

/////////////////////////////////////////
///
/// Utilties
///

/// This is temporary allocator that usually a function needs
typedef struct FsTemp {
    uint8_t *buffer;
    uint32_t reserved;
    uint32_t capacity;
} FsTemp;
void *fs_temp_alloc(FsTemp *tmp, size_t size);
void  fs_temp_reset(FsTemp *tmp);
void *fs_temp_strndup(FsTemp *tmp, const char *cstr, size_t size);
void *fs_temp_strdup(FsTemp *tmp, const char *cstr);

typedef enum {
    FS_OK = 0,
    FS_RESULT_ALREADY_EXISTS,
    FS_RESULT_MKDIR_FAILED,
    FS_RESULT_MALLOC_FAILED,
    FS_RESULT_TEMP_ALLOC_FAILED,
    FS_RESULT_NOT_A_DIR,
    FS_RESULT_NOT_A_FILE,
    FS_RESULT_COULDNT_OPEN_DIR_ENTRY,
    FS_RESULT_COULDNT_READ_NEXT_DIR_ENTRY,

    _COUNT_FS_RESULTS,
} FsResult;
const char *fs_result_to_cstr(FsResult result);

/////////////////////////////////////////
///
/// File System API
///

typedef enum {
    FS_FILE_NOT_EXIST = 0,

    FS_FILE_REGULER,
    FS_FILE_DIRECTORY,
    FS_FILE_SYMLINK,
    FS_FILE_OTHER,
} FsFileType;

FsResult fs_mkdir(const char *path);
FsResult fs_rmdir(const char *path);
FsResult fs_delete_file(const char *path);

FsFileType fs_get_file_type(const char *path);

const char *fs_getcwd(FsTemp *temp);
const char *fs_gethomedir(FsTemp *temp);
// This doesn't support the case where b = "./test" or b ="../test"
const char *fs_path_join(FsTemp *temp, const char *a, const char *b);
const char *fs_path_win32_to_posix(FsTemp *temp, const char *path);
const char *fs_path_posix_to_win32(FsTemp *temp, const char *path);
const char *fs_getext(FsTemp *temp, const char *path);
const char *fs_dirname(FsTemp *temp, const char *path);

typedef struct FsDirEntry {
    char *name;
    void *private_fields;
    FsResult result;
} FsDirEntry;
FsResult fs_dir_entry_open(const char *dir, FsDirEntry *entry);
bool fs_dir_entry_next(FsDirEntry *entry);
void fs_dir_entry_close(FsDirEntry *dir);

#endif // FSUTIL_H_

#ifdef FSUTIL_IMPLEMENTATION

const char *fs_result_to_cstr(FsResult result)
{
    static const char *result_to_cstr[_COUNT_FS_RESULTS] = {
        [FS_OK] = "ok",
        [FS_RESULT_ALREADY_EXISTS] = "already exists",
        [FS_RESULT_MKDIR_FAILED]   = "failed to create directory",
        [FS_RESULT_MALLOC_FAILED]  = "failed to allocate memory",
        [FS_RESULT_NOT_A_DIR]      = "not a directory",
        [FS_RESULT_NOT_A_FILE]     = "not a regular file",
        [FS_RESULT_COULDNT_OPEN_DIR_ENTRY] = "couldn't open dir entry",
        [FS_RESULT_COULDNT_READ_NEXT_DIR_ENTRY] = "couldn't read next dir entry",
    };
    if(result >= 0 && result < _COUNT_FS_RESULTS) 
        return result_to_cstr[result];
    else 
        return NULL;
}

void *fs_temp_alloc(FsTemp *tmp, size_t size)
{
    if(!tmp) return NULL;
    if(!tmp->buffer) return NULL;

    if(tmp->reserved + size < tmp->capacity) {
        void *buf = &tmp->buffer[tmp->reserved];
        tmp->reserved += size;
        return buf;
    }
    return NULL;
}

void *fs_temp_strdup(FsTemp *temp, const char *cstr)
{
    size_t n = 0;
    for(; cstr[n] != 0; ++n);
    char *result = fs_temp_alloc(temp, n + 1);
    size_t i = 0;
    for(; i < n; ++i) 
        result[i] = cstr[i];
    result[i++] = 0;
    return result;
}

void *fs_temp_strndup(FsTemp *tmp, const char *cstr, size_t size)
{
    size_t n = 0;
    for(; n < size && cstr[n] != 0; ++n);

    if(n < size) n = size;
    char *result = fs_temp_alloc(tmp, n + 1);
    size_t i = 0;
    for(; i < n; ++i) 
        result[i] = cstr[i];
    result[i++] = 0;
    return result;
}

void fs_temp_reset(FsTemp *tmp)
{
    tmp->reserved = 0;
}

const char *fs_path_posix_to_win32(FsTemp *temp, const char *path)
{
    size_t len = strlen(path);
    size_t cap = len + 1;
    char *result = fs_temp_alloc(temp, cap);
    if(!result) return NULL;

    for(size_t i = 0; i < len; ++i) {
        if(path[i] == '/') {
            result[i] = '\\';
        } else {
            result[i] = path[i];
        }
    }
    result[cap-1] = 0;
    return result;
}

#define FS_PATH_SEP '/'
const char *fs_path_join(FsTemp *temp, const char *path_a, const char *path_b)
{
    FS_ASSERT(path_a != NULL);
    FS_ASSERT(path_b != NULL);
    size_t len_a = strlen(path_a);
    size_t len_b = strlen(path_b);
    int need_separator = path_a[len_a - 1] != FS_PATH_SEP;
    size_t total_length = len_a + len_b + (need_separator ? 1 : 0) + 1;
    char *result = fs_temp_alloc(temp, total_length);
    if(!result) return NULL;

    size_t i;
    for(i = 0; i < len_a; ++i) result[i] = path_a[i];
    if(need_separator) result[i++] = FS_PATH_SEP;
    for(size_t j = 0; j < len_b; ++j) result[i + j] = path_b[j];

    result[total_length-1] = 0;
    return result;
}

#include <stdlib.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
typedef struct FsDirEntryWin32PrivateFields {
    WIN32_FIND_DATA win32_data;
    HANDLE win32_hFind;
    bool win32_init;
} FsDirEntryWin32PrivateFields;
#else
// #include <sys/stat.h>
int mkdir(const char *path, unsigned int stuff);
#endif

const char *fs_gethomedir(FsTemp *temp)
{
#ifdef _WIN32
    const char *home_path = getenv("HOMEPATH");
    return fs_temp_strdup(temp, home_path);
#else
#error "Not implemented for this platform"
#endif
}

FsResult fs_mkdir(const char *path)
{
#ifdef _WIN32
    int result = _mkdir(path);
#else
    int result = mkdir(path, 0755);
#endif
    if(result < 0) {
        if(errno == EEXIST) return FS_RESULT_ALREADY_EXISTS;
        return FS_RESULT_MKDIR_FAILED;
    }
    return FS_OK;
}

FsFileType fs_get_file_type(const char *path)
{
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    if(attr == INVALID_FILE_ATTRIBUTES) {
        return FS_FILE_NOT_EXIST;
    }
    if((attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return FS_FILE_DIRECTORY;
    }
    return FS_FILE_REGULER;
#else
#error "Not implemented for this platform"
#endif
}

FsResult fs_dir_entry_open(const char *path, FsDirEntry *dir)
{
    memset(dir, 0, sizeof(*dir));
#ifdef _WIN32
    uint8_t buf[1024];
    FsTemp tmp = {0};
    tmp.buffer = buf;
    tmp.capacity = sizeof(buf);
    const char *win32_path = fs_path_posix_to_win32(&tmp, path);
    if(!win32_path) return FS_RESULT_TEMP_ALLOC_FAILED;
    int length = strlen(win32_path);
    char *win32_path_ast = fs_temp_strndup(&tmp, win32_path, length + 3);
    if(!win32_path_ast) return FS_RESULT_TEMP_ALLOC_FAILED;
    if(win32_path[length - 1] != '\\') {
        win32_path_ast[length++] = '\\';
    }
    win32_path_ast[length++] = '*';
    win32_path_ast[length++] =  0;
    WIN32_FIND_DATA find_data = {0};
    HANDLE hFind = FindFirstFile(win32_path_ast, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        return FS_RESULT_COULDNT_OPEN_DIR_ENTRY;
    }

    FsDirEntryWin32PrivateFields *private_fields = FS_MALLOC(sizeof(*private_fields));
    if(private_fields == NULL) {
        FindClose(hFind);
        return FS_RESULT_MALLOC_FAILED;
    }
    private_fields->win32_hFind = hFind;
    private_fields->win32_data  = find_data;
    private_fields->win32_init  = false;
    dir->private_fields = private_fields;
#else
#error "Not implemented for this platform"
#endif
    return FS_OK;
}

bool fs_dir_entry_next(FsDirEntry *dir)
{
#ifdef _WIN32
    FS_ASSERT(dir);
    FS_ASSERT(dir->private_fields);
    FsDirEntryWin32PrivateFields *private = dir->private_fields;
    if (!private->win32_init) {
        private->win32_init = true;
        dir->name = private->win32_data.cFileName;
        return true;
    }

    if (!FindNextFile(private->win32_hFind, &private->win32_data)) {
        if (GetLastError() == ERROR_NO_MORE_FILES) return false;
        dir->result = FS_RESULT_COULDNT_READ_NEXT_DIR_ENTRY;
        return false;
    }
    dir->name = private->win32_data.cFileName;
    return true;
#else
#error "Not implemented for this platform"
#endif
}

void fs_dir_entry_close(FsDirEntry *dir)
{
#ifdef _WIN32
    FsDirEntryWin32PrivateFields *private = dir->private_fields;
    FindClose(private->win32_hFind);
    FS_FREE(private);
#else
#error "Not implemented for this platform"
#endif
}

#endif // FSUTIL_IMPLEMENTATION
