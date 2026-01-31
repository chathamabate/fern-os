
#include "k_startup/plugin_gfx.h"
#include "k_startup/gfx.h"
#include "s_gfx/window_dummy.h"
#include "os_defs.h"

static fernos_error_t plg_gfx_kernel_cmd(plugin_t *plg, plugin_kernel_cmd_id_t kcmd, uint32_t arg0,
        uint32_t arg1, uint32_t arg2, uint32_t arg3);
static fernos_error_t plg_gfx_cmd(plugin_t *plg, plugin_cmd_id_t cmd,
        uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);

static fernos_error_t plg_gfx_tick(plugin_t *plg);

static const plugin_impl_t PLUGIN_GFX_IMPL = {
    .plg_on_shutdown = NULL,
    .plg_kernel_cmd = plg_gfx_kernel_cmd,
    .plg_cmd = plg_gfx_cmd,
    .plg_tick = plg_gfx_tick,
    .plg_on_fork_proc = NULL,
    .plg_on_reset_proc = NULL,
    .plg_on_reap_proc = NULL,
};

plugin_t *new_plugin_gfx(kernel_state_t *ks, window_t *root_window) {
    fernos_error_t err;

    plugin_gfx_t *plg_gfx = al_malloc(ks->al, sizeof(plugin_gfx_t));
    if (!plg_gfx) {
        return NULL;
    }

    // We actually allow the `root_window` resize to fail.
    // As long as the window is still active, we are good!
    err = win_resize(root_window, FERNOS_GFX_WIDTH, FERNOS_GFX_HEIGHT);
    if (err != FOS_E_INACTIVE) {
        err = win_fwd_event(root_window, (window_event_t) {
            .event_code = WINEC_FOCUSED,
        });
    }

    if (err == FOS_E_INACTIVE) {
        al_free(ks->al, plg_gfx);
        return NULL;
    }

    // Success!
    init_base_plugin((plugin_t *)plg_gfx, &PLUGIN_GFX_IMPL, ks);
    *(window_t **)&(plg_gfx->root_window) = root_window;

    return (plugin_t *)plg_gfx;
}

static fernos_error_t plg_gfx_kernel_cmd(plugin_t *plg, plugin_kernel_cmd_id_t kcmd, uint32_t arg0,
        uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    fernos_error_t err;

    (void)arg1;
    (void)arg2;
    (void)arg3;

    plugin_gfx_t *plg_gfx = (plugin_gfx_t *)plg;

    switch (kcmd) {

    case PLG_GFX_KCID_KEY_EVENT: {
        const scs1_code_t sc = (scs1_code_t)arg0;

        err = win_fwd_event(plg_gfx->root_window, (window_event_t) {
            .event_code = WINEC_KEY_INPUT,
            .d.key_code = sc
        });

        if (err == FOS_E_INACTIVE) {
            return FOS_E_INACTIVE;
        }

        return FOS_E_SUCCESS;
    }

    default: {
        return FOS_E_SUCCESS;
    }

    }
}

static fernos_error_t plg_gfx_cmd(plugin_t *plg, plugin_cmd_id_t cmd,
        uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    fernos_error_t err;

    (void)arg0;
    (void)arg1;
    (void)arg2;
    (void)arg3;

    plugin_gfx_t *plg_gfx = (plugin_gfx_t *)plg;
    kernel_state_t *ks = plg->ks;
    thread_t *curr_thr = (thread_t *)(ks->schedule.head);

    switch (cmd) {

    /*
     * Add a dummy window to the current root window!
     * Propagates errors to user thread.
     *
     * Really meant for debug of the root window.
     */
    case PLG_GFX_PCID_NEW_DUMMY: {
        window_t *dummy = new_window_dummy(ks->al);    
        if (!dummy) {
            DUAL_RET(curr_thr, FOS_E_NO_MEM, FOS_E_SUCCESS);
        }

        err = win_register_child(plg_gfx->root_window, dummy);
        if (err == FOS_E_INACTIVE) {
            return FOS_E_INACTIVE; // Major kernel error if root window goes 
                                   // inactive. (Don't even worry about cleanup)
        }

        if (err != FOS_E_SUCCESS) { // Non-fatal error we clean up dummy.
            delete_window(dummy); 
            DUAL_RET(curr_thr, err, FOS_E_SUCCESS);
        }

        // dummy windows are self managed.
        // The user doesn't get a reference back.

        DUAL_RET(curr_thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
    }

    default: {
        DUAL_RET(curr_thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
    }

    }
}

static fernos_error_t plg_gfx_tick(plugin_t *plg) {
    fernos_error_t err;

    plugin_gfx_t *plg_gfx = (plugin_gfx_t *)plg;

    // Here we send a tick to the root window.
    // Then render it.

    err = win_fwd_event(plg_gfx->root_window, (window_event_t) {
        .event_code = WINEC_TICK
    });
    if (err == FOS_E_INACTIVE) {
        return FOS_E_INACTIVE;
    }

    win_render(plg_gfx->root_window);
    gfx_to_screen(SCREEN, plg_gfx->root_window->buf);

    return FOS_E_SUCCESS;
}
