
#include "k_startup/plugin_fut.h"

static bool fm_key_eq(const void *k0, const void *k1) {
    return *(const futex_t **)k0 == *(const futex_t **)k1;
}

static uint32_t fm_key_hash(const void *k) {
    const futex_t *futex = *(const futex_t **)k;

    // Kinda just a random hash function here.
    return (((uint32_t)futex + 1373) * 7) + 2;
}

static const plugin_impl_t PLUGIN_FUT_IMPL = {
    .plg_on_shutdown = NULL,
    .plg_kernel_cmd = NULL,
    .plg_cmd = NULL,
    .plg_tick = NULL,
    .plg_on_fork_proc = NULL,
    .plg_on_reap_proc = NULL
};

plugin_t *new_plugin_fut(kernel_state_t *ks) {
    plugin_fut_t *plg_fut = al_malloc(ks->al, sizeof(plugin_fut_t));
    if (!plg_fut) {
        return NULL;
    }

    init_base_plugin((plugin_t *)plg_fut, &PLUGIN_FUT_IMPL, ks);

    for (size_t i = 0; i < FOS_MAX_PROCS; i++) {
        plg_fut->fut_maps[i] = NULL;  // Set all maps to NULL at start time.
    }

    bool alloc_failure = false;

    for (proc_id_t pid = 0; pid < FOS_MAX_PROCS; pid++) {
        if (idtb_get(ks->proc_table, pid)) { // Is there a process at `pid`?
            map_t *fut_map = new_chained_hash_map(ks->al, sizeof(futex_t *), sizeof(basic_wait_queue_t *),
                    3, fm_key_eq, fm_key_hash);

            if (!fut_map) {
                alloc_failure = true;
                break;
            }

            plg_fut->fut_maps[pid] = fut_map;
        }
    }

    if (alloc_failure) {
        for (size_t i = 0; i < FOS_MAX_PROCS; i++) {
            delete_map(plg_fut->fut_maps[i]); // NULL pass through.
        }
        al_free(ks->al, plg_fut);
        return NULL;
    }

    // Success!
    return (plugin_t *)plg_fut;
}
