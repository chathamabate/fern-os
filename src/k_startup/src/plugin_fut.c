
#include "k_startup/plugin_fut.h"

static bool fm_key_eq(const void *k0, const void *k1) {
    return *(const futex_t **)k0 == *(const futex_t **)k1;
}

static uint32_t fm_key_hash(const void *k) {
    const futex_t *futex = *(const futex_t **)k;

    // Kinda just a random hash function here.
    return (((uint32_t)futex + 1373) * 7) + 2;
}

static fernos_error_t plg_fut_cmd(plugin_t *plg, plugin_cmd_id_t cmd, uint32_t arg0, uint32_t arg1,
        uint32_t arg2, uint32_t arg3);
static fernos_error_t plg_fut_on_fork_proc(plugin_t *plg, proc_id_t cpid);
static fernos_error_t plg_fut_on_reap_proc(plugin_t *plg, proc_id_t rpid);

static const plugin_impl_t PLUGIN_FUT_IMPL = {
    .plg_on_shutdown = NULL,
    .plg_kernel_cmd = NULL,
    .plg_cmd = plg_fut_cmd,
    .plg_tick = NULL,
    .plg_on_fork_proc = plg_fut_on_fork_proc,
    .plg_on_reap_proc =plg_fut_on_reap_proc 
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

static fernos_error_t plg_fut_cmd(plugin_t *plg, plugin_cmd_id_t cmd, uint32_t arg0, uint32_t arg1,
        uint32_t arg2, uint32_t arg3) {
    plugin_fut_t *plg_fut = (plugin_fut_t *)plg;
    kernel_state_t *ks = plg->ks;

    thread_t *curr_thr = (thread_t *)(ks->schedule.head);


    switch (cmd) {

    case PLG_FUT_PCID_REGISTER: {

    }

	case PLG_FUT_PCID_DEREGISTER: {
		
	}

	case PLG_FUT_PCID_WAIT: {
		
	}

	case PLG_FUT_PCID_WAKE: {
		
	}

    default: {
        DUAL_RET(curr_thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
    }

    }
}

static fernos_error_t plg_fut_on_fork_proc(plugin_t *plg, proc_id_t cpid) {
    plugin_fut_t *plg_fut = (plugin_fut_t *)plg;

    // On fork, futexes are NOT copied over. Forking from a multithreaded process is seen as kinda
    // dicey. The child process is just given a fresh map, that's all!

    if (plg_fut->fut_maps[cpid]) {
        return FOS_E_STATE_MISMATCH;
    }

    map_t *fut_map = new_chained_hash_map(plg->ks->al, sizeof(futex_t *), sizeof(basic_wait_queue_t *),
            3, fm_key_eq, fm_key_hash);

    // It would be nice if we could return an error which stops the fork, but whatever,
    // we'll just shutdown the system when kernel memory runs out.
    if (!fut_map) {
        return FOS_E_NO_MEM;
    }

    plg_fut->fut_maps[cpid] = fut_map;

    return FOS_E_SUCCESS;
}

static fernos_error_t plg_fut_on_reap_proc(plugin_t *plg, proc_id_t rpid) {
    plugin_fut_t *plg_fut = (plugin_fut_t *)plg;

    map_t *fut_map = plg_fut->fut_maps[rpid];
    plg_fut->fut_maps[rpid] = NULL;

    if (!fut_map) {
        return FOS_E_STATE_MISMATCH; // something is very wrong if this happens!
    }

    // Now, delete all basic wait queues? Remember, this is OK. When a process exits, all threads
    // are detached. So, if a process is being reaped, none of its threads will be in the wait
    // queues we are about to delete!

    mp_reset_iter(fut_map);
    
    basic_wait_queue_t **bwq;
    for (fernos_error_t err = mp_get_iter(fut_map, NULL, (void **)&bwq); err != FOS_E_EMPTY;
            err = mp_next_iter(fut_map, NULL, (void **)&bwq)) {
        delete_wait_queue((wait_queue_t *)*bwq);
    }

    // Finally, delete the map itself!
    delete_map(fut_map);

    return FOS_E_SUCCESS;
}
