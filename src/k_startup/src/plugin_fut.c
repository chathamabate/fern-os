
#include "k_startup/plugin_fut.h"
#include "k_startup/process.h"
#include "k_startup/page_helpers.h"

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
    (void)arg2;
    (void)arg3;

    fernos_error_t err;

    plugin_fut_t *plg_fut = (plugin_fut_t *)plg;
    kernel_state_t *ks = plg->ks;

    thread_t *curr_thr = (thread_t *)(ks->schedule.head);
    process_t *proc = curr_thr->proc;

    map_t *fut_map = plg_fut->fut_maps[proc->pid];
    if (!fut_map) {
        return FOS_E_STATE_MISMATCH;
    }


    switch (cmd) {

    /*
     * Register a futex with the futex plugin.
     *
     * A futex is a number, which is mapped to a wait queue in this plugin.
     *
     * Threads will be able to wait while the futex holds a specific value.
     *
     * Returns an error if there are insufficient resources, if futex is NULL,
     * or if the futex is already in use!
     */
    case PLG_FUT_PCID_REGISTER: {
        futex_t *u_fut = (futex_t *)arg0;

        if (!u_fut) {
            DUAL_RET(curr_thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS); 
        }

        if (mp_get(fut_map, &u_fut)) {
            DUAL_RET(curr_thr, FOS_E_ALREADY_ALLOCATED, FOS_E_SUCCESS);
        }

        // Do we have access to this futex?
        futex_t test;
        err = mem_cpy_from_user(&test, proc->pd, u_fut, sizeof(futex_t), NULL);
        if (err != FOS_E_SUCCESS) {
            DUAL_RET(curr_thr, FOS_E_INVALID_INDEX, FOS_E_SUCCESS);
        }

        basic_wait_queue_t *bwq = new_basic_wait_queue(proc->al);
        if (!bwq) {
            DUAL_RET(curr_thr, FOS_E_NO_MEM, FOS_E_SUCCESS);
        }

        err = mp_put(fut_map, &u_fut, &bwq);
        if (err != FOS_E_SUCCESS) {
            delete_wait_queue((wait_queue_t *)bwq);
            DUAL_RET(curr_thr, FOS_E_NO_MEM, FOS_E_SUCCESS);
        }

        DUAL_RET(curr_thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
    }

    /*
     * Deregister a futex of the current process.
     *
     * Doesn't return a user error.
     *
     * If threads are currently waiting on this futex, they are rescheduled with return value
     * FOS_E_STATE_MISMATCH.
     *
     * Returns an error if there is some issue with the deregister.
     */
	case PLG_FUT_PCID_DEREGISTER: {
        futex_t *u_fut = (futex_t *)arg0;

        if (!u_fut) {
            return FOS_E_SUCCESS;
        }

        basic_wait_queue_t **_bwq = mp_get(fut_map, &u_fut);
        if (!_bwq) {
            return FOS_E_SUCCESS; // Unmapped futex doesn't do anything.
        }

        basic_wait_queue_t *bwq = *_bwq;
        if (!bwq) {
            return FOS_E_STATE_MISMATCH; // something very wrong if this is the case.
        }

        // Now we are going to wake up all threads which may have been waiting on this futex!
        err = bwq_notify_all(bwq);
        if (err != FOS_E_SUCCESS) {
            return err;
        }

        thread_t *woken_thread;
        while ((err = bwq_pop(bwq, (void **)&woken_thread)) == FOS_E_SUCCESS) {
            woken_thread->wq = NULL;
            woken_thread->ctx.eax = FOS_E_STATE_MISMATCH;
            woken_thread->state = THREAD_STATE_DETATCHED;

            thread_schedule(woken_thread, &(ks->schedule));
        }

        // Some sort of error with the popping.
        if (err != FOS_E_EMPTY) {
            return err;
        }

        // At this point the wait queue should be totally empty,
        // let's delete it and remove it from the map!

        delete_wait_queue((wait_queue_t *)bwq);
        mp_remove(fut_map, &u_fut);

        return FOS_E_SUCCESS;
	}

    /*
     * This command will check if the futex's value = exp_val.
     *
     * If the values match as expected, the calling thread will be put to sleep.
     * If the values don't match, this call will return immediately with FOS_E_SUCCESS.
     *
     * When a thread is put to sleep it can only be rescheduled by an `sc_futex_wake` call.
     * This will also return FOS_E_SUCCESS.
     *
     * This call returns FOS_E_STATE_MISMATCH if the futex is deregistered mid wait!
     *
     * This call return other errors if something goes wrong or if the given futex doesn't exist!
     */
	case PLG_FUT_PCID_WAIT: {
        futex_t *u_fut = (futex_t *)arg0;
        futex_t exp_val = (futex_t)arg1;

        if (!u_fut) {
            DUAL_RET(curr_thr, FOS_E_INVALID_INDEX, FOS_E_SUCCESS);
        }

        basic_wait_queue_t **_bwq = mp_get(fut_map, &u_fut);
        if (!_bwq) {
            DUAL_RET(curr_thr, FOS_E_INVALID_INDEX, FOS_E_SUCCESS);
        }

        basic_wait_queue_t *bwq = *_bwq;
        if (!bwq) {
            return FOS_E_STATE_MISMATCH; // very bad!
        }

        // Now let's try to read out the actual value.
        futex_t act_val;
        err = mem_cpy_from_user(&act_val, proc->pd, u_fut, sizeof(futex_t), NULL);
        if (err != FOS_E_SUCCESS) {
            return err; 
        }

        // Values don't match is a success!
        DUAL_RET_COND(act_val != exp_val, curr_thr, FOS_E_SUCCESS, FOS_E_SUCCESS);

        // Values match, let's attempt to queue up our curr thread.
        err = bwq_enqueue(bwq, curr_thr);
        DUAL_RET_FOS_ERR(err, curr_thr); // An error to enqueue is actually recoverable!

        thread_detach(curr_thr);
        curr_thr->wq = (wait_queue_t *)bwq;
        curr_thr->state = THREAD_STATE_WAITING;

        return FOS_E_SUCCESS;
	}

    /*
     * Wake up one or all threads waiting on a futex.
     *
     * Returns user error if arguements are invalid.
     */
	case PLG_FUT_PCID_WAKE: {
        futex_t *u_fut = (futex_t *)arg0;
        bool all = (bool)arg1;

        if (!u_fut) {
            DUAL_RET(curr_thr, FOS_E_INVALID_INDEX, FOS_E_SUCCESS);
        }

        basic_wait_queue_t **_bwq = mp_get(fut_map, &u_fut);
        if (!_bwq) {
            DUAL_RET(curr_thr, FOS_E_INVALID_INDEX, FOS_E_SUCCESS);
        }

        basic_wait_queue_t *bwq = *_bwq;
        if (!bwq) {
            return FOS_E_STATE_MISMATCH; // very bad!
        }

        bwq_notify_mode_t mode = all ? BWQ_NOTIFY_ALL : BWQ_NOTIFY_NEXT;

        err = bwq_notify(bwq, mode);
        if (err != FOS_E_SUCCESS) {
            return err; // fatal kernel error.
        }

        // Regardless of the mode, we'll just wake up in a loop. In the case of "NOTIFY_NEXT",
        // this loop should only iterate at most once.

        thread_t *woken_thread;
        while ((err = bwq_pop(bwq, (void **)&woken_thread)) == FOS_E_SUCCESS) {
            woken_thread->wq = NULL;
            woken_thread->ctx.eax = FOS_E_SUCCESS;
            woken_thread->state = THREAD_STATE_DETATCHED;

            thread_schedule(woken_thread, &(ks->schedule));
        }

        if (err != FOS_E_EMPTY) {
            return err;
        }
		
        DUAL_RET(curr_thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
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
