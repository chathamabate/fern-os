
#include "k_startup/plugin_pipe.h"
#include "k_startup/page_helpers.h"

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

static fernos_error_t pipe_hs_write(handle_state_t *hs, const void *u_src, size_t len, size_t *u_written); 
static fernos_error_t pipe_hs_read(handle_state_t *hs, void *u_dest, size_t len, size_t *u_readden);
static fernos_error_t pipe_hs_wait(handle_state_t *hs);
static fernos_error_t pipe_hs_cmd(handle_state_t *hs, handle_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, 
        uint32_t arg2, uint32_t arg3);

const handle_state_impl_t PIPE_HS_IMPL = {
    .copy_handle_state = copy_pipe_handle_state,
    .delete_handle_state = delete_pipe_handle_state,
    .hs_write = pipe_hs_write,
    .hs_read = pipe_hs_read,
    .hs_wait = pipe_hs_wait,
    .hs_cmd = pipe_hs_cmd
};

static fernos_error_t copy_pipe_handle_state(handle_state_t *hs, process_t *proc, handle_state_t **out) {
    return FOS_E_NOT_IMPLEMENTED;
}

static fernos_error_t delete_pipe_handle_state(handle_state_t *hs) {
    return FOS_E_NOT_IMPLEMENTED;
}

static fernos_error_t pipe_hs_write(handle_state_t *hs, const void *u_src, size_t len, size_t *u_written) {
    return FOS_E_NOT_IMPLEMENTED;
}

static fernos_error_t pipe_hs_read(handle_state_t *hs, void *u_dest, size_t len, size_t *u_readden) {
    return FOS_E_NOT_IMPLEMENTED;
}

static fernos_error_t pipe_hs_wait(handle_state_t *hs) {
    return FOS_E_NOT_IMPLEMENTED;
}

static fernos_error_t pipe_hs_cmd(handle_state_t *hs, handle_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, 
        uint32_t arg2, uint32_t arg3) {
    return FOS_E_NOT_IMPLEMENTED;
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

plugin_t *new_pipe_plugin(kernel_state_t *ks) {
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

        if (!u_handle || sig_cap == 0) {
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
