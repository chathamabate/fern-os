
#pragma once

#include <stdint.h>
#include <stddef.h>
#include "s_util/err.h"
#include "s_util/datetime.h"
#include <stdbool.h>

/*
 * Below lies a generic file system interface.
 */

/**
 * Maximum length of a filename. (Not including NT)
 */
#define FS_MAX_FILENAME_LEN (127U)

/**
 * Maximum length of a path. (Including "/"'s, Not including NT)
 */
#define FS_MAX_PATH_LEN (4096 - 1)

/*
 * NOTE: The below two functions just check characters and lengths are expected.
 * A filename/path being "valid" has nothing to do with whether or not said
 * filename/path actually points to an existing location.
 * 
 *  For a path to be valid:
 * The length of the entire path string must not exceed FS_MAX_PATH_LEN.
 * A path is just a list of valid filenames separated by SINGLE "/"'s
 * A path can optionally both end and start with "/"'s
 * "" is an invalid path. "/" is a valid path.
 */

/**
 * Returns true if the given filename is valid. False otherwise.
 */
bool is_valid_filename(const char *fn);

/**
 * Returns true if the given path is valid. False otherwise.
 */
bool is_valid_path(const char *path);

/**
 * Given a VALID OR EMPTY path, write it's next filename part into `*dest`.
 * `dest` should point to a buffer with length at least (FS_MAX_FILENAME_LEN + 1).
 *
 * Returns the index into path of the first character of the rest path.
 *
 * For example `next_filename("/a/b", d)` would write "a" (with NT) into `dest` and
 * return 2. If the path has no filenames (i.e. an empty string or just "/"), 
 * '\0' is written to `dest` and 0 is returned.
 *
 * Undefined behavior if the path is Non-empty and invalid.
 */
size_t next_filename(const char *path, char *dest);

/**
 * Given a valid path, return true iff the path is relative.
 *
 * A path is relative iff it doesn't start with '/' AND it's first filename is "." or "..".
 */
bool is_relative(const char *path);


/**
 * A Node key is an immutable piece of data which can be used to efficiently reference a 
 * file or directory.
 * 
 * Values of this type should always be passed around via pointers. (With actual data likely
 * living on some sort of heap)
 */
typedef const void *fs_node_key_t;

/**
 * Information which is retrieveable for any node (i.e. files or directories alike)
 */
typedef struct _fs_node_info_t {
    /**
     * Datetime of when the node was created.
     */
    fernos_datetime_t creation_dt;

    /**
     * Datetime of when the node was last edited.
     *
     * For a file, this is when the file was last written to.
     * For a directory, this is the last time of file within the directory was created or deleted.
     */
    fernos_datetime_t last_edited_dt;

    /**
     * For a file, this is the length of the file in bytes.
     * For a directory, this is the number of entries within the directory.
     *
     * NOTE: For directories, this includes its relative entries. For example, an empty non-root
     * directory will always have length 2 (for "." and "..").
     */
    size_t len;
} fs_node_info_t;

typedef struct _file_sys_t {
    uint8_t dummy; 
} file_sys_t;

/**
 * Flush! (What this means/does is up to the implementor)
 *
 * If `key` is NULL, this should flush the entire file system.
 * Otherwise, this should at least flush the file pointed to by `key`.
 */
fernos_error_t fs_flush(fs_node_key_t key);

/**
 * Delete the file system.
 *
 * As I want this to always succeed, this DOES NOT FLUSH. Make sure to always flush before deleting!
 */
void delete_file_sys(file_sys_t *fs);

/**
 * Get a new key which references the described file/directory.
 *
 * `cwd` will be the "current working directory" key. If `path` is relative, the node search will
 * start at this directory. 
 * If you know `path` is absolute, `cwd` can be NULL signifying no relative starting point.
 *
 * On success, FOS_SUCCESS is returned and the new key is written to `*key`.
 * If `path` does not point to an existing file, FOS_EMPTY is returned.
 * If `path` is relative and `cwd` is NULL, FOS_BAD_ARGS is returned.
 */
fernos_error_t fs_new_key(file_sys_t *fs, fs_node_key_t cwd, const char *path, fs_node_key_t *key);

/**
 * Delete a key which was allocated using the `fs_new_key` function.
 *
 * This should always succeed!
 */
void fs_delete_key(file_sys_t *fs, fs_node_key_t key);


