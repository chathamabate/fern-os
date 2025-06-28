
#pragma once

#include "s_util/err.h"

typedef struct _file_sys_t file_sys_t;
typedef struct _file_sys_impl_t file_sys_impl_t;

struct _file_sys_impl_t {
    int nop;
};

struct _file_sys_t {
    const file_sys_impl_t * const impl;
};

/**
 * A handle is an implementation defined way of referencing a file or directory.
 */
typedef void *file_sys_handle_t;

/**
 * The root directory should always be accessible via this handle which never needs to be 
 * returned.
 */
#define FS_ROOT_DIR_HANDLE (NULL)

/**
 * Delete a file system.
 */
void delete_file_system(file_sys_t *fs);


fernos_error_t fs_find(file_sys_handle_t dir_hndl, const char *name, file_sys_handle_t *out);


