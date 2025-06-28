
#pragma once

#include "s_util/err.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "s_data/list.h"

/*
 * A file system is a tree of directories and files. 
 * 
 * A directory is a file which contains references to other files.
 * A file is just a chunk of arbitrary data.
 *
 * The name of any file is just a string of ascii characters in the character class:
 * 'a'-'z', 'A'-'Z', '0'-'9', '_', '-', '.'.
 *
 * The maximum length of any filename is FS_MAX_FILENAME_LEN.
 */

#define FS_MAX_FILENAME_LEN     (60U)

/**
 * Determine if a filename is valid.
 */
bool is_valid_filename(const char *fn);

typedef struct _file_sys_t file_sys_t;
typedef struct _file_sys_impl_t file_sys_impl_t;

struct _file_sys_impl_t {
    int nop;
};

struct _file_sys_t {
    const file_sys_impl_t * const impl;
};

/**
 * A handle is an implementation specific way of referencing a file or directory.
 *
 * A handle is given to the user via various calls below, and all handles must be returned to the
 * file system. 
 */
typedef void *file_sys_handle_t;

/**
 * Delete a file system.
 */
void delete_file_system(file_sys_t *fs);

/**
 * Return a handle to the file system.
 */
void fs_return_handle(file_sys_t *fs, file_sys_handle_t hndl);

/**
 * Retrieve a handle for the root directory.
 *
 * The root directory should always exist within the file system!
 *
 * NOTE: This handle needs to be returned after use just like any other handle!
 */
fernos_error_t fs_find_root_dir(file_sys_t *fs, file_sys_handle_t *out);

/**
 * Find a file within a directory. On error, nothing will be written to the out pointer.
 *
 * NOTE: I thought about saying NULL is always written to the out handle on error, but in another 
 * implementation, NULL might actually have some significance.
 */
fernos_error_t fs_find(file_sys_t *fs, file_sys_handle_t dir_hndl, 
        const char *fn, file_sys_handle_t *out);

/**
 * Determine if the given handle points to a directory or file. If it is a directory, true is 
 * written to *out.
 *
 * This returns an error and not a boolean to account for the case where the given handle is
 * invalid.
 */
fernos_error_t fs_handle_is_dir(file_sys_t *fs, file_sys_handle_t hndl, bool *out);

/**
 * Create a new directory with the given name.
 *
 * out is an optional parameter. If provided, on success, a handle is created for the new directory
 * and written to *out.
 */
fernos_error_t fs_mkdir(file_sys_t *fs, file_sys_handle_t dir, 
        const char *fn, file_sys_handle_t *out);

/**
 * Maybe get one file name at a time??
 * Could be one way of doing this???
 * Ehhh, We do need to change file reading def tbh...
 * Get the names of all files within a directory!
 *
 * This is returned as a new list of char *'s? Ehhh, not the best interface imo...
 * would be better if static tbh...
 */
fernos_error_t fs_dir_entries(file_sys_t *fs, file_sys_handle_t dir, list_t **out);

/**
 * Create a new non-directory file with the given name.
 *
 * out is an optional parameter. If provided, on success, a handle is created for the new file
 * and written to *out.
 */
fernos_error_t fs_touch(file_sys_t *fs, file_sys_handle_t dir, 
        const char *fn, file_sys_handle_t *out);

/**
 * Read data from a file. 
 *
 * On Success, FOS_SUCCESS is returned and the number of bytes read are written to *readden.
 * A partial read is still a success!!! So always check readden to confirm.
 *
 * readden should be a required argument.
 */
fernos_error_t fs_read(file_sys_t *fs, file_sys_handle_t hndl, size_t offset, 
        void *dest, size_t *readden);

/**
 * 
 */
fernos_error_t fs_append(file_sys_t *fs, file_sys_handle_t hndl, 
        const void *src, size_t len, size_t *written);


