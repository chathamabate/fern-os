
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "s_util/err.h"
#include "s_bridge/shared_defs.h"

#include "s_block_device/file_sys.h"

/**
 * Set the current process's working directory.
 *
 * Paths which are relative are interpreted as relative to the current working directory.
 *
 * returns FOS_INVALID_INDEX if the given path does not exist.
 * returns FOS_STATE_MISMATCH if path doesn't point to a directory.
 */
fernos_error_t sc_fs_set_wd(const char *path);

/**
 * Create a new file.
 *
 * returns FOS_INVALID_INDEX if the directory component of the path does not exist.
 * returns FOS_IN_USE if the given path already exists.
 */
fernos_error_t sc_fs_touch(const char *path);

/**
 * Create a new directory.
 *
 * returns FOS_INVALID_INDEX if the directory component of the path does not exist.
 * returns FOS_IN_USE if the given path already exists.
 */
fernos_error_t sc_fs_mkdir(const char *path);

/**
 * Remove a file or directory.
 *
 * If this node trying to be deleted is currently refercence by any process, FOS_IN_USE
 * is returned.
 *
 * returns FOS_INVALID_INDEX if the given path does not exist.
 * FOS_IN_USE is also returned when trying to delete a non-empty directory.
 */
fernos_error_t sc_fs_remove(const char *path);

/**
 * Attempt to get information about a file or directory.
 *
 * returns FOS_INVALID_INDEX if the given path does not exist.
 * On success, the information is written to `*info`.
 */
fernos_error_t sc_fs_get_info(const char *path, fs_node_info_t *info);

/**
 * Get the name of a node within a directory at a given index.
 *
 * `child_name` should be a buffer in with size at least `FS_MAX_FILENAME_LEN + 1`.
 *
 * returns FOS_INVALID_INDEX if the given path does not exist.
 * returns FOS_STATE_MISMATCH if given path leads to a file.
 * On success, FOS_SUCCESS is returned in the calling thread, and the child's name is written
 * to `*child_name` in userspace.
 *
 * If `index` overshoots the end of the directory, FOS_SUCCESS is still returned, but
 * `\0` is written to `child_name`.
 */
fernos_error_t sc_fs_get_child_name(const char *path, 
        size_t index, char *child_name);

/**
 * Flush the entire file system.
 */
fernos_error_t sc_fs_flush_all(void);

/**
 * Open an existing file.
 * 
 * When a file is opened, it's corresponding handle will start at position 0. (i.e. the 
 * beginning of the file)
 *
 * returns FOS_INVALID_INDEX if the given path does not exist.
 * returns FOS_STATE_MISMATCH if the given path is a directory.
 * FOS_EMPTY is returned when we are out of space in the file handle table for this
 * process!
 */
fernos_error_t sc_fs_open(const char *path, handle_t *h);

/**
 * Move a handle's position.
 *
 * If `pos` > len(file), then the `fh`'s position will be set to len(file).
 *
 * Using SIZE_MAX as the position will thus always bring the handle position to the end of the
 * file. (Remember, we will enforce the max file size of SIZE_MAX, and thus a maximum addressable
 * byte of SIZE_MAX - 1)
 */
fernos_error_t sc_fs_seek(handle_t h, size_t pos);

/**
 * Call the flush command on a given handle.
 */
fernos_error_t sc_fs_flush(handle_t h);


