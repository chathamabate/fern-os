
#include "k_startup/plugin_shm.h"
#include "s_util/str.h"
#include "k_startup/page_helpers.h"
#include "k_startup/process.h"

static int32_t cmp_shm_range(const void *k0, const void *k1) {
    const void *s0 = ((const plugin_shm_range_t *)k0)->start;
    const void *s1 = ((const plugin_shm_range_t *)k1)->start;

    if (s0 < s1) {
        return -1;
    }

    if (s0 > s1) {
        return 1;
    }

    return 0;
}

/**
 * Find the start of an unmapped region of shared memory with size at least `len`.
 */
static void *find_shm_start(binary_search_tree_t *bst, uint32_t len) {
    const plugin_shm_range_t *prev = NULL;
    const plugin_shm_range_t *curr = bst_min(bst);

    while (1) {
        const void *start = prev ? prev->end : (void *)FOS_SHARED_AREA_START;
        const void *end = curr ? curr->start : (void *)FOS_SHARED_AREA_END;
        
        uint32_t area_size = (uint32_t)end - (uint32_t)start;

        if (area_size >= len) {
            return (void *)start;
        }

        /**
         * This'll be here and not in the top level condition as we want at least 1 iteration
         * to always run. Could've just used a do while, but whatevs.
         */
        if (!curr) {
            return NULL;
        }

        prev = curr;
        curr = bst_next(bst, curr);
    };
}

/**
 * If `addr` falls within a range in the binary search tree, a pointer to that range is returned!
 *
 * Returns NULL if no range is found!
 */
static plugin_shm_range_t *find_referenced_shm_range(binary_search_tree_t *bst, void *addr) {
    for (plugin_shm_range_t *iter = bst_min(bst); iter; iter = bst_next(bst, iter)) {
        if (iter->start <= addr && addr < iter->end) {
            return iter;
        }
    }

    return NULL;
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
    id_table_t *st = new_id_table(ks->al, KS_SHM_MAX_SEMS);
    binary_search_tree_t *rt = new_simple_bst(ks->al, cmp_shm_range, sizeof(plugin_shm_range_t));

    if (!plg_shm || !st || !rt) {
        delete_binary_search_tree(rt);
        delete_id_table(st);
        al_free(ks->al, plg_shm);
        return NULL;
    }

    // Success!

    init_base_plugin((plugin_t *)plg_shm, &PLUGIN_SHM_IMPL, ks);
    *(id_table_t **)&(plg_shm->sem_table) = st;
    *(binary_search_tree_t **)&(plg_shm->range_tree) = rt;

    return (plugin_t *)plg_shm;
}

/**
 * Static helper! Given a node from the plugin's range tree, unmap it for process `pid`!
 * If the range is no longer referenced by any processes, it is entirely cleaned up!
 * If `curr_proc` is not in the range's reference vector, do nothing!
 */
static void plg_shm_unmap_shm(plugin_shm_t *plg_shm, plugin_shm_range_t *range_node, process_t *curr_proc) {
    proc_id_t pid = curr_proc->pid;
    if (range_node->refs[pid / 8] & (1 << (pid % 8))) {
        // Always unmap in calling proc and remove caller from bitvector.
        pd_free_pages(curr_proc->pd, false, range_node->start, range_node->end);
        range_node->refs[pid / 8] &= ~(1 << (pid % 8));

        // Now, do we entirely delete the shared memory though?
        if (mem_chk(range_node->refs, 0, sizeof(range_node->refs))) {
            pd_free_pages(plg_shm->super.ks->root_proc->pd, true, range_node->start, range_node->end);
            bst_remove_node(plg_shm->range_tree, range_node); 
        }
    }
}

