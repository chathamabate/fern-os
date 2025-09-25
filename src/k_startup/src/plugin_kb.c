
#include "k_startup/plugin_kb.h"
#include "s_util/ps2_scancodes.h"

static fernos_error_t plg_kb_kernel_cmd(plugin_t *plg, plugin_kernel_cmd_id_t kcmd, uint32_t arg0,
        uint32_t arg1, uint32_t arg2, uint32_t arg3);

static const plugin_impl_t PLUGIN_KB_IMPL = {
    .plg_on_shutdown = NULL,
    .plg_kernel_cmd = plg_kb_kernel_cmd,
    .plg_cmd = NULL,
    .plg_tick = NULL,
    .plg_on_fork_proc = NULL,
    .plg_on_reap_proc = NULL,
};

plugin_t *new_plugin_kb(kernel_state_t *ks) {
    plugin_kb_t *plg_kb = al_malloc(ks->al, sizeof(plugin_kb_t));
    if (!plg_kb) {
        return NULL;
    }

    init_base_plugin((plugin_t *)plg_kb, &PLUGIN_KB_IMPL, ks);

    return (plugin_t *)plg_kb;
}

#include "k_startup/vga_cd.h"

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
