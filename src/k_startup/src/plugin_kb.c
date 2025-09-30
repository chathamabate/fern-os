
#include "k_startup/plugin_kb.h"
#include "s_util/ps2_scancodes.h"
#include "k_startup/page_helpers.h"

#if PLG_KB_BUFFER_SIZE & 1
#error "The Keyboard Plugin Buffer size must be even!"
#endif

/*
 * Keyboard handle state stuff.
 */

static fernos_error_t copy_kb_handle_state(handle_state_t *hs, process_t *proc, handle_state_t **out);
static fernos_error_t delete_kb_handle_state(handle_state_t *hs);

static fernos_error_t kb_hs_read(handle_state_t *hs, void *u_dest, size_t len, size_t *u_readden);
static fernos_error_t kb_hs_wait(handle_state_t *hs);
static fernos_error_t kb_hs_cmd(handle_state_t *hs, handle_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, 
        uint32_t arg2, uint32_t arg3);

const handle_state_impl_t KB_HS_IMPL = {
    .copy_handle_state = copy_kb_handle_state,
    .delete_handle_state = delete_kb_handle_state,
    .hs_write = NULL,
    .hs_read = kb_hs_read,
    .hs_wait = kb_hs_wait,
    .hs_cmd = kb_hs_cmd
};

static fernos_error_t copy_kb_handle_state(handle_state_t *hs, process_t *proc, handle_state_t **out) {
    plugin_kb_handle_state_t *plg_hs = (plugin_kb_handle_state_t *)hs;

    plugin_kb_handle_state_t *plg_hs_copy = al_malloc(hs->ks->al, sizeof(plugin_kb_handle_state_t));
    if (!plg_hs_copy) {
        return FOS_E_NO_MEM;
    }

    init_base_handle((handle_state_t *)plg_hs_copy, &KB_HS_IMPL, hs->ks, proc, hs->handle);
    *(plugin_kb_t **)&(plg_hs_copy->plg_kb) = plg_hs->plg_kb;
    plg_hs_copy->pos = plg_hs->pos;

    *out = (handle_state_t *)plg_hs_copy;

    return FOS_E_SUCCESS;
}

static fernos_error_t delete_kb_handle_state(handle_state_t *hs) {
    al_free(hs->ks->al, hs);
    return FOS_E_SUCCESS;
}

/**
 * If there are no more keys to read at this moment FOS_E_EMPTY is returned.
 *
 * On success, the number of BYTES read will be written to `*u_readden`. 
 * Remember, each scancode is a 16-bit value. To determine how many scancodes were read,
 * divide the read bytes amount by 2.
 *
 * NOTE: This implementation WILL let the user read an odd number of bytes.
 * However, this is highly unadvised. Scancodes will be stored little endian
 * in the buffer.
 */
static fernos_error_t kb_hs_read(handle_state_t *hs, void *u_dest, size_t len, size_t *u_readden) {
    fernos_error_t err;

    plugin_kb_handle_state_t *plg_kb_hs = (plugin_kb_handle_state_t *)hs;
    plugin_kb_t *plg_kb = plg_kb_hs->plg_kb;

    kernel_state_t *ks = plg_kb_hs->super.ks;
    thread_t *thr = ks->curr_thread;
    process_t *proc = thr->proc;

    if (!u_dest) {
        DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
    }

    // Here we are caught up with the global buffer, nothing to read!
    if (plg_kb_hs->pos == plg_kb->sc_buf_pos) {
        DUAL_RET(thr, FOS_E_EMPTY, FOS_E_SUCCESS);
    }

    size_t bytes_to_read = len; // How many more bytes we have left to read.
    size_t bytes_read = 0; // How many bytes we've read thus far.

    // We'll first place all bytes into the non-cycle buffer, than copy to user space.
    uint8_t copy_buffer[PLG_KB_BUFFER_SIZE];

    // Remember, the global buffer is cyclic.
    if (bytes_to_read > 0 && plg_kb_hs->pos > plg_kb->sc_buf_pos) {
        size_t first_copy_amt = PLG_KB_BUFFER_SIZE - plg_kb_hs->pos;
        if (first_copy_amt > bytes_to_read) {
            first_copy_amt = bytes_to_read;
        }

        mem_cpy(&(copy_buffer[bytes_read]), &(plg_kb->sc_buf[plg_kb_hs->pos]), first_copy_amt);

        plg_kb_hs->pos += first_copy_amt;
        if (plg_kb_hs->pos == PLG_KB_BUFFER_SIZE) {
            plg_kb_hs->pos = 0;
        }

        bytes_to_read -= first_copy_amt;
        bytes_read += first_copy_amt;
    }

    // If the if statement above ran, it's possible the handle position and the global position
    // are both now 0, in which case there is no more to read. So, we will check if the 
    // handle position is less than the global position here.
    if (bytes_to_read > 0 && plg_kb_hs->pos < plg_kb->sc_buf_pos) {
        size_t second_copy_amt = plg_kb->sc_buf_pos - plg_kb_hs->pos;
        if (second_copy_amt > bytes_to_read) {
            second_copy_amt = bytes_to_read;
        }

        mem_cpy(&(copy_buffer[bytes_read]), &(plg_kb->sc_buf[plg_kb_hs->pos]), second_copy_amt);

        // At most, this will bring pos directly up to the global position.
        // Thus, we don't need to check if pos == The Buffer size like we did in the other
        // if statement.
        plg_kb_hs->pos += second_copy_amt;

        bytes_to_read -= second_copy_amt;
        bytes_read += second_copy_amt;
    }

    // Finally, copy to userspace in one go.
    
    err = mem_cpy_to_user(proc->pd, u_dest, copy_buffer, bytes_read, NULL);
    DUAL_RET_FOS_ERR(err, thr);

    if (u_readden) {
        err = mem_cpy_to_user(proc->pd, u_readden, &bytes_read, sizeof(size_t), NULL);
        DUAL_RET_FOS_ERR(err, thr);
    }

    DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
}

