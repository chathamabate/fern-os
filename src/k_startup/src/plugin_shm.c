
#include "k_startup/plugin_shm.h"

static int32_t cmp_shm_range(const void *k0, const void *k1) {
    const void *s0 = ((plugin_shm_range_t *)k0)->start;
    const void *s1 = ((plugin_shm_range_t *)k1)->start;

    if (s0 < s1) {
        return -1;
    }

    if (s0 > s1) {
        return 1;
    }

    return 0;
}

static fernos_error_t plg_shm_cmd(plugin_t *plg, plugin_cmd_id_t cmd, uint32_t arg0, uint32_t arg1,
        uint32_t arg2, uint32_t arg3);
static fernos_error_t plg_shm_on_fork_proc(plugin_t *plg, proc_id_t cpid);
static fernos_error_t plg_shm_on_reset_proc(plugin_t *plg, proc_id_t pid);
static fernos_error_t plg_shm_on_reap_proc(plugin_t *plg, proc_id_t rpid);

static const plugin_impl_t PLUGIN_SHM_IMPL = {
    .plg_on_shutdown = NULL,
    .plg_kernel_cmd = NULL,
    .plg_cmd = plg_shm_cmd,
    .plg_tick = NULL,
    .plg_on_fork_proc = plg_shm_on_fork_proc,
    .plg_on_reset_proc = plg_shm_on_reset_proc,
    .plg_on_reap_proc =plg_shm_on_reap_proc 
};

plugin_t *new_plugin_shm(kernel_state_t *ks) {
    plugin_shm_t *plg_shm = al_malloc(ks->al, sizeof(plugin_shm_t));
    binary_search_tree_t *rt = new_simple_bst(ks->al, cmp_shm_range, sizeof(plugin_shm_range_t));

    if (!plg_shm || !rt) {
        delete_binary_search_tree(rt);
        al_free(ks->al, plg_shm);
        return NULL;
    }

    // Success!

    init_base_plugin((plugin_t *)plg_shm, &PLUGIN_SHM_IMPL, ks);
    *(binary_search_tree_t **)&(plg_shm->range_tree) = rt;

    for (size_t i = 0; i < FOS_MAX_PROCS; i++) {
        plg_shm->range_sets[i] = NULL;
    }

    return (plugin_t *)plg_shm;
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
