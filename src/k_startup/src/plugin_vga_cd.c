

#include "k_startup/plugin_vga_cd.h"
#include "k_startup/plugin.h"
#include "k_startup/handle.h"
#include "k_startup/page_helpers.h"
#include "s_bridge/shared_defs.h"

static fernos_error_t copy_vga_cd_handle_state(handle_state_t *hs, process_t *proc, handle_state_t **out);
static fernos_error_t delete_vga_cd_handle_state(handle_state_t *hs);
static fernos_error_t vga_cd_hs_write(handle_state_t *hs, const void *u_src, size_t len, size_t *u_written);

const handle_state_impl_t VGA_CD_HS_IMPL = {
    .copy_handle_state = copy_vga_cd_handle_state,
    .delete_handle_state = delete_vga_cd_handle_state,
    .hs_write = vga_cd_hs_write,
    .hs_read = NULL,
    .hs_wait = NULL,
    .hs_cmd = NULL
};

static fernos_error_t copy_vga_cd_handle_state(handle_state_t *hs, process_t *proc, handle_state_t **out) {
    handle_state_t *hs_copy = al_malloc(hs->ks->al, sizeof(handle_state_t));
    if (!hs_copy) {
        return FOS_E_NO_MEM;
    }

    init_base_handle(hs_copy, &VGA_CD_HS_IMPL, hs->ks, proc, hs->handle);
    *out = hs_copy;
    return FOS_E_SUCCESS;
}

static fernos_error_t delete_vga_cd_handle_state(handle_state_t *hs) {
    al_free(hs->ks->al, hs);
    return FOS_E_SUCCESS;
}

static fernos_error_t vga_cd_hs_write(handle_state_t *hs, const void *u_src, size_t len, size_t *u_written) {
    // If it's too large we'll need to back track to see if there are any control sequences...
    // Make sure not to print those.
}

static fernos_error_t vga_cd_plg_cmd(plugin_t *plg, plugin_cmd_id_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);

static const plugin_impl_t PLUGIN_VGA_CD_IMPL = {
    .plg_on_shutdown = NULL,
    .plg_kernel_cmd = NULL,
    .plg_cmd = vga_cd_plg_cmd,
    .plg_tick = NULL,
    .plg_on_fork_proc = NULL,
    .plg_on_reap_proc = NULL,
};

/**
 * This just returns a pointer to the singleton.
 */
plugin_t *new_plugin_vga_cd(kernel_state_t *ks) {
    plugin_t *vga_cd_plg = al_malloc(ks->al, sizeof(plugin_t));
    if (!vga_cd_plg) {
        return NULL;
    }

    init_base_plugin(vga_cd_plg, &PLUGIN_VGA_CD_IMPL, ks);

    return vga_cd_plg;
}

static fernos_error_t vga_cd_plg_cmd(plugin_t *plg, plugin_cmd_id_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    fernos_error_t err = FOS_E_SUCCESS;

    (void)arg1;
    (void)arg2;
    (void)arg3;

    thread_t *thr = plg->ks->curr_thread;
    process_t *proc = thr->proc;

    if (cmd_id == PLG_VGA_CD_PCID_OPEN) {
        handle_t *u_handle = (handle_t *)arg0;

        if (!u_handle) {
            DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
        }

        const handle_t NULL_HANDLE = idtb_null_id(proc->handle_table);
        handle_t h = idtb_pop_id(proc->handle_table);
        if (h == NULL_HANDLE) {
            err = FOS_E_EMPTY;
        }

        handle_state_t *vga_cd_hs = NULL;
        if (err == FOS_E_SUCCESS) {
            vga_cd_hs = al_malloc(plg->ks->al, sizeof(handle_state_t));
            if (!vga_cd_hs) {
                err = FOS_E_NO_MEM;
            }
        }

        if (err == FOS_E_SUCCESS) {
            init_base_handle(vga_cd_hs, &VGA_CD_HS_IMPL, plg->ks, proc, h);
            err = mem_cpy_to_user(proc->pd, u_handle, &h, sizeof(handle_t), NULL);
        }

        if (err != FOS_E_SUCCESS) {
            al_free(plg->ks->al, vga_cd_hs);
            idtb_push_id(proc->handle_table, h);

            DUAL_RET(thr, err, FOS_E_SUCCESS);
        }

        // Success!

        idtb_set(proc->handle_table, h, vga_cd_hs);

        DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
    } 

    DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
}
