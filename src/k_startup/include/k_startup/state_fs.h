
#pragma once

#include "k_startup/fwd_defs.h"
#include "k_startup/state.h"
#include "s_data/wait_queue.h"

/*
 * Kernel State mechanisms the relate to the file system.
 *
 * This is really just an extension of the `state.h/c` files.
 * Kinda like a "partial" class in C#.
 */

struct _kernel_fs_node_state_t {
    /**
     * When an fs_node_key is allocated inside the kernel, it will be reference counted instead of
     * deep copied. That is multiple shallow copies of the same key will exist throughout the kernel
     * with this field keeping track of how many of those copies exist.
     *
     * When this number reaches zero, this file state structure is deleted and removed from
     * the `open_files` map.
     */
    uint32_t references;

    /**
     * When a user attempts to delete a file it will only actually be deleted after all currently
     * open handles are closed. So, if the file is still in use by other users when deletion is
     * requested, this field is set to true.
     *
     * When this file is true, the referenced file cannot be openned again. When all handles
     * are returned, this file will be entirely deleted.
     *
     * NOTE: For directories, this field isn't really used. When trying to delete a directory
     * which is the CWD of other processes, an error is returned. A directory can only be
     * deleted if it's node_key has no references and the directory itself is empty.
     */
    bool set_to_delete;

    /**
     * While files have nothing to do with times, we are actually going to use a "timed" wait
     * queue here to keep track of threads waiting for data to abe added to a file.
     *
     * The "time" will actually be the length of the file! As data is written to the file,
     * this queue will be notified with the new length each time!
     *
     * NOTE: This strategy would causes issues if we allow users to shrink files. Right now,
     * users will only be allowed to extend files for this reason.
     *
     * A Thread which is placed in this queue MUST have the following wait context:
     *
     * thr->wait_context[0] = (file_handle_t) The file handle used for this read request.
     * thr->wait_context[1] = (user void *) user buffer to read to
     * thr->wait_context[2] = (uint32_t) amount to attempt to read into buffer
     * thr->wait_context[3] = (user uint32_t *) where to write the actual amount read
     *
     * (The position in the given file handle will be used)
     *
     * NOTE: For directory states, this field will be NULL.
     */
    timed_wait_queue_t *twq;
};

/*
 * NOTE: All processes will have a notion of a "current working directory". When a relative
 * path is handed to any of the below functions, it is interpreted as relative to the calling
 * processes CWD.
 *
 * ALSO NOTE: Many of the below calls require user pointers to strings. Because we can't deduce
 * the length of userspace strings easily, these calls also require that the length of the string
 * is provided. Each string pointer should point to a null terminated buffer with size
 * `length + 1`.
 */

/**
 * Set the current process's working directory.
 *
 * Paths which are relative are interpreted as relative to the current working directory.
 */
fernos_error_t ks_fs_set_wd(kernel_state_t *ks, const char *u_path, size_t u_path_len);

/**
 * Create a new file.
 *
 * if `u_fh` is non-null, a new handle is created for the file, and written to `*u_fh`.
 */
fernos_error_t ks_fs_touch(kernel_state_t *ks, const char *u_path, size_t u_path_len, file_handle_t *u_fh);

/**
 * Create a new directory.
 *
 * This only creates the final component of the path. If there are directories within the path
 * which don't exist yet, this will fail.
 */
fernos_error_t ks_fs_mkdir(kernel_state_t *ks, const char *u_path, size_t u_path_len);

/**
 * Attempt to get information about a file or directory.
 *
 * On success, the information is written to userspace at `*u_info`.
 */
fernos_error_t ks_fs_get_info(kernel_state_t *ks, const char *u_path, size_t u_path_len, fs_node_info_t *u_info);

/**
 * Get the name of a node within a directory at a given index.
 *
 * `u_child_name` should be a buffer in userspace with size at least `FS_MAX_FILENAME_LEN + 1`.
 *
 * If `index` overshoots the end of the directory, FOS_INVALID is returned in the calling thread.
 *
 * On success, FOS_SUCCESS is returned in the calling thread, and the child's name is written
 * to `*u_child_name` in userspace.
 */
fernos_error_t ks_fs_get_child_name(kernel_state_t *ks, const char *u_path, size_t u_path_len, 
        size_t index, char *u_child_name);

/**
 * Open an existing file.
 * 
 * When a file is opened, it's corresponding handle will start at position 0. (i.e. the 
 * beginning of the file)
 */
fernos_error_t ks_fs_open(kernel_state_t *ks, char *u_path, size_t u_path_len, file_handle_t *u_fh);

/**
 * Return a file handle to the operating system. After this call, the file handle stored in `fh`
 * is no longer usable.
 */
void ks_fs_close(kernel_state_t *ks, file_handle_t fh);

/**
 * Move a handle's position.
 *
 * If `pos` > len(file), then the `fh`'s position will be set to len(file).
 *
 * Using SIZE_MAX as the position will thus always bring the handle position to the end of the
 * file. (Remember, we will enforce the max file size of SIZE_MAX, and thus a maximum addressable
 * byte of SIZE_MAX - 1)
 */
fernos_error_t ks_fs_seek(kernel_state_t *ks, file_handle_t fh, size_t pos);

/**
 * Write the contents of `u_src` to the referenced file.
 *
 * If `len` overshoots the end of the file, the file will be expanded as necessary.
 * If the given file handle's position is already SIZE_MAX, FOS_NO_SPACE will be returned
 * in the calling thread.
 *
 * The total number of bytes written is written to `*written`.
 * FOS_SUCCESS does NOT mean all bytes were written!!!
 */
fernos_error_t ks_fs_write(kernel_state_t *ks, file_handle_t fh, const void *u_src, size_t len, size_t *written);

/**
 * Blocking ready baybEE.
 */
fernos_error_t ks_fs_read(kernel_state_t *ks, 
