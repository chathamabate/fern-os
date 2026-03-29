
#include "k_startup/plugin_shm.h"
#include "s_util/str.h"
#include "k_startup/page_helpers.h"
#include "k_startup/process.h"

#if KS_SEM_MAX_SEMS % 8 != 0
#error "Semaphore limit must be a multiple of 8!"
#endif

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
    id_table_t *st = new_id_table(ks->al, KS_SEM_MAX_SEMS);
    
    // binary_search_tree_t *rt = new_simple_bst(ks->al, cmp_shm_range, sizeof(plugin_shm_range_t));

    if (!plg_shm || !st /*|| !rt*/) {
        //delete_binary_search_tree(rt);
        delete_id_table(st);
        al_free(ks->al, plg_shm);
        return NULL;
    }

    // Success!

    init_base_plugin((plugin_t *)plg_shm, &PLUGIN_SHM_IMPL, ks);
    *(id_table_t **)&(plg_shm->sem_table) = st;
    mem_set(plg_shm->sem_vectors, 0, sizeof(plg_shm->sem_vectors));

    /*
    *(binary_search_tree_t **)&(plg_shm->range_tree) = rt;
    for (size_t i = 0; i < FOS_MAX_PROCS; i++) {
        plg_shm->range_sets[i] = NULL;
    }
    */

    return (plugin_t *)plg_shm;
}

