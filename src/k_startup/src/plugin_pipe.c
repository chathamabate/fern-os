
#include "k_startup/plugin_pipe.h"
#include "k_startup/page_helpers.h"
#include "s_util/misc.h"

/**
 * Create a new pipe!
 */
static pipe_t *new_pipe(allocator_t *al, size_t sig_cap) {
    if (!al || sig_cap == 0) {
        return NULL;
    }

    const size_t cap = sig_cap + 1;

    pipe_t *p = al_malloc(al, sizeof(pipe_t));
    uint8_t *buf = al_malloc(al, cap);
    basic_wait_queue_t *wq = new_basic_wait_queue(al);

    if (!p || !buf || !wq) {
        delete_wait_queue((wait_queue_t *)wq);
        al_free(al, buf);
        al_free(al, p);

        return NULL;
    }

    *(allocator_t **)&(p->al) = al;
    p->ref_count = 0;
    p->i = 0;
    p->j = 0;
    *(size_t *)&(p->cap) = cap;
    *(uint8_t **)&(p->buf) = buf;
    *(basic_wait_queue_t **)&(p->wq) = wq;

    return p;
}

/**
 * Delete a pipe. The deletes regardless of what the reference count value is!
 * (Does no checks on wait queues!!!!)
 */
static void delete_pipe(pipe_t *p) {
    if (!p) {
        return;
    }

    al_free(p->al, p->buf);
    delete_wait_queue((wait_queue_t *)(p->wq));
    al_free(p->al, p);
}

/*
 * Pipe handle state stuff.
 */

static fernos_error_t copy_pipe_handle_state(handle_state_t *hs, process_t *proc, handle_state_t **out);
static fernos_error_t delete_pipe_handle_state(handle_state_t *hs);

static fernos_error_t pipe_hs_wait_write_ready(handle_state_t *hs);
static fernos_error_t pipe_hs_write(handle_state_t *hs, const void *u_src, size_t len, size_t *u_written); 
static fernos_error_t pipe_hs_wait_read_ready(handle_state_t *hs);
static fernos_error_t pipe_hs_read(handle_state_t *hs, void *u_dest, size_t len, size_t *u_readden);

const handle_state_impl_t PIPE_HS_IMPL = {
    .copy_handle_state = copy_pipe_handle_state,
    .delete_handle_state = delete_pipe_handle_state,
    .hs_wait_write_ready = pipe_hs_wait_write_ready,
    .hs_write = pipe_hs_write,
    .hs_wait_read_ready = pipe_hs_wait_read_ready,
    .hs_read = pipe_hs_read,
    .hs_cmd = NULL,
};

static fernos_error_t copy_pipe_handle_state(handle_state_t *hs, process_t *proc, handle_state_t **out) {
    pipe_handle_state_t *pipe_hs = (pipe_handle_state_t *)hs;
    
    pipe_handle_state_t *pipe_hs_copy = al_malloc(hs->ks->al, sizeof(pipe_handle_state_t));
    if (!pipe_hs_copy) {
        return FOS_E_NO_MEM;
    }

    init_base_handle((handle_state_t *)pipe_hs_copy, &PIPE_HS_IMPL, hs->ks, proc, hs->handle, false); 
    *(pipe_t **)&(pipe_hs_copy->pipe) = pipe_hs->pipe;

    pipe_hs_copy->pipe->ref_count++;

    *out = (handle_state_t *)pipe_hs_copy;

    return FOS_E_SUCCESS;
}

static fernos_error_t delete_pipe_handle_state(handle_state_t *hs) {
    pipe_handle_state_t *pipe_hs = (pipe_handle_state_t *)hs;

    pipe_t *pipe = pipe_hs->pipe;

    // Before anything, let's just free the handle state, this must always be done!
    al_free(pipe_hs->super.ks->al, pipe_hs);

    // Next, we check if the underlying pipe has hit 0 references!
    if ((--(pipe->ref_count)) == 0) { 
        // Time to destruct the pipe!

        // Wake up all potentially waiting threads.
        PROP_ERR(bwq_wake_all_threads(pipe->wq, &(pipe_hs->super.ks->schedule), FOS_E_STATE_MISMATCH));

        delete_pipe(pipe);
    }

    return FOS_E_SUCCESS;
}

