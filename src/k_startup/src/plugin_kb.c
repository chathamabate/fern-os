
#include "k_startup/plugin_kb.h"
#include "s_util/ps2_scancodes.h"
#include "k_startup/vga_cd.h"
#include "k_startup/page_helpers.h"

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
 * This function will return FOS_E_BAD_ARGS if the value of `len` is 1, as it cannot separate 
 * a scancode. Higher odd values of `len` are OK.
 */
static fernos_error_t kb_hs_read(handle_state_t *hs, void *u_dest, size_t len, size_t *u_readden) {
    fernos_error_t err;
    plugin_kb_handle_state_t *plg_kb_hs = (plugin_kb_handle_state_t *)hs;
    plugin_kb_t *plg_kb = plg_kb_hs->plg_kb;

    kernel_state_t *ks = plg_kb_hs->super.ks;
    thread_t *thr = ks->curr_thread;
    process_t *proc = thr->proc;

    if (!u_dest || !u_readden || len == 1) {
        DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
    }

    // Here we are caught up with the global buffer, nothing to read!
    if (plg_kb_hs->pos == plg_kb->sc_buf_pos) {
        DUAL_RET(thr, FOS_E_EMPTY, FOS_E_SUCCESS);
    }

    size_t codes_to_read = len / sizeof(uint16_t);
    size_t codes_read = 0;
    uint16_t read_scancodes[PLG_KB_BUFFER_SIZE];

    // Remember, the global buffer is cyclic.
    if (codes_to_read > 0 && plg_kb_hs->pos > plg_kb->sc_buf_pos) {
        size_t first_copy_amt = PLG_KB_BUFFER_SIZE - plg_kb_hs->pos;
        if (first_copy_amt > codes_to_read) {
            first_copy_amt = codes_to_read;
        }

        mem_cpy(read_scancodes, &(plg_kb->sc_buf[plg_kb_hs->pos]), first_copy_amt * sizeof(uint16_t));

        plg_kb_hs->pos += first_copy_amt;
        if (plg_kb_hs->pos == PLG_KB_BUFFER_SIZE) {
            plg_kb_hs->pos = 0;
        }

        codes_to_read -= first_copy_amt;
        codes_read += first_copy_amt;
    }

    // If the if statement above ran, it's possible the handle position and the global position
    // are both now 0, in which case there is no more to read. So, we will check if the 
    // handle position is less than the global position here.
    if (codes_to_read > 0 && plg_kb_hs->pos < plg_kb->sc_buf_pos) {
        size_t second_copy_amt = plg_kb->sc_buf_pos - plg_kb_hs->pos;
        if (second_copy_amt > codes_to_read) {
            second_copy_amt = codes_to_read;
        }

        mem_cpy(&(read_scancodes[codes_read]), &(plg_kb->sc_buf[plg_kb_hs->pos]), second_copy_amt * sizeof(uint16_t));

        // At most, this will bring pos directly up to the global position.
        // Thus, we don't need to check if pos == The Buffer size like we did in the other
        // if statement.
        plg_kb_hs->pos += second_copy_amt;

        codes_to_read -= second_copy_amt;
        codes_read += second_copy_amt;
    }

    // Finally, copy to userspace in one go.
    
    err = mem_cpy_to_user(proc->pd, u_dest, read_scancodes, codes_read * sizeof(uint16_t), NULL);
    DUAL_RET_FOS_ERR(err, thr);

    size_t bytes_read = codes_read * sizeof(uint16_t);
    err = mem_cpy_to_user(proc->pd, u_readden, &bytes_read, sizeof(size_t), NULL);
    DUAL_RET_FOS_ERR(err, thr);


    DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
}

static fernos_error_t kb_hs_wait(handle_state_t *hs) {

}

static fernos_error_t kb_hs_cmd(handle_state_t *hs, handle_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, 
        uint32_t arg2, uint32_t arg3) {

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
    //plugin_kb_t *plg_kb = (plugin_kb_t *)plg;

    (void)plg;
    (void)arg1;
    (void)arg2;
    (void)arg3;

    scs1_code_t sc;

    switch (kcmd) {

    /*
     * A Key Event is either a key press or key release. (Using PS/2 Scancode Set 1)
     *
     * If a normal key event, `arg0` should be the scancode.
     * If a key event of an extended key, `arg0` should be 0xE0 and `arg1` should be the scancode.
     */
    case PLG_KB_KCID_KEY_EVENT: {
        sc = (scs1_code_t)arg0;

        if (sc == SCS1_ENTER) {
            term_put_c('\n');
        } else if (sc == SCS1_E_DOWN) {
            term_put_c('\n');
        } else {
            char c = scs1_to_ascii_lc(sc);
            if (c) {
                term_put_c(c);
            }
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

}
