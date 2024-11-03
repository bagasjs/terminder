#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

#ifndef FS_WITHOUT_STDINT
#include <stdint.h>
typedef uint8_t fs_uint8;
typedef uint32_t fs_uint32;
#else
#endif

typedef fs_uint8 fs_bool;
#define FS_TRUE 1
#define FS_FALSE 0

#if !defined(FS_MALLOC) && !defined(FS_FREE)
#include <stdlib.h>
#define FS_MALLOC malloc
#define FS_FREE free
#endif

char *fs_getcwd();

#endif // FILESYSTEM_H_
