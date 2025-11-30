
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
    basic_wait_queue_t *w_wq = new_basic_wait_queue(al);
    basic_wait_queue_t *r_wq = new_basic_wait_queue(al);

    if (!p || !buf || !w_wq || !r_wq) {
        delete_wait_queue((wait_queue_t *)w_wq);
        delete_wait_queue((wait_queue_t *)r_wq);
        al_free(al, buf);
        al_free(al, p);

        return NULL;
    }

    *(allocator_t **)&(p->al) = al;
    p->write_refs = 0;
    p->read_refs = 0;
    p->i = 0;
    p->j = 0;
    *(size_t *)&(p->cap) = cap;
    *(uint8_t **)&(p->buf) = buf;
    *(basic_wait_queue_t **)&(p->w_wq) = w_wq;
    *(basic_wait_queue_t **)&(p->r_wq) = r_wq;

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
    delete_wait_queue((wait_queue_t *)(p->w_wq));
    delete_wait_queue((wait_queue_t *)(p->r_wq));
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

static const handle_state_impl_t PIPE_WRITER_HS_IMPL = {
    .copy_handle_state = copy_pipe_handle_state,
    .delete_handle_state = delete_pipe_handle_state,
    .hs_wait_write_ready = pipe_hs_wait_write_ready,
    .hs_write = pipe_hs_write,
    .hs_wait_read_ready = NULL,
    .hs_read = NULL,
    .hs_cmd = NULL,
};

static const handle_state_impl_t PIPE_READER_HS_IMPL = {
    .copy_handle_state = copy_pipe_handle_state,
    .delete_handle_state = delete_pipe_handle_state,
    .hs_wait_write_ready = NULL,
    .hs_write = NULL,
    .hs_wait_read_ready = pipe_hs_wait_read_ready,
    .hs_read = pipe_hs_read,
    .hs_cmd = NULL,
};

static fernos_error_t copy_pipe_handle_state(handle_state_t *hs, process_t *proc, handle_state_t **out) {
    pipe_handle_state_t *pipe_hs = (pipe_handle_state_t *)hs;

    const handle_state_impl_t * const impl = pipe_hs->super.impl;
    pipe_t * const pipe = pipe_hs->pipe;
    
    pipe_handle_state_t *pipe_hs_copy = al_malloc(hs->ks->al, sizeof(pipe_handle_state_t));
    if (!pipe_hs_copy) {
        return FOS_E_NO_MEM;
    }

    init_base_handle((handle_state_t *)pipe_hs_copy, impl, hs->ks, proc, hs->handle, false); 
    *(pipe_t **)&(pipe_hs_copy->pipe) = pipe;

    if (impl == &PIPE_WRITER_HS_IMPL) {
        pipe->write_refs++;
    } else if (impl == &PIPE_READER_HS_IMPL) {
        pipe->read_refs++; 
    } else {
        return FOS_E_ABORT_SYSTEM; // Something very wrong if we make it here!
    }

    *out = (handle_state_t *)pipe_hs_copy;

    return FOS_E_SUCCESS;
}

static fernos_error_t delete_pipe_handle_state(handle_state_t *hs) {
    pipe_handle_state_t *pipe_hs = (pipe_handle_state_t *)hs;

    pipe_t * const pipe = pipe_hs->pipe;

    const handle_state_impl_t * const impl = pipe_hs->super.impl;

    // NOTE that the r_wq and w_wq are mutually exclusive.
    // If r_wq is non-empty w_wq is empty and vice versa.
    // 
    // If for example, the final write handle is closed, the read queue is woken up with 
    // FOS_E_EMPTY signaling no more data will ever come. The write queue is woken up
    // with FOS_E_STATE_MISMATCH signaling strange user behavior. (i.e. deleting a handle
    // when a thread is waiting on it?)

    if (impl == &PIPE_WRITER_HS_IMPL) {
        pipe->write_refs--;
        if (pipe->write_refs == 0) {
            PROP_ERR(bwq_wake_all_threads(pipe->w_wq, &(pipe_hs->super.ks->schedule), FOS_E_STATE_MISMATCH));
            PROP_ERR(bwq_wake_all_threads(pipe->r_wq, &(pipe_hs->super.ks->schedule), FOS_E_EMPTY));
        }
    } else if (impl == &PIPE_READER_HS_IMPL) {
        pipe->read_refs--;
        if (pipe->read_refs == 0) {
            PROP_ERR(bwq_wake_all_threads(pipe->w_wq, &(pipe_hs->super.ks->schedule), FOS_E_EMPTY));
            PROP_ERR(bwq_wake_all_threads(pipe->r_wq, &(pipe_hs->super.ks->schedule), FOS_E_STATE_MISMATCH));
        }
    } else {
        return FOS_E_ABORT_SYSTEM; // VERY BAD!
    }

    // If no more references at all we can delete the pipe, we know the above cases will
    // catch waking up threads as needed before deletion.
    if ((pipe->write_refs + pipe->read_refs) == 0) { 
        // Time to destruct the pipe!

        delete_pipe(pipe);
    }

    // At the end we free the actual handle state no matter what.
    al_free(pipe_hs->super.ks->al, pipe_hs);

    return FOS_E_SUCCESS;
}