static fernos_error_t plg_shm_cmd(plugin_t *plg, plugin_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    fernos_error_t err;

    kernel_state_t * const ks = plg->ks;
    plugin_shm_t * const plg_shm = (plugin_shm_t *)plg;

    thread_t * const curr_thr = (thread_t *)(ks->schedule.head);
    process_t * const curr_proc = curr_proc;

    (void)arg1;
    (void)arg2;
    (void)arg3;

    switch (cmd) {

    /**
     * Create a new sempahore!
     *
     * Returns FOS_E_BAD_ARGS if `arg0` is NULL or if copying to `arg0` fails.
     * Returns FOS_E_BAD_ARGS if `arg1` is 0.
     * Returns FOS_E_NO_SPACE if the sempahore table is already full.
     * Returns FOS_E_NO_MEM if we fail to allocate the sempahore state.
     *
     * `arg0` - User pointer to a `sem_id_t`.
     * `arg1` - Semaphore number of passes.
     */
    case PLG_SHM_PCID_NEW_SEM: {
        err = FOS_E_SUCCESS;

        sem_id_t * const u_sem_id = (sem_id_t *)arg0;
        const uint32_t sem_passes = arg1;

        const sem_id_t NULL_SEM_ID = idtb_null_id(plg_shm->sem_table);
        sem_id_t sem_id = NULL_SEM_ID;
        basic_wait_queue_t *bwq = NULL;
        plugin_shm_sem_t *sem = NULL; 

        if (!u_sem_id || sem_passes == 0) {
            err = FOS_E_BAD_ARGS;
        }

        // Allocate semaphore ID.
        if (err == FOS_E_SUCCESS) {
            sem_id = idtb_pop_id(plg_shm->sem_table);
            if (sem_id == NULL_SEM_ID) {
                err = FOS_E_NO_SPACE;
            }
        }
        
        // Allocate semaphore basic wait queue.
        if (err == FOS_E_SUCCESS) {
            bwq = new_basic_wait_queue(ks->al);
            if (!bwq) {
                err = FOS_E_NO_MEM;
            }
        }

        // Allocate semaphore state structure.
        if (err == FOS_E_SUCCESS) {
            sem = al_malloc(ks->al, sizeof(plugin_shm_sem_t));
            if (!sem) {
                err = FOS_E_NO_MEM;
            }
        }

        if (err == FOS_E_SUCCESS) {
            // If we've made it here, all allocations have succeeded!
            // As long as we successfully write back the allocated ID, we are in the clear!

            err = mem_cpy_to_user(curr_proc->pd, u_sem_id, &sem_id, sizeof(sem_id_t), NULL);
            if (err != FOS_E_SUCCESS) {
                err = FOS_E_BAD_ARGS;
            }
        }

        if (err != FOS_E_SUCCESS) {
            al_free(ks->al, sem);
            delete_wait_queue((wait_queue_t *)bwq);
            idtb_push_id(plg_shm->sem_table, sem_id);

            DUAL_RET(curr_thr, err, FOS_E_SUCCESS);
        }

        // Success! setup our semaphore structure!
        *(sem_id_t *)&(sem->id) = sem_id;
        *(uint32_t *)&(sem->max_passes) = sem_passes;
        sem->passes = sem_passes;
        *(basic_wait_queue_t **)&(sem->bwq) = bwq;
        sem->references = 1;

        idtb_set(plg_shm->sem_table, sem_id, sem);
        plg_shm->sem_vectors[curr_proc->pid][sem_id / 8] |= 1 << (sem_id % 8);

        DUAL_RET(curr_thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
    }

    /**
     * This attempts to decrement the sempahore's internal pass counter!
     *
     * If the pass counter is 0, this call blocks the current thread until pass counter becomes
     * non-zero.
     *
     * Returns FOS_E_BAD_ARGS if the given semaphore id doesn't reference a semaphore available to 
     * the calling process.
     * Returns FOS_E_SUCCESS when the semaphore has been successfully decremented.
     * Returns FOS_E_NO_MEM if we fail to add our thread to the wait queue in the waiting case.
     *
     * `arg0` - The id of the semaphore we are trying to acquire.
     */
    case PLG_SHM_PCID_SEM_DEC: {
        const sem_id_t sem_id = (sem_id_t)arg0;

        if (sem_id >= KS_SEM_MAX_SEMS) {
            DUAL_RET(curr_thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
        }

        // Does this proccess even have access `sem_id` though?
        if (!(plg_shm->sem_vectors[curr_proc->pid][sem_id / 8] & (1 << (sem_id % 8)))) {
            DUAL_RET(curr_thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
        }

        plugin_shm_sem_t * const sem = idtb_get(plg_shm->sem_table, sem_id);
        if (!sem) {
            return FOS_E_STATE_MISMATCH; // Very bad.
        }

        if (sem->passes > 0) {
            sem->passes--;

            DUAL_RET(curr_thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
        }

        // Semaphore counter is 0, we must wait :,(

        err = bwq_enqueue(sem->bwq, curr_thr);
        if (err != FOS_E_SUCCESS) {
            DUAL_RET(curr_thr, FOS_E_NO_MEM, FOS_E_SUCCESS);
        }

        thread_detach(curr_thr);
        curr_thr->state = THREAD_STATE_WAITING;
        curr_thr->wq = (wait_queue_t *)(sem->bwq);

        return FOS_E_SUCCESS;
    }

    /**
     * Increment a semaphore!
     *
     * If the semaphore's current value is zero, then incrementing will wake up one waiting thread!
     * (If there are any)
     * 
     * This is a destructor style call, and thus will return nothing!
     * Just remember that incrementing will NEVER push the semaphore pass count past it's max value!
     *
     * `arg0` - The id of the semaphore to increment.
     */
    case PLG_SHM_PCID_SEM_INC: {
        const sem_id_t sem_id = (sem_id_t)arg0;

        if (sem_id >= KS_SEM_MAX_SEMS) {
            return FOS_E_SUCCESS;
        }

        // Does this proccess even have access `sem_id` though?
        if (!(plg_shm->sem_vectors[curr_proc->pid][sem_id / 8] & (1 << (sem_id % 8)))) {
            return FOS_E_SUCCESS;
        }

        plugin_shm_sem_t * const sem = idtb_get(plg_shm->sem_table, sem_id);
        if (!sem) {
            return FOS_E_STATE_MISMATCH; // Very bad.
        }

        if (sem->passes == sem->max_passes) {
            return FOS_E_SUCCESS;
        }

        sem->passes++;

        if (sem->passes == 1) { // If we went from 0 to 1, wake up!
            err = bwq_notify_next(sem->bwq);
            if (err != FOS_E_SUCCESS) {
                return err; // We'll just say a failure to notify is system fatal to make my life
                            // easier.
            }

            thread_t *woken_thr;
            err = bwq_pop(sem->bwq, (void **)&woken_thr);
            if (err == FOS_E_SUCCESS) {
                // We successfully popped a waiting thread, it will take the pass just returned!
                sem->passes--;

                woken_thr->state = THREAD_STATE_DETATCHED;
                woken_thr->wq = NULL;
                mem_set(woken_thr->wait_ctx, 0, sizeof(woken_thr->wait_ctx)); // Not really necessary, but whatever.
                woken_thr->ctx.eax = FOS_E_SUCCESS;

                thread_schedule(woken_thr, &(ks->schedule));
            }

            if (err != FOS_E_EMPTY) {
                return err;
            }

            // NOTE: if `err` was FOS_E_EMPTY, this indicates there were no waiting thread!
            // This is totally fine, `sem->passes` will retain its new value of 1.
        }

        return FOS_E_SUCCESS;
    }

    /**
     * Close a semaphore!
     *
     * This is a destructor style call, and thus returns nothing.
     *
     * `arg0` - the ID of the semaphore to close.
     *
     * NOTE: The underlying semaphore is only actually deleted if its reference count reaches 0.
     * NOTE: If the calling process has threads which are currently in the given semaphore's 
     * wait queue, said threads are woken up with return code `FOS_E_STATE_MISMATCH`.
     */
    case PLG_SHM_PCID_SEM_CLOSE: {
        const sem_id_t sem_id = (sem_id_t)arg0;

        if (sem_id >= KS_SEM_MAX_SEMS) {
            return FOS_E_SUCCESS;
        }

        // Do we even have access to this semaphore?
        if (!(plg_shm->sem_vectors[curr_proc->pid][sem_id / 8] & (1 << (sem_id % 8)))) {
            return FOS_E_SUCCESS;
        }

        plugin_shm_sem_t *sem = (plugin_shm_sem_t *)(idtb_get(plg_shm->sem_table, sem_id));
        if (!sem) {
            return FOS_E_STATE_MISMATCH;
        }

        // When we close a semaphore, we first detach all of the calling process's current threads
        // from the semaphore with return code FOS_E_STATE_MISMATCH.

        const thread_id_t NULL_TID = idtb_null_id(curr_proc->thread_table);
        idtb_reset_iterator(curr_proc->thread_table);
        for (thread_id_t tid = idtb_get_iter(curr_proc->thread_table); tid != NULL_TID; 
                tid = idtb_next(curr_proc->thread_table)) {
            thread_t *thr = (thread_t *)idtb_get(curr_proc->thread_table, tid);

            if (thr->state == THREAD_STATE_WAITING && thr->wq == (wait_queue_t *)(sem->bwq)) {
                thread_schedule(thr, &(ks->schedule));
                thr->ctx.eax = FOS_E_STATE_MISMATCH;
            }
        }

        // Now we remove semaphore from reference vector.
        
        plg_shm->sem_vectors[curr_proc->pid][sem_id / 8] &= ~(1 << (sem_id % 8));

        // Finally decrement semaphore counter! (And potentially delete)
        
        if (--(sem->references) == 0) {
            delete_wait_queue((wait_queue_t *)(sem->bwq));
            al_free(ks->al, sem);
            idtb_push_id(plg_shm->sem_table, sem_id);
        }

        return FOS_E_SUCCESS;
    }

    default: {
        DUAL_RET(curr_thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
    }

    }
}

static fernos_error_t plg_shm_on_fork_proc(plugin_t *plg, proc_id_t cpid) {
    (void)plg;
    (void)cpid;

    return FOS_E_NOT_IMPLEMENTED;
}

static fernos_error_t plg_shm_on_reset_proc(plugin_t *plg, proc_id_t pid) {
    (void)plg;
    (void)pid;

    return FOS_E_NOT_IMPLEMENTED;
}

static fernos_error_t plg_shm_on_reap_proc(plugin_t *plg, proc_id_t rpid) {
    (void)plg;
    (void)rpid;

    return FOS_E_NOT_IMPLEMENTED;
}