/**
 * Here we just check if the calling thread's position matches the current global position.
 */
static fernos_error_t kb_hs_wait(handle_state_t *hs) {
    fernos_error_t err;

    plugin_kb_handle_state_t *plg_kb_hs = (plugin_kb_handle_state_t *)hs;
    plugin_kb_t *plg_kb = plg_kb_hs->plg_kb;

    kernel_state_t *ks = plg_kb_hs->super.ks;
    thread_t *thr = ks->curr_thread;

    if (plg_kb_hs->pos == plg_kb->sc_buf_pos) { // Time to wait!
        basic_wait_queue_t *bwq = plg_kb->bwq;      

        err = bwq_enqueue(bwq, thr);
        DUAL_RET_COND(err != FOS_E_SUCCESS, thr, FOS_E_UNKNWON_ERROR, FOS_E_SUCCESS);

        thr->wq = (wait_queue_t *)bwq;
        thr->wait_ctx[0] = (uint32_t)plg_kb_hs;
        thr->state = THREAD_STATE_WAITING;

        // Current thread is now waiting, just return one code to kernel space.
        return FOS_E_SUCCESS;
    }

    DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
}

static fernos_error_t kb_hs_cmd(handle_state_t *hs, handle_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, 
        uint32_t arg2, uint32_t arg3) {

    (void)arg0;
    (void)arg1;
    (void)arg2;
    (void)arg3;

    plugin_kb_handle_state_t *plg_kb_hs = (plugin_kb_handle_state_t *)hs;
    plugin_kb_t *plg_kb = plg_kb_hs->plg_kb;

    kernel_state_t *ks = plg_kb_hs->super.ks;
    thread_t *thr = ks->curr_thread;
    
    switch (cmd) {

    /*
     * This just sets the handle's position equal to the global buffer's position.
     * All keycodes inbetween will be skipped over from this handle's perspective.
     */
    case PLG_KB_HCID_SKIP_FWD: {
        plg_kb_hs->pos = plg_kb->sc_buf_pos;

        DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
    }

    default: {
        DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
    }

    }
}

/*
 * Keyboard plugin state stuff.
 */

static fernos_error_t plg_kb_kernel_cmd(plugin_t *plg, plugin_kernel_cmd_id_t kcmd, uint32_t arg0,
        uint32_t arg1, uint32_t arg2, uint32_t arg3);
static fernos_error_t plg_kb_cmd(plugin_t *plg, plugin_cmd_id_t cmd, uint32_t arg0, uint32_t arg1,
        uint32_t arg2, uint32_t arg3);

static const plugin_impl_t PLUGIN_KB_IMPL = {
    .plg_on_shutdown = NULL,
    .plg_kernel_cmd = plg_kb_kernel_cmd,
    .plg_cmd = plg_kb_cmd,
    .plg_tick = NULL,
    .plg_on_fork_proc = NULL,
    .plg_on_reap_proc = NULL,
};

plugin_t *new_plugin_kb(kernel_state_t *ks) {
    plugin_kb_t *plg_kb = al_malloc(ks->al, sizeof(plugin_kb_t));
    basic_wait_queue_t *bwq = new_basic_wait_queue(ks->al);

    if (!plg_kb || !bwq) {
        delete_wait_queue((wait_queue_t *)bwq);
        al_free(ks->al, plg_kb);
        return NULL;
    }

    // Success!

    init_base_plugin((plugin_t *)plg_kb, &PLUGIN_KB_IMPL, ks);

    plg_kb->sc_buf_pos = 0;
    *(basic_wait_queue_t **)&(plg_kb->bwq) = bwq;

    return (plugin_t *)plg_kb;
}

