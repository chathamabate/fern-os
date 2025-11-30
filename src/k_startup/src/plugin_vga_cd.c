

#include "k_startup/plugin_vga_cd.h"
#include "k_startup/plugin.h"
#include "k_startup/handle.h"
#include "s_bridge/shared_defs.h"
#include "k_startup/handle_cd.h"
#include "k_startup/vga_cd.h"
#include "k_startup/page_helpers.h"

static fernos_error_t vga_cd_plg_cmd(plugin_t *plg, plugin_cmd_id_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);

static const plugin_impl_t PLUGIN_VGA_CD_IMPL = {
    .plg_on_shutdown = NULL,
    .plg_kernel_cmd = NULL,
    .plg_cmd = vga_cd_plg_cmd,
    .plg_tick = NULL,
    .plg_on_fork_proc = NULL,
    .plg_on_reset_proc = NULL,
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

    thread_t *thr = (thread_t *)(plg->ks->schedule.head);
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
            vga_cd_hs = new_handle_cd_state(plg->ks, proc, h, VGA_CD);
            if (!vga_cd_hs) {
                err = FOS_E_NO_MEM;
            }
        }

        if (err == FOS_E_SUCCESS) {
            err = mem_cpy_to_user(proc->pd, u_handle, &h, sizeof(handle_t), NULL);
        }

        if (err != FOS_E_SUCCESS) {
            delete_handle_state(vga_cd_hs);
            idtb_push_id(proc->handle_table, h);

            DUAL_RET(thr, err, FOS_E_SUCCESS);
        }

        // Success!

        idtb_set(proc->handle_table, h, vga_cd_hs);

        DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
    } 

    DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
}