static fernos_error_t pipe_hs_wait_write_ready(handle_state_t *hs) {
    fernos_error_t err;

    pipe_handle_state_t *pipe_hs = (pipe_handle_state_t *)hs;
    thread_t *thr = (thread_t *)(pipe_hs->super.ks->schedule.head);
    pipe_t *pipe = pipe_hs->pipe;

    // Ok, so we wait if the pipe is full!

    // If j borders i to the left, the pipe is full!
    if (((pipe->j + 1) % pipe->cap) == pipe->i) {
        err = bwq_enqueue(pipe->wq, thr);
        DUAL_RET_FOS_ERR(err, thr); // Failing to enqueue here is recoverable!

        thread_detach(thr);
        thr->wq = (wait_queue_t *)(pipe->wq);
        thr->state = THREAD_STATE_WAITING;

        return FOS_E_SUCCESS;
    }

    // Pipe is not full! No need to wait!
    DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
}

static fernos_error_t pipe_hs_write(handle_state_t *hs, const void *u_src, size_t len, size_t *u_written) {
    fernos_error_t err;

    pipe_handle_state_t *pipe_hs = (pipe_handle_state_t *)hs;
    pipe_t *pipe = pipe_hs->pipe;
    thread_t *thr = (thread_t *)(pipe_hs->super.ks->schedule.head);

    if (!u_src || len == 0) {
        DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
    }

    // If the pipe is full we return FOS_E_EMPTY.
    if (((pipe->j + 1) % pipe->cap) == pipe->i) {
        DUAL_RET(thr, FOS_E_EMPTY, FOS_E_SUCCESS);
    }

    // We'll use this later to see if we must wake up sleeping readers.
    const bool was_empty = pipe->i == pipe->j;

    // How many bytes in the pipe contain user data? (Excluding the final empty cell)
    const size_t occupied_bytes = pipe->i <= pipe->j 
        ? pipe->j - pipe->i 
        : (pipe->cap - pipe->i) + pipe->j;

    // How many bytes can significant data be written to?
    const size_t available_bytes = (pipe->cap - 1) - occupied_bytes;
    
    // How many bytes we will attempt to write this call.
    // (This will expand to a deep ternary, but whatever)
    const size_t bytes_to_write = MIN(len, MIN(available_bytes, KS_PIPE_TX_MAX_LEN));

    size_t bytes_written = 0;
    uint32_t copied;
    err = FOS_E_SUCCESS;

    if (pipe->i <= pipe->j) { // j comes after i.
        const size_t bytes_to_end = pipe->cap - pipe->j;
        const size_t first_copy_amt = MIN(bytes_to_write, bytes_to_end);

        err = mem_cpy_from_user(pipe->buf + pipe->j, thr->proc->pd, u_src, first_copy_amt, &copied);

        bytes_written += copied;

        pipe->j += copied;
        if (pipe->j == pipe->cap) {
            pipe->j = 0; // Wrap if needed.
        }
    }

    if (err == FOS_E_SUCCESS && bytes_to_write - bytes_written > 0) { 
        // In this situation, it is gauranteed that j < i.
        // Either a) j < i to start with.
        // Or b) i <= j to start with and the above if statement executed successfully.
        // (with bytes left)

        // (NOTE: technically could be the first copy in the case j < i to begin with)
        const size_t second_copy_amt = bytes_to_write - bytes_written;
        err = mem_cpy_from_user(pipe->buf + pipe->j, thr->proc->pd, (const uint8_t *)u_src + bytes_written, second_copy_amt, &copied);

        bytes_written += copied;

        pipe->j += copied; // Wrap should be impossible here as j will never pass i.
    }

    // Cool, so now, the question become, do we wake anybody up?
    if (was_empty && bytes_written > 0) {
        // A failure here is catastrophic.
        PROP_ERR(bwq_wake_all_threads(pipe->wq, &(hs->ks->schedule), FOS_E_SUCCESS));
    }

    if (u_written) {
        // We don't want to overwrite a potential error stored in `err` from above.
        fernos_error_t cpy_out_err = mem_cpy_to_user(thr->proc->pd, u_written, &bytes_written, sizeof(size_t), NULL);
        DUAL_RET_FOS_ERR(cpy_out_err, thr);
    }

    // If we have succeeded until this point, just propegate `err` to user.
    // NOTE that even when something goes wrong with the signficant copies, we must still
    // handle the data that WAS written correctly!
    DUAL_RET(thr, err, FOS_E_SUCCESS);
}

