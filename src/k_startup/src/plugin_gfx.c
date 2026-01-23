
#include "k_startup/plugin_gfx.h"
#include "k_startup/gfx.h"
#include "os_defs.h"

static fernos_error_t plg_gfx_kernel_cmd(plugin_t *plg, plugin_kernel_cmd_id_t kcmd, uint32_t arg0,
        uint32_t arg1, uint32_t arg2, uint32_t arg3);

static fernos_error_t plg_gfx_tick(plugin_t *plg);

static const plugin_impl_t PLUGIN_GFX_IMPL = {
    .plg_on_shutdown = NULL,
    .plg_kernel_cmd = plg_gfx_kernel_cmd,
    .plg_cmd = NULL,
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
    return FOS_E_SUCCESS;
}

static fernos_error_t plg_gfx_tick(plugin_t *plg) {
    return FOS_E_SUCCESS;
}
