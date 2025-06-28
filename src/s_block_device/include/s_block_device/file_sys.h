
#pragma once

#include "s_util/err.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "s_util/datetime.h"

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
 *
 * For the rest of this file, a file can either be a directory or a non-directory.
 * If there is a situation where an input/output cannot be a directory, "non-directory file"
 * will be used in the docstring..
 */

#define FS_MAX_FILENAME_LEN     (60U)

/**
 * Determine if a filename is valid.
 */
bool is_valid_filename(const char *fn);

/**
 * This structure denotes information which should be accesible for all files.
 */
typedef struct _file_sys_info_t {
    /**
     * REQUIRED!
     */
    char name[FS_MAX_FILENAME_LEN + 1];

    /**
     * Both creation DT and last write DT are optional. If your FS doesn't support them, you
     * can just write dummy values here.
     */
    fernos_datetime_t creation_dt;
    fernos_datetime_t last_write_dt;

    /**
     * REQUIRED! Is this directory or a non-directory?
     */
    bool is_dir;

    /**
     * REQUIRED! 
     *
     * If this is a directory, len should be the number of entries in the directory.
     * If this is a non-directory file, len should be the number of bytes in the file.
     */
    size_t len;
} file_sys_info_t;

typedef struct _file_sys_t file_sys_t;
typedef struct _file_sys_impl_t file_sys_impl_t;

struct _file_sys_impl_t {
    int nop;
};

struct _file_sys_t {
    const file_sys_impl_t * const impl;
};

/**
 * A handle is an implementation specific way of referencing a file.
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
 * Get information about a given file.
 *
 * On error, the value of *out is undefined.
 */
fernos_error_t fs_get_info(file_sys_t *fs, file_sys_handle_t hndl, file_sys_info_t *out);

/**
 * Get the handle of a file within a directory.
 *
 * A directory holds a list of entries. Index is used to pick a certain entry. 
 *
 * It is gauranteed that indeces go from 0 to len - 1.
 *
 * If you wanted to delete all the files in a directory, you could delete index 0 len times. 
 * This would be valid.
 *
 * NOTE: the index of a specific file within a directory can change. (For example if earlier
 * files are deleted from the directory) So, never assume that a file always holds a specific 
 * index. The idea here is that you can iterate over the entries of a directory easily, and use
 * fs_get_info above to determine if the file you retrieved has the name you are looking for.
 *
 * NOTE: I thought about saying NULL is always written to the out handle on error, but in another 
 * implementation, NULL might actually have some significance.
 */
fernos_error_t fs_find(file_sys_t *fs, file_sys_handle_t dir_hndl, 
        size_t index, file_sys_handle_t *out);

/**
 * Get the parent directory of a file.
 *
 * The retrieved handle is written to *out.
 *
 * Returns an error if hndl points to the root directory.
 */
fernos_error_t fs_get_parent_dir(file_sys_t *fs, file_sys_handle_t hndl, file_sys_handle_t *out);

/**
 * Create a new directory with the given name.
 *
 * out is an optional parameter. If provided, on success, a handle is created for the new directory
 * and written to *out.
 */
fernos_error_t fs_mkdir(file_sys_t *fs, file_sys_handle_t dir, 
        const char *fn, file_sys_handle_t *out);

/**
 * Create a new non-directory file with the given name.
 *
 * out is an optional parameter. If provided, on success, a handle is created for the new file
 * and written to *out.
 */
fernos_error_t fs_touch(file_sys_t *fs, file_sys_handle_t dir, 
        const char *fn, file_sys_handle_t *out);

/**
 * Delete a file.
 *
 * Returns an error if hndl points to the root directory.
 *
 * Returns an error if hndl points to a non-empty directory. Recursive deletion could be a pretty
 * time-intensive task. So, this call kinda bubbles up the responsibilty. (Recursive deletion
 * will probably best be implemented in userspace to prevent long kernel operations)
 */
fernos_error_t fs_rm(file_sys_t *fs, file_sys_handle_t hndl);

// Reading/appending will need work!

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