static fernos_error_t pipe_hs_wait_read_ready(handle_state_t *hs) {
    fernos_error_t err;

    pipe_handle_state_t *pipe_hs = (pipe_handle_state_t *)hs;
    thread_t *thr = (thread_t *)(pipe_hs->super.ks->schedule.head);
    pipe_t *pipe = pipe_hs->pipe;

    // Ok, so we wait if the pipe is empty.

    // If i == j, the pipe is empty!
    if (pipe->i == pipe->j) {
        err = bwq_enqueue(pipe->wq, thr);
        DUAL_RET_FOS_ERR(err, thr); // Failing to enqueue here is recoverable!

        thread_detach(thr);
        thr->wq = (wait_queue_t *)(pipe->wq);
        thr->state = THREAD_STATE_WAITING;

        return FOS_E_SUCCESS;
    }

    // Pipe is not empty! No need to wait!
    DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
}

static fernos_error_t pipe_hs_read(handle_state_t *hs, void *u_dest, size_t len, size_t *u_readden) {
    fernos_error_t err;

    pipe_handle_state_t *pipe_hs = (pipe_handle_state_t *)hs;
    pipe_t *pipe = pipe_hs->pipe;

    thread_t *thr = (thread_t *)(hs->ks->schedule.head);

    if (!u_dest || len == 0) {
        DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
    }

    // Ok, we are requesting a non-zero amount of data into a real buffer,
    // is there data to read though?

    if (pipe->i == pipe->j) { // Pipe is empty when i == j.
        DUAL_RET(thr, FOS_E_EMPTY, FOS_E_SUCCESS);
    }

    // Pipe is full when `j` is directly to the left of `i`.
    const bool was_full = (pipe->j + 1) % pipe->cap == pipe->i;

    // How many bytes in the pipe contain user data? (Excluding the final empty cell)
    const size_t occupied_bytes = pipe->i <= pipe->j 
        ? pipe->j - pipe->i 
        : (pipe->cap - pipe->i) + pipe->j;

    // How many bytes we will try to read in this call.
    const size_t bytes_to_read = MIN(len, MIN(occupied_bytes, KS_PIPE_TX_MAX_LEN));

    size_t bytes_readden = 0;
    uint32_t copied;
    err = FOS_E_SUCCESS;

    if (pipe->j < pipe->i) { // In this case, we read up until the end of the buffer first.
        const size_t first_copy_amt = MIN(pipe->cap - pipe->i, bytes_to_read);

        err = mem_cpy_to_user(thr->proc->pd, u_dest, pipe->buf + pipe->i, first_copy_amt, &copied);

        bytes_readden += copied;

        pipe->i += copied;
        if (pipe->i == pipe->cap) {
            pipe->i = 0;
        }
    }

    if (err == FOS_E_SUCCESS && bytes_to_read - bytes_readden > 0) {
        const size_t second_copy_amt = bytes_to_read - bytes_readden;

        err = mem_cpy_to_user(thr->proc->pd, (uint8_t *)u_dest + bytes_readden, pipe->buf + pipe->i, second_copy_amt, &copied);

        bytes_readden += copied;

        pipe->i += copied; // Impossible `i` surpasses `j` here.
    }

    // Ok, final maneuvers here.

    if (was_full && bytes_readden > 0) {
        PROP_ERR(bwq_wake_all_threads(pipe->wq, &(hs->ks->schedule), FOS_E_SUCCESS));
    }

    if (u_readden) {
        fernos_error_t cpy_out_err = mem_cpy_to_user(thr->proc->pd, u_readden, &bytes_readden, sizeof(size_t), NULL);
        DUAL_RET_FOS_ERR(cpy_out_err, thr);
    }

    DUAL_RET(thr, err, FOS_E_SUCCESS);
}

