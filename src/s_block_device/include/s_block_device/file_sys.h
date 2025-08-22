
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
     * NOTE: All directories have a length of at least 2. (All directories have entries for 
     * "." and "..")
     */
    size_t len;
} fs_node_info_t;

typedef struct _fs_t {
    uint8_t dummy; 
} fs_t;

/**
 * Delete the file system.
 */
void delete_file_sys(fs_t *fs);