static fernos_error_t pipe_hs_wait_write_ready(handle_state_t *hs) {
    fernos_error_t err;

    pipe_handle_state_t *pipe_hs = (pipe_handle_state_t *)hs;
    thread_t *thr = (thread_t *)(pipe_hs->super.ks->schedule.head);
    pipe_t *pipe = pipe_hs->pipe;

    // With no readers, the pipe will NEVER accept anymore data!
    if (pipe->read_refs == 0) {
        DUAL_RET(thr, FOS_E_EMPTY, FOS_E_SUCCESS);
    }

    // If j borders i to the left, the pipe is full!
    if (((pipe->j + 1) % pipe->cap) == pipe->i) {
        err = bwq_enqueue(pipe->w_wq, thr);
        DUAL_RET_FOS_ERR(err, thr); // Failing to enqueue here is recoverable!

        thread_detach(thr);
        thr->wq = (wait_queue_t *)(pipe->w_wq);
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

    // With no readers, the pipe will NOT accept anymore data!
    if (pipe->read_refs == 0) {
        DUAL_RET(thr, FOS_E_EMPTY, FOS_E_SUCCESS);
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

    // Cool, so now, the question becomes, do we wake anybody up?
    // If we were empty and no we aren't, wake up any potentially waiting readers!
    if (was_empty && bytes_written > 0) {
        // A failure here is catastrophic.
        PROP_ERR(bwq_wake_all_threads(pipe->r_wq, &(hs->ks->schedule), FOS_E_SUCCESS));
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
        // If there is no more writers around, the pipe will never be written to again!
        if (pipe->write_refs == 0) {
            DUAL_RET(thr, FOS_E_EMPTY, FOS_E_SUCCESS);
        }

        err = bwq_enqueue(pipe->r_wq, thr);
        DUAL_RET_FOS_ERR(err, thr); // Failing to enqueue here is recoverable!

        thread_detach(thr);
        thr->wq = (wait_queue_t *)(pipe->r_wq);
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

    // NOTE: Unlike with write, a reader can continue to read from a pipe even if there are no
    // write handles open! FOS_E_EMPTY is not returned until the pipe itself is completely
    // void of data!

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
        // Wake up all potential writers if the queue was made non-full!
        PROP_ERR(bwq_wake_all_threads(pipe->w_wq, &(hs->ks->schedule), FOS_E_SUCCESS));
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

    (void)arg3;

    switch (cmd) {

    /*
     * Create a new pipe!
     *
     * arg0 - User pointer to a handle (where to write created write handle)
     * arg1 - User pointer to a handle (where to write created read handle)
     * arg2 - size_t significant capacity. (The max data held at once by the pipe)
     *
     * If `u_handle` is NULL or sig_cap is 0, FOS_E_BAD_ARGS is returned to the user.
     * If handle table is full, FOS_E_EMPTY is returned to the user.
     * If not enough memory to create the pipe, FOS_E_NO_MEM is returned to the user.
     * Errors can also occur when writing created handle back to userspace.
     */
    case PLG_PIPE_PCID_OPEN: {
        handle_t *u_write_handle = (handle_t *)arg0;
        handle_t *u_read_handle = (handle_t *)arg1;
        size_t sig_cap = (size_t)arg2;

        if (!u_write_handle || !u_read_handle || sig_cap == 0 || sig_cap > KS_PIPE_MAX_LEN) {
            DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
        }


        const handle_t NULL_HANDLE = idtb_null_id(proc->handle_table);  

        handle_t wh = idtb_pop_id(proc->handle_table);
        handle_t rh = idtb_pop_id(proc->handle_table);
        pipe_handle_state_t *pipe_w_hs = al_malloc(plg->ks->al, sizeof(pipe_handle_state_t));;
        pipe_handle_state_t *pipe_r_hs = al_malloc(plg->ks->al, sizeof(pipe_handle_state_t));
        pipe_t *pipe = new_pipe(plg->ks->al, sig_cap);

        err = mem_cpy_to_user(proc->pd, u_write_handle, &wh, sizeof(handle_t), NULL); 
        if (err == FOS_E_SUCCESS) {
            err = mem_cpy_to_user(proc->pd, u_read_handle, &rh, sizeof(handle_t), NULL); 
        }

        if (wh == NULL_HANDLE || rh == NULL_HANDLE || !pipe_w_hs || !pipe_r_hs || !pipe || err != FOS_E_SUCCESS) {
            delete_pipe(pipe);
            al_free(plg->ks->al, pipe_r_hs);
            al_free(plg->ks->al, pipe_w_hs);
            idtb_push_id(proc->handle_table, rh);
            idtb_push_id(proc->handle_table, wh);

            // Just doing unknown here to make my life easier.
            DUAL_RET(thr, FOS_E_UNKNWON_ERROR, FOS_E_SUCCESS);
        }

        // Success!!!
        pipe->write_refs = 1;
        pipe->read_refs = 1;
        
        init_base_handle((handle_state_t *)pipe_w_hs, &PIPE_WRITER_HS_IMPL, ks, proc, wh, false);
        *(pipe_t **)&(pipe_w_hs->pipe) = pipe;
        idtb_set(proc->handle_table, wh, pipe_w_hs);

        init_base_handle((handle_state_t *)pipe_r_hs, &PIPE_READER_HS_IMPL, ks, proc, rh, false);
        *(pipe_t **)&(pipe_r_hs->pipe) = pipe;
        idtb_set(proc->handle_table, rh, pipe_r_hs);

        DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
    }

    default: { // Unknown command!
        DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
    }

    }
}