static fernos_error_t pipe_plg_cmd(plugin_t *plg, plugin_cmd_id_t cmd, 
        uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);

static const plugin_impl_t PLUGIN_PIPE_IMPL = {
    .plg_on_shutdown = NULL,
    .plg_kernel_cmd = NULL,
    .plg_cmd = pipe_plg_cmd,
    .plg_tick = NULL,
    .plg_on_fork_proc = NULL,
    .plg_on_reset_proc = NULL,
    .plg_on_reap_proc = NULL,
};

plugin_t *new_plugin_pipe(kernel_state_t *ks) {
    plugin_t *pipe_plg = al_malloc(ks->al, sizeof(plugin_t));
    if (!pipe_plg) {
        return NULL;
    }

    init_base_plugin(pipe_plg, &PLUGIN_PIPE_IMPL, ks);
    return pipe_plg;
}

static fernos_error_t pipe_plg_cmd(plugin_t *plg, plugin_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    kernel_state_t *ks = plg->ks;

    thread_t *thr = (thread_t *)(ks->schedule.head);
    process_t *proc = thr->proc;

    fernos_error_t err;

    (void)arg2;
    (void)arg3;

    switch (cmd) {

    /*
     * Create a new pipe!
     *
     * arg0 - User pointer to a handle (where to write created handle)
     * arg1 - size_t significant capacity. (The max data held at once by the pipe)
     *
     * If `u_handle` is NULL or sig_cap is 0, FOS_E_BAD_ARGS is returned to the user.
     * If handle table is full, FOS_E_EMPTY is returned to the user.
     * If not enough memory to create the pipe, FOS_E_NO_MEM is returned to the user.
     * Errors can also occur when writing created handle back to userspace.
     */
    case PLG_PIPE_PCID_OPEN: {
        handle_t *u_handle = (handle_t *)arg0;
        size_t sig_cap = (size_t)arg1;

        if (!u_handle || sig_cap == 0 || sig_cap > KS_PIPE_MAX_LEN) {
            DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
        }

        err = FOS_E_SUCCESS;

        const handle_t NULL_HANDLE = idtb_null_id(proc->handle_table);  
        handle_t h = idtb_pop_id(proc->handle_table);
        if (h == NULL_HANDLE) {
            err = FOS_E_NO_MEM;
        }

        pipe_t *pipe = NULL;
        if (err == FOS_E_SUCCESS) {
            pipe = new_pipe(plg->ks->al, sig_cap);
            if (!pipe) {
                err = FOS_E_NO_MEM;
            }
        }

        pipe_handle_state_t *pipe_hs = NULL;
        if (err == FOS_E_SUCCESS) {
            pipe_hs = al_malloc(plg->ks->al, sizeof(pipe_handle_state_t));
            if (!pipe_hs) {
                err = FOS_E_NO_MEM;
            }
        }

        if (err == FOS_E_SUCCESS) {
            err = mem_cpy_to_user(proc->pd, u_handle, &h, sizeof(handle_t), NULL); 
        }

        if (err != FOS_E_SUCCESS) {
            al_free(plg->ks->al, pipe_hs);
            delete_pipe(pipe);
            idtb_push_id(proc->handle_table, h);

            DUAL_RET(thr, err, FOS_E_SUCCESS);
        }

        // Success!!!
        pipe->ref_count = 1; // Starting reference count 1 for creating process.
        
        init_base_handle((handle_state_t *)pipe_hs, &PIPE_HS_IMPL, ks, proc, h, false);
        *(pipe_t **)&(pipe_hs->pipe) = pipe;

        idtb_set(proc->handle_table, h, pipe_hs);

        DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
    }

    default: { // Unknown command!
        DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
    }

    }
}