static fernos_error_t plg_shm_cmd(plugin_t *plg, plugin_cmd_id_t cmd, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    fernos_error_t err;

    kernel_state_t * const ks = plg->ks;
    plugin_shm_t * const plg_shm = (plugin_shm_t *)plg;

    thread_t * const curr_thr = (thread_t *)(ks->schedule.head);
    process_t * const curr_proc = curr_thr->proc;

    (void)arg2;
    (void)arg3;

    switch (cmd) {

    /**
     * Create a new sempahore!
     *
     * `arg0` - User pointer to a `sem_id_t`.
     * `arg1` - Semaphore number of passes.
     *
     * Returns FOS_E_BAD_ARGS if `arg0` is NULL or if copying to `arg0` fails.
     * Returns FOS_E_BAD_ARGS if `arg1` is 0.
     * Returns FOS_E_NO_SPACE if the sempahore table is already full.
     * Returns FOS_E_NO_MEM if we fail to allocate the sempahore state.
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
        mem_set(sem->refs, 0, sizeof(sem->refs));
        sem->refs[curr_proc->pid / 8] |= 1 << (curr_proc->pid % 8);

        idtb_set(plg_shm->sem_table, sem_id, sem);

        DUAL_RET(curr_thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
    }

    /**
     * This attempts to decrement the sempahore's internal pass counter!
     *
     * If the pass counter is 0, this call blocks the current thread until pass counter becomes
     * non-zero.
     *
     * `arg0` - The id of the semaphore we are trying to acquire.
     *
     * Returns FOS_E_BAD_ARGS if the given semaphore id doesn't reference a semaphore available to 
     * the calling process.
     * Returns FOS_E_STATE_MISMATCH if the calling process closes the semaphore while this thread is 
     * waiting.
     * Returns FOS_E_SUCCESS when the semaphore has been successfully decremented.
     * Returns FOS_E_NO_MEM if we fail to add our thread to the wait queue in the waiting case.
     */
    case PLG_SHM_PCID_SEM_DEC: {
        const sem_id_t sem_id = (sem_id_t)arg0;

        if (sem_id >= KS_SHM_MAX_SEMS) {
            DUAL_RET(curr_thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
        }

        plugin_shm_sem_t * const sem = idtb_get(plg_shm->sem_table, sem_id);
        if (!sem) {
            DUAL_RET(curr_thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
        }

        // Does this proccess even have access `sem` though?
        if (!(sem->refs[curr_proc->pid / 8] & (1 << (curr_proc->pid % 8)))) {
            DUAL_RET(curr_thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
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

        if (sem_id >= KS_SHM_MAX_SEMS) {
            return FOS_E_SUCCESS;
        }

        plugin_shm_sem_t * const sem = idtb_get(plg_shm->sem_table, sem_id);
        if (!sem) {
            DUAL_RET(curr_thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
        }

        // Does this proccess even have access `sem` though?
        if (!(sem->refs[curr_proc->pid / 8] & (1 << (curr_proc->pid % 8)))) {
            DUAL_RET(curr_thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
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

        if (sem_id >= KS_SHM_MAX_SEMS) {
            return FOS_E_SUCCESS;
        }

        plugin_shm_sem_t * const sem = idtb_get(plg_shm->sem_table, sem_id);
        if (!sem) {
            return FOS_E_SUCCESS; // semaphore doesn't even exist.
        }

        // Does this proccess even have access `sem` though?
        if (!(sem->refs[curr_proc->pid / 8] & (1 << (curr_proc->pid % 8)))) {
            return FOS_E_SUCCESS;
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

        
        // Now we remove this process from the semaphores reference vector!
        sem->refs[curr_proc->pid / 8] &= ~(1 << (curr_proc->pid % 8));

        // If the semaphore reference vector is all 0's now, dispose of the semaphore.        
        if (mem_chk(sem->refs, 0, sizeof(sem->refs))) {
            delete_wait_queue((wait_queue_t *)(sem->bwq));
            al_free(ks->al, sem);
            idtb_push_id(plg_shm->sem_table, sem_id);
        }

        return FOS_E_SUCCESS;
    }

    /**
     * Create a new shared memory area of size at least `bytes`.
     *
     * `arg0` - The minimum size of the area to create.
     * `arg1` - A userspace void **.
     *
     * Returns FOS_E_BAD_ARGS if `bytes` is 0 or `shm` is NULL.
     * Returns FOS_E_NO_SPACE if the shared memory area doesn't have a large enough unmapped area.
     * Returns FOS_E_NO_MEM if there aren't enough free pages to complete the allocation.
     * Returns FOS_E_SUCCESS on success and writes a pointer to the beginning of the area to `*shm`.
     */
    case PLG_SHM_PCID_NEW_SHM: {
        uint32_t len = arg0;
        if (len == 0) {
            DUAL_RET(curr_thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
        }

        if (len > FOS_SHARED_AREA_SIZE) {
            DUAL_RET(curr_thr, FOS_E_NO_MEM, FOS_E_SUCCESS);
        }

        // Here we know `len` <= SHARED_AREA_SIZE, it is impossible aligning up causes `len`
        // to loop back to 0.
        len = ALIGN_UP(len, M_4K);

        void ** const u_ret_ptr = (void **)arg1;
        if (!u_ret_ptr) {
            DUAL_RET(curr_thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
        }


        void *new_start = find_shm_start(plg_shm->range_tree, len);
        if (!new_start) {
            DUAL_RET(curr_thr, FOS_E_NO_SPACE, FOS_E_SUCCESS);
        }

        plugin_shm_range_t range = {
            .start = new_start,
            .end = (uint8_t *)new_start + len,
            .refs = {0}
        };

        range.refs[curr_proc->pid / 8] |= 1 << (curr_proc->pid % 8);

        err = bst_add(plg_shm->range_tree, &range);
        if (err != FOS_E_SUCCESS) {
            if (err == FOS_E_ALREADY_ALLOCATED) {
                return FOS_E_STATE_MISMATCH;
            }

            err = FOS_E_UNKNWON_ERROR;
        }

        if (err == FOS_E_SUCCESS) {
            // our range is in our tree! Can we map it though?
            
            const void *true_e;
            err = pd_alloc_pages(ks->root_proc->pd, true, true, range.start, range.end, &true_e);
            if (err != FOS_E_SUCCESS) {
                if (err != FOS_E_NO_MEM) {
                    return FOS_E_STATE_MISMATCH; // We should only ever encounter a no memory 
                                                 // failure!
                }

                // we ran out of memory, return pages which were allocated.
                pd_free_pages(ks->root_proc->pd, true, range.start, true_e); 
            }
        }

        if (err == FOS_E_SUCCESS) {
            // Our range is allocated and mapped in kernel space.
            // Can we map it to the user memory area though?
            
            err = pd_copy_range(curr_proc->pd, ks->root_proc->pd, range.start, range.end);
            if (err != FOS_E_SUCCESS && err != FOS_E_NO_MEM) {
                return FOS_E_STATE_MISMATCH; // We only ever allow no mem failures.
            }
        }

        if (err == FOS_E_SUCCESS) {
            // Finally, write back start bab!

            err = mem_cpy_to_user(curr_proc->pd, u_ret_ptr, &(range.start), sizeof(void *), NULL);
            if (err != FOS_E_SUCCESS) {
                err = FOS_E_UNKNWON_ERROR;
            }
        }

        if (err != FOS_E_SUCCESS) {
            // Cleanup time in error situation!

            // These frees are safe! We know that are allocation errors did not fail due to running
            // into another mapped area. There is no chance we free something that shouldn't be freed!
            pd_free_pages(curr_proc->pd, false, range.start, range.end); 
            pd_free_pages(ks->root_proc->pd, true, range.start, range.end);

            bst_remove(plg_shm->range_tree, &range);

            DUAL_RET(curr_thr, err, FOS_E_SUCCESS);
        }

        DUAL_RET(curr_thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
    }

    /**
     * Close a shared memory area!
     *
     * `arg0` - the address of a byte within the shared memory region to unmap.
     *
     * Remember, this is a destructor style call, and thus needs not return anything to the 
     * calling thread.
     */
    case PLG_SHM_PCID_CLOSE_SHM: {
        void *addr = (void *)arg0;

        plugin_shm_range_t *range = find_referenced_shm_range(plg_shm->range_tree, addr);
        if (!range) {
            return FOS_E_SUCCESS;
        }

        plg_shm_unmap_shm(plg_shm, range, curr_proc);

        return FOS_E_SUCCESS;
    }

    default: {
        DUAL_RET(curr_thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
    }

    }
}

static fernos_error_t plg_shm_on_fork_proc(plugin_t *plg, proc_id_t cpid) {
    kernel_state_t * const ks = plg->ks;
    plugin_shm_t * const plg_shm = (plugin_shm_t *)plg;

    process_t * const child_proc = idtb_get(ks->proc_table, cpid);
    if (!child_proc) {
        return FOS_E_STATE_MISMATCH;
    }

    process_t * const parent_proc = child_proc->parent;
    if (!parent_proc) {
        return FOS_E_STATE_MISMATCH;
    }

    // We must iterate over every semaphore in the system, if a semaphore is referenced by
    // the parent process, it will now also become referenced by this new child process.
    
    idtb_reset_iterator(plg_shm->sem_table);
    const sem_id_t NULL_SEM_ID = idtb_null_id(plg_shm->sem_table);
    for (sem_id_t sem_id = idtb_get_iter(plg_shm->sem_table); sem_id != NULL_SEM_ID;
            sem_id = idtb_next(plg_shm->sem_table)) {
        plugin_shm_sem_t * const sem = idtb_get(plg_shm->sem_table, sem_id);
        if (!sem) {
            return FOS_E_STATE_MISMATCH;
        }

        if (sem->refs[parent_proc->pid / 8] & (1 << (parent_proc->pid % 8))) {
            sem->refs[child_proc->pid / 8] |= (1 << (child_proc->pid % 8));
        }
    }

    // Ok, this may be kinda slow, but for actual shared memory areas, we need to iterate
    // over all shared memory ranges. A memory range which is referenced by the parent,
    // must now also become referenced by the child.
    // 
    // NOTE: We don't need to do any page directory copying as this will be implicitly done
    // by the fork.

    for (plugin_shm_range_t *iter = bst_min(plg_shm->range_tree); iter; 
            iter = bst_next(plg_shm->range_tree, iter)) {
        
        if (iter->refs[parent_proc->pid / 8] & (1 << (parent_proc->pid % 8))) {
            iter->refs[child_proc->pid / 8] |= (1 << (child_proc->pid % 8));
        }
    }

    return FOS_E_SUCCESS;
}

static fernos_error_t plg_shm_on_reset_or_reap_proc(plugin_t *plg, proc_id_t pid) {
    kernel_state_t * const ks = plg->ks;
    plugin_shm_t * const plg_shm = (plugin_shm_t *)plg;

    process_t *rproc = idtb_get(ks->proc_table, pid);
    if (!rproc) {
        return FOS_E_STATE_MISMATCH;
    }

    // Since we will potentially modifying the semaphore table, we will not use
    // the idtb iterator functions!

    for (sem_id_t sem_id = 0; sem_id < KS_SHM_MAX_SEMS; sem_id++) {
        plugin_shm_sem_t * const sem = idtb_get(plg_shm->sem_table, sem_id);
        if (!sem) {
            continue;
        }

        // We only do anything for semaphores which are referenced by the process about to be 
        // reaped.
        if (sem->refs[pid / 8] & (1 << (pid % 8))) {
            sem->refs[pid / 8] &= ~(1 << (pid % 8));
            if (mem_chk(sem->refs, 0, sizeof(sem->refs))) {
                delete_wait_queue((wait_queue_t *)(sem->bwq));
                al_free(ks->al, sem);
                idtb_push_id(plg_shm->sem_table, sem_id);
            }
        }
    }


    plugin_shm_range_t *iter = bst_min(plg_shm->range_tree);
    while (iter) {
        // Gotta do this here, as we may delete `iter` later.
        // (This is safe because bst nodes are always stable)
        plugin_shm_range_t *next = bst_next(plg_shm->range_tree, iter);

        // This does nothing if `rproc` does not reference `iter`'s range!
        plg_shm_unmap_shm(plg_shm, iter, rproc);

        iter = next;
    }

    return FOS_E_SUCCESS;
}

static fernos_error_t plg_shm_on_reset_proc(plugin_t *plg, proc_id_t pid) {
    return plg_shm_on_reset_or_reap_proc(plg, pid);
}

static fernos_error_t plg_shm_on_reap_proc(plugin_t *plg, proc_id_t rpid) {
    return plg_shm_on_reset_or_reap_proc(plg, rpid);
}
