
#include "k_startup/plugin_shm.h"

static fernos_error_t plg_shm_cmd(plugin_t *plg, plugin_cmd_id_t cmd, uint32_t arg0, uint32_t arg1,
        uint32_t arg2, uint32_t arg3);
static fernos_error_t plg_shm_on_fork_proc(plugin_t *plg, proc_id_t cpid);
static fernos_error_t plg_shm_on_reset_proc(plugin_t *plg, proc_id_t pid);
static fernos_error_t plg_shm_on_reap_proc(plugin_t *plg, proc_id_t rpid);

static const plugin_impl_t PLUGIN_FUT_IMPL = {
    .plg_on_shutdown = NULL,
    .plg_kernel_cmd = NULL,
    .plg_cmd = plg_shm_cmd,
    .plg_tick = NULL,
    .plg_on_fork_proc = plg_shm_on_fork_proc,
    .plg_on_reset_proc = plg_shm_on_reset_proc,
    .plg_on_reap_proc =plg_shm_on_reap_proc 
};

plugin_t *new_plugin_shm(kernel_state_t *ks) {
    return NULL;
}

static fernos_error_t plg_shm_cmd(plugin_t *plg, plugin_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    return FOS_E_NOT_IMPLEMENTED;
}

static fernos_error_t plg_shm_on_fork_proc(plugin_t *plg, proc_id_t cpid) {
    return FOS_E_NOT_IMPLEMENTED;
}

static fernos_error_t plg_shm_on_reset_proc(plugin_t *plg, proc_id_t pid) {
    return FOS_E_NOT_IMPLEMENTED;
}

static fernos_error_t plg_shm_on_reap_proc(plugin_t *plg, proc_id_t rpid) {
    return FOS_E_NOT_IMPLEMENTED;
}
