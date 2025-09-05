
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "s_util/err.h"
#include "s_bridge/shared_defs.h"

#include "s_block_device/file_sys.h"

/*
 * All of these calls correspond 1:1 to the functions in `k_startup/state_fs.h`.
 * For more information on what these functions do, see the Docstrings there.
 */

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
fernos_error_t sc_fs_open(const char *path, file_handle_t *fh);

/**
 * Return a file handle to the operating system. After this call, the file handle stored in `fh`
 * is no longer usable.
 *
 * NOTE: If the file referenced by this handle is no longer referenced by any handles after this
 * call, all threads waiting to read from said file will be woken up. This is a somewhat
 * niche case. To avoid, never close your handles until after your done reading!
 */
void sc_fs_close(file_handle_t fh);

/**
 * Move a handle's position.
 *
 * If `pos` > len(file), then the `fh`'s position will be set to len(file).
 *
 * Using SIZE_MAX as the position will thus always bring the handle position to the end of the
 * file. (Remember, we will enforce the max file size of SIZE_MAX, and thus a maximum addressable
 * byte of SIZE_MAX - 1)
 *
 * Returns FOS_INVALID_INDEX if `fh` cannot be found.
 */
fernos_error_t sc_fs_seek(file_handle_t fh, size_t pos);

/**
 * Write the contents of `src` to the referenced file.
 *
 * If `len` overshoots the end of the file, the file will be expanded as necessary.
 * If the given file handle's position is already SIZE_MAX, FOS_NO_SPACE will be returned
 * in the calling thread.
 *
 * The total number of bytes written is written to `*written`.
 * FOS_SUCCESS does NOT mean all bytes were written!!! (This will likely write in chunks to 
 * prevent being in the kernel for too long)
 */
fernos_error_t sc_fs_write(file_handle_t fh, const void *src, size_t len, size_t *written);

/**
 * Wrapper around `sc_fs_write`. It will write in a loop until all bytes from 
 * `src` are written, or an error is thrown.
 */
fernos_error_t sc_fs_write_full(file_handle_t fh, const void *src, size_t len);

/**
 * Blocking read.
 *
 * Read from a file into userspace buffer `dst`.
 *
 * If the position of `fh` = len(file), this function blocks the current thread until more data 
 * is added to the file. If any data at all can be read, it is immediately written to `dst`
 * and FOS_SUCCESS is returned.
 *
 * If the position of `fh` = SIZE_MAX, FOS_SUCCESS is return and 0 is written to `*readden`.
 *
 * Just like with `sc_fs_write`, FOS_SUCCESS does NOT mean `len` bytes were read. 
 * On Success, always check what is written to `readden` to confirm the actual number of 
 * read bytes.
 */
fernos_error_t sc_fs_read(file_handle_t fh, void *dst, size_t len, size_t *readden);

/**
 * Wrapper around `sc_fs_read`. It reads in a loop until `len` bytes are read into `dst`.
 * (Or an error is returned)
 */
fernos_error_t sc_fs_read_full(file_handle_t fh, void *dst, size_t len);

/**
 * Flush!
 *
 * If `fh == FOS_MAX_FILE_HANDLES_PER_PROC`, this will flush the entire kernel file
 * system. (Again, what "flushing" actually means depends on what file system is being used)
 *
 * returns FOS_INVALID_INDEX if `fh` cannot be found AND it isn't equal to FOS_MAX_FILE_HANDLES_PER_PROC.
 */
fernos_error_t sc_fs_flush(file_handle_t fh);
