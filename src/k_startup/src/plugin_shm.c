
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

    kernel_state_t *ks = plg->ks;
    plugin_shm_t *plg_shm = (plugin_shm_t *)plg;

    thread_t *curr_thr = (thread_t *)(ks->schedule.head);

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

            err = mem_cpy_to_user(curr_thr->proc->pd, u_sem_id, &sem_id, sizeof(sem_id_t), NULL);
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
        sem->passes = sem_passes;
        *(basic_wait_queue_t **)&(sem->bwq) = bwq;
        sem->references = 1;

        idtb_set(plg_shm->sem_table, sem_id, sem);
        plg_shm->sem_vectors[curr_thr->proc->pid][sem_id / 8] |= 1 << (sem_id % 8);

        DUAL_RET(curr_thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
    }

    case PLG_SHM_PCID_SEM_ACQUIRE: {

    }

    case PLG_SHM_PCID_RELEASE: {

    }

    case PLG_SHM_PCID_SEM_CLOSE: {

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
