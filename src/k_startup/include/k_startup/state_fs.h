
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
     * This will contain all threads which are waiting for this file to expand.
     *
     * thr->wait_context[0] = (file_handle_state_t *) The pointer to the file handle's state.
     * thr->wait_context[1] = (user void *) user buffer to read to
     * thr->wait_context[2] = (uint32_t) amount to attempt to read into buffer
     * thr->wait_context[3] = (user uint32_t *) where to write the actual amount read
     *
     * (The position in the given file handle will be used, although all threads waiting in this
     * queue will be reading from the same position. Remember, attempting to read from a position
     * before the end of the file will never block!)
     *
     * NOTE: For directory states, this field will be NULL.
     */
    basic_wait_queue_t *bwq;
};


/**
 * Register a node key with the kernel state.
 *
 * If `nk` already appears in the kernel node key map, it's corresponding entry has its
 * reference count incremented.
 *
 * If `nk` does not appear in the map. `nk` is copied to create a new node key which is then placed
 * in the map. 
 *
 * The given node key `nk` is NEVER consumed by this function.
 *
 * If `kernel_nk` is non-NULL. The copy of `nk` which is used by the kernel will be written to
 * `*kernel_nk`.
 *
 * returns FOS_ABORT_SYSTEM in the case where this call encounters some unexpected state.
 * (i.e. it is used incorrectly within the kernel)
 *
 * Other errors, mean something non-fatal went wrong, for example, we just don't have enough memory.
 */
fernos_error_t ks_fs_register_nk(kernel_state_t *ks, fs_node_key_t nk, fs_node_key_t *kernel_nk);

/**
 * Deregister a node key with the kernel state.
 *
 * If `nk` appears in the kernel node key map, it's corresponding value has its reference count
 * decremented.
 *
 * If `nk`'s reference count becomes zero, It's entry in the map is deleted. All threads which
 * were waiting on the node key will be woken up with error code FOS_STATE_MISMATCH.
 *
 * The given node key `nk` is NEVER consumed by this function.
 *
 * While this is a destructor like function it returns an error in the situation
 * where the kernel state/`nk` is not as expected.
 *
 * Will always succeed unless some unexpected state is encountered,
 * then FOS_ABORT_SYSTEM is returned.
 */
fernos_error_t ks_fs_deregister_nk(kernel_state_t *ks, fs_node_key_t nk);

/**
 * Deregister all node keys reference by a given process.
 */
fernos_error_t ks_fs_deregister_proc_nks(kernel_state_t *ks, process_t *proc);

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
 */
fernos_error_t ks_fs_touch(kernel_state_t *ks, const char *u_path, size_t u_path_len);

/**
 * Create a new directory.
 *
 * This only creates the final component of the path. If there are directories within the path
 * which don't exist yet, this will fail.
 */
fernos_error_t ks_fs_mkdir(kernel_state_t *ks, const char *u_path, size_t u_path_len);

/**
 * Remove a file or directory.
 *
 * If this node trying to be deleted is currently refercence by any process, FOS_IN_USE
 * is returned.
 *
 * FOS_IN_USE is also returned when trying to delete a non-empty directory.
 */
fernos_error_t ks_fs_remove(kernel_state_t *ks, const char *u_path, size_t u_path_len);

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
 *
 * This DOES correspond to a system call. HOWEVER, the calling thread doesn't get a return
 * value. The error code here is just so the kernel knows if some fatal state was found.
 */
fernos_error_t ks_fs_close(kernel_state_t *ks, file_handle_t fh);

/**
 * Move a handle's position.
 *
 * If `pos` > len(file), then the `fh`'s position will be set to len(file).
 *
 * Using SIZE_MAX as the position will thus always bring the handle position to the end of the
 * file. (Remember, we will enforce the max file size of SIZE_MAX, and thus a maximum addressable
 * byte of SIZE_MAX - 1)
 *
 * Returns FOS_INVALID_INDEX if `fh` cannot be found (To calling thread)
 */
fernos_error_t ks_fs_seek(kernel_state_t *ks, file_handle_t fh, size_t pos);

/**
 * In one transaction, read/write, this is the maximum number of bytes transferable
 * in a single system call. Requesting more than this number will NOT cause an error.
 * The request will simply be rounded down.
 *
 * This is to minimize time in the kernel, and also to avoid big dynamic allocations
 * during reads/writes.
 */
#define KS_FS_TX_MAX_LEN (2048U)

/**
 * Write the contents of `u_src` to the referenced file.
 *
 * If `len` overshoots the end of the file, the file will be expanded as necessary.
 * If the given file handle's position is already SIZE_MAX, FOS_NO_SPACE will be returned
 * in the calling thread.
 *
 * The total number of bytes written is written to `*written`.
 * FOS_SUCCESS does NOT mean all bytes were written!!! (This will likely write in chunks to 
 * prevent being in the kernel for too long)
 */
fernos_error_t ks_fs_write(kernel_state_t *ks, file_handle_t fh, const void *u_src, size_t len, size_t *u_written);

/**
 * Blocking read.
 *
 * Read from a file into userspace buffer `u_dst`.
 *
 * If the position of `fh` = len(file), this function blocks the current thread until more data 
 * is added to the file. If any data at all can be read, it is immediately written to `u_dst`
 * and FOS_SUCCESS is returned.
 *
 * If the position of `fh` = SIZE_MAX, FOS_SUCCESS is return and 0 is written to `*u_readden`.
 *
 * Just like with `ks_fs_write`, FOS_SUCCESS does NOT mean `len` bytes were read. 
 * On Success, always check what is written to `u_readden` to confirm the actual number of 
 * read bytes.
 */
fernos_error_t ks_fs_read(kernel_state_t *ks, file_handle_t fh, void *u_dst, size_t len, size_t *u_readden);
