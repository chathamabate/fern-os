
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

struct _kernel_file_state_t {
    /**
     * Each process will have a table of file descriptors or handles. This field is the total
     * number of allocated handles which reference this file in the entire system.
     *
     * NOTE: One process can actually have multiple handles open for the same file. So this number
     * isn't just "how many processes are using this file".
     *
     * When this number reaches zero, this file state structure is deleted and removed from
     * the `open_files` map.
     */
    uint32_t open_handles;

    /**
     * When a user attempts to delete a file it will only actually be deleted after all currently
     * open handles are closed. So, if the file is still in use by other users when deletion is
     * requested, this field is set to true.
     *
     * When this file is true, the referenced file cannot be openned again. When all handles
     * are returned, this file will be entirely deleted.
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
     */
    timed_wait_queue_t *twq;
};

/**
 * 
 */
fernos_error_t ks_fs_open(kernel_state_t *ks, char *u_path, size_t u_path_len, file_handle_t *u_fh);