static fernos_error_t plg_kb_kernel_cmd(plugin_t *plg, plugin_kernel_cmd_id_t kcmd, uint32_t arg0,
        uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    fernos_error_t err;

    (void)arg1;
    (void)arg2;
    (void)arg3;

    scs1_code_t sc;
    plugin_kb_t *plg_kb = (plugin_kb_t *)plg;

    switch (kcmd) {

    /*
     * A Key Event is either a key press or key release. (Using PS/2 Scancode Set 1)
     */
    case PLG_KB_KCID_KEY_EVENT: {
        sc = (scs1_code_t)arg0;

        const size_t old_pos = plg_kb->sc_buf_pos;

        // Now, place the scancode in the buffer little endian.

        plg_kb->sc_buf[plg_kb->sc_buf_pos++] = 0xFF & sc;
        if (plg_kb->sc_buf_pos == PLG_KB_BUFFER_SIZE) {
            plg_kb->sc_buf_pos = 0;
        }

        plg_kb->sc_buf[plg_kb->sc_buf_pos++] = (0xFF00 & sc) >> 8;
        if (plg_kb->sc_buf_pos == PLG_KB_BUFFER_SIZE) {
            plg_kb->sc_buf_pos = 0;
        }

        // Technically, because the buffer size is gauranteed to be even, I think
        // the first wrap if statement above is unnecessary, but whatever.

        // Anyway, now let's wake up any waiting boys.

        basic_wait_queue_t *bwq = plg_kb->bwq;

        err = bwq_notify_all(bwq);
        if (err != FOS_E_SUCCESS) {
            return err;
        }

        thread_t *woken_thr;

        while ((err = bwq_pop(bwq, (void **)&woken_thr)) == FOS_E_SUCCESS) {
            plugin_kb_handle_state_t *hs_kb = (plugin_kb_handle_state_t *)(woken_thr->wait_ctx[0]);
            mem_set(woken_thr->wait_ctx, 0, sizeof(woken_thr->wait_ctx));

            if (hs_kb->pos != old_pos) {
                return FOS_E_STATE_MISMATCH; // sanity check for the boys back home.
            }

            // Now let's schedule this guy.
            
            woken_thr->ctx.eax = FOS_E_SUCCESS;

            ks_schedule_thread(plg->ks, woken_thr);
        }

        if (err != FOS_E_EMPTY) {
            return err;
        }

        return FOS_E_SUCCESS;
    }

    default: {
        return FOS_E_INVALID_INDEX;
    }

    }
}

static fernos_error_t plg_kb_cmd(plugin_t *plg, plugin_cmd_id_t cmd, uint32_t arg0, uint32_t arg1,
        uint32_t arg2, uint32_t arg3) {
    fernos_error_t err;

    plugin_kb_t *plg_kb = (plugin_kb_t *)plg;
    kernel_state_t *ks = plg->ks;

    thread_t *thr = ks->curr_thread;
    process_t *proc = thr->proc;

    (void)arg1;
    (void)arg2;
    (void)arg3;

    switch (cmd) {

    case PLG_KB_PCID_OPEN: {
        handle_t *u_handle = (handle_t *)arg0;
        if (!u_handle) {
            DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
        }

        // Ok, so now we create the 

        err = FOS_E_SUCCESS;

        const handle_t NULL_HANDLE = idtb_null_id(proc->handle_table);
        handle_t h = NULL_HANDLE;
        plugin_kb_handle_state_t *hs_kb = NULL;

        if (err == FOS_E_SUCCESS) {
            h = idtb_pop_id(proc->handle_table);
            if (h == NULL_HANDLE) {
                err = FOS_E_NO_MEM;
            }
        }

        if (err == FOS_E_SUCCESS) {
            hs_kb = al_malloc(ks->al, sizeof(plugin_kb_handle_state_t));
            if (!hs_kb) {
                err = FOS_E_NO_MEM;
            }
        }

        if (err == FOS_E_SUCCESS) {
            err = mem_cpy_to_user(proc->pd, u_handle, &h, sizeof(handle_t), NULL);
        }

        if (err != FOS_E_SUCCESS) {
            idtb_push_id(proc->handle_table, h);
            al_free(ks->al, hs_kb);
            
            DUAL_RET(thr, err, FOS_E_SUCCESS);
        }

        // Success!

        init_base_handle((handle_state_t *)hs_kb, &KB_HS_IMPL, ks, proc, h);
        *(plugin_kb_t **)&(hs_kb->plg_kb) = plg_kb;
        hs_kb->pos = plg_kb->sc_buf_pos;

        idtb_set(proc->handle_table, h, hs_kb);

        DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
    }

    default: {
        DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
    }

    }
}
