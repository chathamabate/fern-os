

#include "k_startup/page_helpers.h"
#include "s_util/constraints.h"
#include "k_startup/state.h"
#include "k_startup/thread.h"
#include "k_startup/process.h"
#include "k_startup/page.h"
#include "k_startup/handle.h"
#include "k_startup/plugin.h"

#include "k_startup/vga_cd.h"
#include "s_util/str.h"

/**
 * Creates a new kernel state with basically no fleshed out details.
 */
kernel_state_t *new_kernel_state(allocator_t *al) {
    kernel_state_t *ks = al_malloc(al, sizeof(kernel_state_t));
    id_table_t *pt = new_id_table(al, FOS_MAX_PROCS);
    timed_wait_queue_t *twq = new_timed_wait_queue(al);

    if (!ks || !pt || !twq) {
        al_free(al, ks);
        delete_id_table(pt);
        delete_wait_queue((wait_queue_t *)twq);
        return NULL;
    }

    *(allocator_t **)&(ks->al) = al;
    init_ring(&(ks->schedule));
    *(id_table_t **)&(ks->proc_table) = pt;
    ks->root_proc = NULL;

    ks->curr_tick = 0;
    *(timed_wait_queue_t **)&(ks->sleep_q) = twq;

    for (size_t i = 0; i < FOS_MAX_PLUGINS; i++) {
        ks->plugins[i] = NULL; // No plugins yet!
    }

    return ks;
}

fernos_error_t ks_set_plugin(kernel_state_t *ks, plugin_id_t plg_id, plugin_t *plg) {
    if (plg_id >= FOS_MAX_PLUGINS) {
        return FOS_E_INVALID_INDEX;
    }

    if (ks->plugins[plg_id]) {
        return FOS_E_IN_USE;
    }

    ks->plugins[plg_id] = plg;

    return FOS_E_SUCCESS;
}

void ks_save_ctx(kernel_state_t *ks, user_ctx_t *ctx) {
    if (ks->schedule.head) {
        ((thread_t *)(ks->schedule.head))->ctx = *ctx;
    }
}

fernos_error_t ks_expand_stack(kernel_state_t *ks, void *new_base) {
    fernos_error_t err;

    if (!(ks->schedule.head)) {
        return FOS_E_STATE_MISMATCH;
    }

    if (!IS_ALIGNED(new_base, M_4K)) {
        return FOS_E_ALIGN_ERROR;
    }

    thread_t *thr = (thread_t *)(ks->schedule.head);
    thread_id_t tid = thr->tid;

    void *stack_start = (void *)FOS_THREAD_STACK_START(tid);
    void *true_stack_start = (uint8_t *)stack_start + M_4K;
    const void *stack_end = (const void *)FOS_THREAD_STACK_END(tid);

    if (new_base < true_stack_start || stack_end <= new_base) {
        return FOS_E_INVALID_INDEX; // Trying to access an address which isn't really on the stack.
    }

    if (thr->stack_base <= new_base) {
        return FOS_E_SUCCESS;
    }

    const void *true_e;
    err = pd_alloc_pages(thr->proc->pd, true, new_base, thr->stack_base, &true_e);

    if (err != FOS_E_SUCCESS && err != FOS_E_ALREADY_ALLOCATED) {
        return err;
    }

    thr->stack_base = new_base;

    return FOS_E_SUCCESS;
}

void ks_shutdown(kernel_state_t *ks) {
    for (size_t i = 0; i < FOS_MAX_PLUGINS; i++) {
        if (ks->plugins[i]) {
            plg_on_shutdown(ks->plugins[i]);
        }
    }

    lock_up();
}

fernos_error_t ks_tick(kernel_state_t *ks) {
    fernos_error_t err;

    bool not_halted = ks->schedule.head != NULL;

    ks->curr_tick++;

    twq_notify(ks->sleep_q, ks->curr_tick);

    thread_t *woken_thread;
    while ((err = twq_pop(ks->sleep_q, (void **)&woken_thread)) == FOS_E_SUCCESS) {
        // Remove reference to sleep queue.
        woken_thread->wq = NULL;
        woken_thread->state = THREAD_STATE_DETATCHED;

        // Schedule woken thread. 
        
        thread_schedule(woken_thread, &(ks->schedule));
    }

    if (err != FOS_E_EMPTY) {
        return err;
    }

    /*
     * We only advance the schedule if we weren't halted when we entered this handler.
     *
     * Otherwise, the order threads are woken up doesn't always match when they execute.
     * (This doesn't really matter that much, but makes execution behavior more predictable)
     */
    if (not_halted) {
        ring_advance(&(ks->schedule));
    }

    // trigger plugin on tick handlers every 16th tick.
    if ((ks->curr_tick & 0xF) == 0) {
        err = plgs_tick(ks->plugins, FOS_MAX_PLUGINS);
        if (err != FOS_E_SUCCESS) {
            return err;
        }
    }

    return FOS_E_SUCCESS;
}

/**
 * Helper function.
 *
 * This gives all living and zombie children to the root process.
 * If zombie children are adopted by the root process, a signal will be sent.
 *
 * NOTE: What if `proc` is the root process?
 * In this case, if `proc` has any living/zombie children, FOS_E_NOT_PERMITTED is returned
 * Otherwise, this function doesn't need to do anything and FOS_E_SUCCESS is returned.
 */
static fernos_error_t ks_abandon_children(kernel_state_t *ks, process_t *proc);

static fernos_error_t ks_exit_proc_p(kernel_state_t *ks, process_t *proc, 
        proc_exit_status_t status);

static fernos_error_t ks_signal_p(kernel_state_t *ks, process_t *proc, sig_id_t sid);

KS_SYSCALL fernos_error_t ks_fork_proc(kernel_state_t *ks, proc_id_t *u_cpid) {
    if (!(ks->schedule.head)) {
        return FOS_E_STATE_MISMATCH;
    }

    thread_t *thr = (thread_t *)(ks->schedule.head);
    process_t *proc = thr->proc;

    // Reserve an ID for the child process.

    const proc_id_t NULL_PID = idtb_null_id(ks->proc_table);

    proc_id_t cpid = NULL_PID;
    process_t *child = NULL;
    fernos_error_t err = FOS_E_UNKNWON_ERROR;

    // Request child process ID.
    cpid = idtb_pop_id(ks->proc_table);

    // Create the child process.
    if (cpid != NULL_PID) {
        err = new_process_fork(proc, thr, cpid, &child);
        if (err == FOS_E_ABORT_SYSTEM) {
            return FOS_E_ABORT_SYSTEM;
        }
    }

    // Push child on to children list.
    if (err == FOS_E_SUCCESS) {
        err = l_push_back(proc->children, &child);
    }

    // Now check for errors.
    if (cpid == NULL_PID || err != FOS_E_SUCCESS) {
        idtb_push_id(ks->proc_table, cpid);

        if (delete_process(child) != FOS_E_SUCCESS) {
            return FOS_E_ABORT_SYSTEM;
        }

        if (u_cpid) {
            cpid = FOS_MAX_PROCS;
            mem_cpy_to_user(proc->pd, u_cpid, &cpid, 
                    sizeof(proc_id_t), NULL);
        }

        // Since adding the child to the child list was our last step, it is impossible
        // that we are in this function if pushing the child to the list succeeded.
        //
        // Thus, we don't need to pop anything from our child list in this error case.

        DUAL_RET(thr, FOS_E_NO_MEM, FOS_E_SUCCESS);
    }

    idtb_set(ks->proc_table, cpid, child);

    // Remember, now we are working with 2 different user processes!

    child->main_thread->ctx.eax = FOS_E_SUCCESS;
    thread_schedule(child->main_thread, &(ks->schedule));

    // Call the on fork handlers!
    err = plgs_on_fork_proc(ks->plugins, FOS_MAX_PLUGINS, cpid);
    if (err != FOS_E_SUCCESS) {
        return err;
    }

    // Finally I think we are done!

    if (u_cpid) {
        proc_id_t temp_pid;

        temp_pid = cpid;
        mem_cpy_to_user(proc->pd, u_cpid, &temp_pid, 
                sizeof(proc_id_t), NULL);

        temp_pid = FOS_MAX_PROCS;
        mem_cpy_to_user(child->pd, u_cpid, &temp_pid, 
                sizeof(proc_id_t), NULL);
    }

    DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
}

static fernos_error_t ks_abandon_children(kernel_state_t *ks, process_t *proc) {
    if (!proc) {
        return FOS_E_BAD_ARGS;
    }

    if (proc == ks->root_proc) {
        if (l_get_len(proc->children) > 0 || l_get_len(proc->zombie_children) > 0) {
            return FOS_E_NOT_PERMITTED;
        }

        // If we have no children to abandon, it is OK for this call to succeed on the root
        // process.
        return FOS_E_SUCCESS;
    }

    // Ok, now begin the adoption process for a non-root process.

    fernos_error_t err;

    // If we have zombie children, we will need to signal the root process at the end!
    bool signal_root = l_get_len(proc->zombie_children) > 0;

    // Give all living children to the root process.
    l_reset_iter(proc->children);
    for (process_t **iter = l_get_iter(proc->children); 
            iter; iter = l_next_iter(proc->children)) {
        (*iter)->parent = ks->root_proc;
    }

    err = l_push_back_all(ks->root_proc->children, proc->children);
    if (err != FOS_E_SUCCESS) {
        return FOS_E_UNKNWON_ERROR;
    }

    // Give all zombie children to the root process!
    l_reset_iter(proc->zombie_children);
    for (process_t **iter = l_get_iter(proc->zombie_children); 
            iter; iter = l_next_iter(proc->zombie_children)) {
        (*iter)->parent = ks->root_proc;
    }

    err = l_push_back_all(ks->root_proc->zombie_children, proc->zombie_children);
    if (err != FOS_E_SUCCESS) {
        return FOS_E_UNKNWON_ERROR;
    }

    if (signal_root) {
        err = ks_signal_p(ks, ks->root_proc, FSIG_CHLD);
        if (err != FOS_E_SUCCESS) {
            return FOS_E_UNKNWON_ERROR;
        }
    }

    return FOS_E_SUCCESS;
}

static fernos_error_t ks_exit_proc_p(kernel_state_t *ks, process_t *proc, 
        proc_exit_status_t status) {
    fernos_error_t err;

    if (ks->root_proc == proc) {
        term_put_fmt_s("[System Exit with Status 0x%X]", status);
        ks_shutdown(ks);
    }

    // First what do we do? Detatch all threads plz!

    const thread_id_t NULL_TID = idtb_null_id(proc->thread_table);

    idtb_reset_iterator(proc->thread_table);
    for (id_t iter = idtb_get_iter(proc->thread_table); iter != NULL_TID;
            iter = idtb_next(proc->thread_table)) {
        thread_t *worker =  idtb_get(proc->thread_table, iter);
        thread_detach(worker);
    }

    // This gives all zombie and living children of `proc` to the root process!
    err = ks_abandon_children(ks, proc);
    if (err != FOS_E_SUCCESS) {
        return err; // we know `proc` is not the root process here, so a non-success code 
                    // is catastrophic!
    }

    // Now remove our process from the parent's children list, and move it to the parent's
    // zombie list!

    if (!l_pop_ele(proc->parent->children, &proc, l_ptr_cmp, false)) {
        return FOS_E_STATE_MISMATCH; // Something very wrong if we can't find this process in
                                   // the parent's children list.
    }

    // Now we are no longer in a living children list, mark exited.
    proc->exited = true;
    proc->exit_status = status;

    // Add to parents zombie children list!
    err = l_push_back(proc->parent->zombie_children, &proc);
    if (err != FOS_E_SUCCESS) {
        return err;
    }

    // NOTE: By the way this has been designed, the root process may get MULTIPLE FSIG_CHLD
    // signals on a signle exit.
    //
    // Example 1:
    // root -> proc0 -> proc1 (zombie)
    //
    // Here if proc0 exits, root will get an FSIG_CHLD for `proc0` AND for when `proc1` is adopted.
    //
    // Example 2:
    //
    // root -> proc0 -> proc1 -> proc2 (zombie)
    //
    // Assume proc1 exits and proc0 doesn't allow signals. 
    // First `proc2` is adopted by `root` (sending a FSIG_CHLD)
    // Then `proc1` is added to `proc0`'s zombie children, `proc0` then forcefully exits,
    // sending even more FSIG_CHLD's to the root.
    //
    // Moral of the story, gauranteeing the root only receives a single FSIG_CHLD during a process
    // exit is kinda hard to do well. 
    // For this reason, users of FernOS should understand that receiving an FSIG_CHLD does not
    // gaurantee there is anyone to reap! 
    err = ks_signal_p(ks, proc->parent, FSIG_CHLD);
    if (err != FOS_E_SUCCESS) {
        return err;
    }

    // This call does not return to any user thread!
    return FOS_E_SUCCESS;
}

KS_SYSCALL fernos_error_t ks_exit_proc(kernel_state_t *ks, proc_exit_status_t status) {
    if (!(ks->schedule.head)) {
        return FOS_E_STATE_MISMATCH;
    }

    thread_t *thr = (thread_t *)(ks->schedule.head);

    return ks_exit_proc_p(ks, thr->proc, status);
}

KS_SYSCALL fernos_error_t ks_reap_proc(kernel_state_t *ks, proc_id_t cpid, 
        proc_id_t *u_rcpid, proc_exit_status_t *u_rces) {
    fernos_error_t err;

    if (!(ks->schedule.head)) {
        return FOS_E_STATE_MISMATCH;
    }

    thread_t *thr = (thread_t *)(ks->schedule.head);
    process_t *proc = thr->proc;

    process_t *rproc = NULL;
    fernos_error_t user_err = FOS_E_UNKNWON_ERROR;

    // First we retrieve what zombie child we want to reap (and also remove it from the zombie list)
    // If we cannot find an applicable zombie child process, user_err will be set to the correct
    // error code.

    if (cpid == FOS_MAX_PROCS) {
        // We are reaping any process!

        // Default to error case at first.
        user_err = FOS_E_EMPTY;

        if (l_get_len(proc->zombie_children) > 0) {
            err = l_pop_front(proc->zombie_children, &rproc);

            // Failing to pop from the zombie list is seen as a fatal kernel error.
            // Should never really happen though.
            if (err != FOS_E_SUCCESS) {
                return err;
            }

            user_err = FOS_E_SUCCESS;
        } 
    } else {

        // Default to error case at first.
        user_err = FOS_E_STATE_MISMATCH;

        // We are reaping a specific process!
        rproc = idtb_get(ks->proc_table, cpid);

        if (rproc && rproc->parent == proc) {
            user_err = FOS_E_EMPTY; // We found an expected child process!

            // but is it reapable?
            if (rproc->exited == true) {
                bool removed = l_pop_ele(proc->zombie_children, &rproc, l_ptr_cmp, false);

                if (!removed) {
                    // Something is very wrong if the described process is not in the zombie list.
                    return FOS_E_STATE_MISMATCH;
                }

                user_err = FOS_E_SUCCESS;
            }
        }
    }

    proc_id_t rcpid = FOS_MAX_PROCS;
    proc_exit_status_t rces = PROC_ES_UNSET;

    // Ok, at this point, user_err is either FOS_E_SUCCESS, meaning we have a freestanding zombie
    // process stored in rproc which we can reap!
    //
    // Or, we couldn't find an apt process to reap.

    if (user_err == FOS_E_SUCCESS) {
        // REAP!
        
        // First off, call the on reap handler!
        err = plgs_on_reap_proc(ks->plugins, FOS_MAX_PLUGINS, rproc->pid);
        if (err != FOS_E_SUCCESS) {
            return err;
        }

        rcpid = rproc->pid;
        rces = rproc->exit_status;

        err = delete_process(rproc);
        if (err != FOS_E_SUCCESS) {
            return err; // failure to delete the process is always fatal!
        }

        idtb_push_id(ks->proc_table, rcpid);
    }

    if (u_rcpid) {
        mem_cpy_to_user(proc->pd, u_rcpid, &rcpid, 
                sizeof(proc_id_t), NULL);
    }

    if (u_rces) {
        mem_cpy_to_user(proc->pd, u_rces, &rces, 
                sizeof(proc_id_t), NULL);
    }
    
    DUAL_RET(thr, user_err, FOS_E_SUCCESS);
}

KS_SYSCALL fernos_error_t ks_exec(kernel_state_t *ks, user_app_t *u_ua, const void *u_abs_ab,
        size_t u_abs_ab_len) {
    fernos_error_t err;

    if (!(ks->schedule.head)) {
        return FOS_E_STATE_MISMATCH;
    }

    thread_t *thr = (thread_t *)(ks->schedule.head);
    process_t *proc = thr->proc;

    // Things that need to be done:
    // 1) Copy entire user app and args block from userspace into kernel space.
    // 2) Create a new page directroy for the user app and args block.
    // 3) Somehow reset the calling process to this new state?
    // 4) I think before reset we move children and zombie to the root?
    //      Based on how I have organized handles.. and everything else, we must do this all
    //      within the calling process! 
}

static fernos_error_t ks_signal_p(kernel_state_t *ks, process_t *proc, sig_id_t sid) {
    fernos_error_t err;

    if (!proc || sid >= 32) {
        return FOS_E_BAD_ARGS;
    }

    // If it is already set, we do nothing!
    if (proc->sig_vec & (1 << sid)) {
        return FOS_E_SUCCESS;
    }

    // Set the signal bit no matter what!
    proc->sig_vec |= (1 << sid);

    // Ok, but is this signal even allowed in the first place??
    // If not, force exit the process.
    if (!(proc->sig_allow & (1 << sid))) {
        /*
         * NOTE: Very importnat, this logic actually causes indirect recursion.
         * A chain too deep of child processes could result in a stack overflow in the kernel!
         */
        return ks_exit_proc_p(ks, proc, PROC_ES_SIGNAL);
    }

    // Otherwise, we notify our wait queue!
    
    err = vwq_notify(proc->signal_queue, sid, VWQ_NOTIFY_FIRST);
    if (err != FOS_E_SUCCESS) {
        return err;
    }

    thread_t *woken_thread;
    err = vwq_pop(proc->signal_queue, (void **)&woken_thread, NULL);

    if (err == FOS_E_SUCCESS) {
        // We woke a thread up!
        
        // Clear our bit real quick.
        proc->sig_vec &= ~(1 << sid);

        // When waiting for a signal, just the first wait context field is used.
        sig_id_t *u_sid = (sig_id_t *)(woken_thread->wait_ctx[0]);

        // Detach thread. 
        // NOTE: VERY IMPORTANT Here we DO NOT use thread_detach, because at this point,
        // `woken_thread` will not actually be referenced by `woken_thread->wq`. 
        //
        // We detach manually instead!
        woken_thread->wait_ctx[0] = 0;
        woken_thread->wq = NULL;
        woken_thread->state = THREAD_STATE_DETATCHED;

        // Schedule thread and return correct values!
        thread_schedule(woken_thread, &(ks->schedule));

        if (u_sid) {
            mem_cpy_to_user(woken_thread->proc->pd, u_sid, &sid, 
                    sizeof(sig_id_t), NULL);
        }

        woken_thread->ctx.eax = FOS_E_SUCCESS;
    } else if  (err != FOS_E_EMPTY) {
        // If the error was FOS_E_EMPTY, this is no big deal!
        return err;
    }

    return FOS_E_SUCCESS;
}

KS_SYSCALL fernos_error_t ks_signal(kernel_state_t *ks, proc_id_t pid, sig_id_t sid) {
    fernos_error_t err;

    if (!(ks->schedule.head)) {
        return FOS_E_STATE_MISMATCH;
    }

    thread_t *thr = (thread_t *)(ks->schedule.head);

    process_t *recv_proc;

    if (pid == FOS_MAX_PROCS) {
        recv_proc = thr->proc->parent;
    } else {
        recv_proc = idtb_get(ks->proc_table, pid);
    }

    if (!recv_proc) {
        // We couldn't find who to send to...
        DUAL_RET(thr, FOS_E_STATE_MISMATCH, FOS_E_SUCCESS);
    }

    err = ks_signal_p(ks, recv_proc, sid);
    if (err != FOS_E_SUCCESS) {
        return err;
    }

    /*
     * It's very possible that the act of sending the signal exited this process!
     *
     * So we only return to the thread if the owning process is still alive!
     */
    if (!(thr->proc->exited)) {
        thr->ctx.eax = FOS_E_SUCCESS;
    }

    return FOS_E_SUCCESS;
}

KS_SYSCALL fernos_error_t ks_allow_signal(kernel_state_t *ks, sig_vector_t sv) {
    if (!(ks->schedule.head)) {
        return FOS_E_STATE_MISMATCH;
    }

    thread_t *thr = (thread_t *)(ks->schedule.head);
    process_t *proc = thr->proc;

    sig_vector_t old = proc->sig_allow;

    proc->sig_allow = sv;

    // Are there pending signals which are not allowed??
    if ((~(proc->sig_allow)) & proc->sig_vec) {
        return ks_exit_proc_p(ks, proc, PROC_ES_SIGNAL);
    }

    DUAL_RET(thr, old, FOS_E_SUCCESS);
}

KS_SYSCALL fernos_error_t ks_wait_signal(kernel_state_t *ks, sig_vector_t sv, sig_id_t *u_sid) {
    fernos_error_t err;
    
    if (!(ks->schedule.head)) {
        return FOS_E_STATE_MISMATCH;
    }

    thread_t *thr = (thread_t *)(ks->schedule.head);
    process_t *proc = thr->proc;

    const sig_id_t NULL_SID = 32;

    if (sv == 0) {
        if (u_sid) {
            mem_cpy_to_user(proc->pd, u_sid, &NULL_SID, 
                    sizeof(sig_id_t), NULL);
        }

        DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
    }

    // Before doing any waiting, let's see if we can service a pending signal immediately!

    if (sv & proc->sig_vec) { // Quick check before looping 
        sig_id_t msid;
        for (msid = 0; msid < 32; msid++) {
            sig_vector_t tv = 1 << msid;
            if ((sv & tv) && (proc->sig_vec & tv)) {
                break;
            }
        }

        // did we find a match?
        if (msid < 32) {
            // Clear the pending bit.
            proc->sig_vec &= ~(1 << msid);

            if (u_sid) {
                mem_cpy_to_user(proc->pd, u_sid, &msid, 
                        sizeof(sig_id_t), NULL);
            }

            DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
        }
    }

    // We didn't find a match, looks like we'll need to wait!

    err = vwq_enqueue(proc->signal_queue, thr, sv);
    if (err != FOS_E_SUCCESS) {
        return err;
    }

    // This is safe because we know `thr->state` is THREAD_SCHEDULED!
    thread_detach(thr); 
    thr->wq = (wait_queue_t *)(proc->signal_queue);
    thr->state = THREAD_STATE_WAITING;
    thr->wait_ctx[0] = (uint32_t)u_sid;

    return FOS_E_SUCCESS;
}

KS_SYSCALL fernos_error_t ks_signal_clear(kernel_state_t *ks, sig_vector_t sv) {
    if (!(ks->schedule.head)) {
        return FOS_E_STATE_MISMATCH;
    }

    thread_t *thr = (thread_t *)(ks->schedule.head);

    thr->proc->sig_vec &= ~sv;

    return FOS_E_SUCCESS;
}

KS_SYSCALL fernos_error_t ks_request_mem(kernel_state_t *ks, void *s, const void *e, const void **u_true_e) {
    fernos_error_t err;

    if (!(ks->schedule.head)) {
        return FOS_E_STATE_MISMATCH;
    }

    thread_t *thr = (thread_t *)(ks->schedule.head);
    phys_addr_t pd = thr->proc->pd;

    if (!u_true_e) {
        DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
    }

    if ((uintptr_t)s < FOS_FREE_AREA_START || (uintptr_t)s >= FOS_FREE_AREA_END) {
        DUAL_RET(thr, FOS_E_INVALID_RANGE, FOS_E_SUCCESS);
    }

    if ((uintptr_t)e < FOS_FREE_AREA_START || (uintptr_t)e > FOS_FREE_AREA_END) {
        DUAL_RET(thr, FOS_E_INVALID_RANGE, FOS_E_SUCCESS);
    }
    
    // All other errors should be handled in `pd_alloc_pages`.

    const void *true_e = NULL;
    err = pd_alloc_pages(pd, true, s, e, &true_e);

    // Sadly we are not going to error check here, but this should really always succeed
    // unless the user gives a bad address, in which case it doesn't matter anyway.
    mem_cpy_to_user(pd, u_true_e, &true_e, sizeof(*u_true_e), NULL);

    DUAL_RET(thr, err, FOS_E_SUCCESS);
}

KS_SYSCALL fernos_error_t ks_return_mem(kernel_state_t *ks, void *s, const void *e) {
    if (!(ks->schedule.head)) {
        return FOS_E_STATE_MISMATCH;
    }

    thread_t *thr = (thread_t *)(ks->schedule.head);
    phys_addr_t pd = thr->proc->pd;

    if ((uintptr_t)s < FOS_FREE_AREA_START || (uintptr_t)s >= FOS_FREE_AREA_END) {
        return FOS_E_SUCCESS;
    }

    if ((uintptr_t)e < FOS_FREE_AREA_START || (uintptr_t)e > FOS_FREE_AREA_END) {
        return FOS_E_SUCCESS;
    }

    // All other error cases are checked inside `pd_free_pages`.
    pd_free_pages(pd, s, e);

    return FOS_E_SUCCESS;
}

KS_SYSCALL fernos_error_t ks_sleep_thread(kernel_state_t *ks, uint32_t ticks) {
    fernos_error_t err;

    if (!(ks->schedule.head)) {
        return FOS_E_STATE_MISMATCH;
    }

    thread_t *thr = (thread_t *)(ks->schedule.head);

    err = twq_enqueue(ks->sleep_q, (void *)thr, 
            ks->curr_tick + ticks);
    if (err != FOS_E_SUCCESS) {
        return err;
    }

    // Only deschedule one we know our thread was added successfully to the wait queue!
    thread_detach(thr);

    thr->wq = (wait_queue_t *)(ks->sleep_q);
    thr->state = THREAD_STATE_WAITING;

    return FOS_E_SUCCESS;
}

KS_SYSCALL fernos_error_t ks_spawn_local_thread(kernel_state_t *ks, thread_id_t *u_tid, 
        thread_entry_t entry, void *arg) {
    if (!(ks->schedule.head)) {
        return FOS_E_STATE_MISMATCH;
    }

    thread_t *thr = (thread_t *)(ks->schedule.head);
    process_t *proc = thr->proc;

    const thread_id_t NULL_TID = idtb_null_id(proc->thread_table);

    DUAL_RET_COND(!entry, thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);

    thread_t *new_thr = proc_new_thread(proc, (uintptr_t)entry, (uint32_t)arg, 0, 0);

    if (!new_thr) {
        thr->ctx.eax = FOS_E_UNKNWON_ERROR;

        if (u_tid) {
            mem_cpy_to_user(proc->pd, u_tid, &NULL_TID, sizeof(thread_id_t), NULL);
        }

        return FOS_E_SUCCESS;
    }

    // Schedule our new thread!
    thread_schedule(new_thr, &(ks->schedule));

    thread_id_t new_tid = new_thr->tid;

    if (u_tid) {
        mem_cpy_to_user(proc->pd, u_tid, &new_tid, sizeof(thread_id_t), NULL);
    }

    DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
}

KS_SYSCALL fernos_error_t ks_exit_thread(kernel_state_t *ks, void *ret_val) {
    fernos_error_t err;

    if (!(ks->schedule.head)) {
        return FOS_E_STATE_MISMATCH;
    }

    thread_t *thr = (thread_t *)(ks->schedule.head);
    process_t *proc = thr->proc;

    // In case of main thread, just defer to process exit.
    if (thr == proc->main_thread) {
        return ks_exit_proc(ks, (proc_exit_status_t)ret_val);
    }

    // First, let's deschedule our current thread.

    thread_detach(thr);

    thr->exit_ret_val = ret_val;
    thr->state = THREAD_STATE_EXITED;

    thread_id_t tid = thr->tid;

    err = vwq_notify_first(proc->join_queue, (uint8_t)tid);

    // This really should never happen.
    if (err != FOS_E_SUCCESS) {
        return err;
    }

    thread_t *joining_thread;

    err = vwq_pop(proc->join_queue, (void **)&joining_thread, NULL);

    // No one was waiting on our thread so we just leave here successfully.
    // Remember, we don't reap our thread until someone else joins on it!
    if (err == FOS_E_EMPTY) {
        return FOS_E_SUCCESS;
    }

    // Some sort of pop error?
    if (err != FOS_E_SUCCESS) {
        return err;
    }

    // Ok, we found a joining thread, let's remove all information about its waiting state,
    // write return values, then schedule it!

    joining_thread->wq = NULL;
    thread_join_ret_t *u_ret_ptr = (thread_join_ret_t *)(joining_thread->wait_ctx[0]);
    joining_thread->wait_ctx[0] = 0;
    joining_thread->state = THREAD_STATE_DETATCHED;

    if (u_ret_ptr) {
        thread_join_ret_t local_ret = {
            .retval = thr->exit_ret_val, 
            .joined = tid
        };

        mem_cpy_to_user(proc->pd, u_ret_ptr, &local_ret, sizeof(thread_join_ret_t), NULL);
    }
    joining_thread->ctx.eax = FOS_E_SUCCESS;

    thread_schedule(joining_thread, &(ks->schedule));

    // Ok finally, since our exited thread is no longer needed
    proc_delete_thread(proc, thr, true);

    return FOS_E_SUCCESS;
}

KS_SYSCALL fernos_error_t ks_join_local_thread(kernel_state_t *ks, join_vector_t jv, 
        thread_join_ret_t *u_join_ret) {
    fernos_error_t err;

    if (!(ks->schedule.head)) {
        return FOS_E_STATE_MISMATCH;
    }

    thread_t *thr = (thread_t *)(ks->schedule.head);
    process_t *proc = thr->proc;

    DUAL_RET_COND(
        jv == 0 || jv == (1U << thr->tid),
        thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS
    );

    // First we search to see if there already is an exited thread we can join on!

    thread_id_t ready_tid;
    thread_t *joinable_thread;
    
    for (ready_tid = 0; ready_tid < 32; ready_tid++) {
        joinable_thread = idtb_get(proc->thread_table, ready_tid);
        if ((jv & (1 << ready_tid)) && 
                joinable_thread && joinable_thread->state == THREAD_STATE_EXITED) {
            // We found a joinable thread, break!
            break;
        }
    }

    // Did we find such a thread?
    if (ready_tid < 32) {
        thread_join_ret_t local_ret = {
            .joined = ready_tid,
            .retval = joinable_thread->exit_ret_val
        };

        // Now reap our joinable thread!
        proc_delete_thread(proc, joinable_thread, true);

        if (u_join_ret) {
            mem_cpy_to_user(proc->pd, u_join_ret, &local_ret, sizeof(thread_join_ret_t), NULL);
        }

        DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
    }

    // Here, we searched all threads and didn't find one to join on.
    // So, The current thread must wait!

    err = vwq_enqueue(proc->join_queue, thr, jv);
    DUAL_RET_FOS_ERR(err, thr);

    // Here we successfully queued our thread!
    // Now we can safely deschedule it!

    thread_detach(thr);
    thr->wq = (wait_queue_t *)(proc->join_queue);
    thr->wait_ctx[0] = (uint32_t)u_join_ret; // Save where we will eventually return to!
    thr->state = THREAD_STATE_WAITING;

    return FOS_E_SUCCESS;
}

KS_SYSCALL fernos_error_t ks_set_in_handle(kernel_state_t *ks, handle_t in) {
    if (!(ks->schedule.head)) {
        return FOS_E_STATE_MISMATCH;
    }

    thread_t *thr = (thread_t *)(ks->schedule.head);
    process_t *proc = thr->proc;

    handle_t NULL_HANDLE = idtb_null_id(proc->handle_table);

    if (in >= idtb_max_cap(proc->handle_table)) {
        proc->in_handle = NULL_HANDLE; 
    } else {
        proc->in_handle = in;
    }

    DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
}

KS_SYSCALL fernos_error_t ks_in_read(kernel_state_t *ks, void *u_dest, size_t len, size_t *u_readden) {
    if (!(ks->schedule.head)) {
        return FOS_E_STATE_MISMATCH;
    }

    thread_t *thr = (thread_t *)(ks->schedule.head);
    process_t *proc = thr->proc;

    handle_state_t *hs = idtb_get(proc->handle_table, proc->in_handle);
    if (!hs) { // Dummy read implementation.
        if (!u_dest) {
            DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
        }

        DUAL_RET(thr, FOS_E_EMPTY, FOS_E_SUCCESS);
    }

    // If the handle is mapped, default to handle read.
    return hs_read(hs, u_dest, len, u_readden);
}

KS_SYSCALL fernos_error_t ks_in_wait(kernel_state_t *ks) {
    if (!(ks->schedule.head)) {
        return FOS_E_STATE_MISMATCH;
    }

    thread_t *thr = (thread_t *)(ks->schedule.head);
    process_t *proc = thr->proc;

    handle_state_t *hs = idtb_get(proc->handle_table, proc->in_handle);
    if (!hs) {
        DUAL_RET(thr, FOS_E_EMPTY, FOS_E_SUCCESS);
    }

    // If the handle is mapped, default to handle wait
    return hs_wait(hs);
}

KS_SYSCALL fernos_error_t ks_set_out_handle(kernel_state_t *ks, handle_t out) {
    if (!(ks->schedule.head)) {
        return FOS_E_STATE_MISMATCH;
    }

    thread_t *thr = (thread_t *)(ks->schedule.head);
    process_t *proc = thr->proc;

    handle_t NULL_HANDLE = idtb_null_id(proc->handle_table);

    if (out >= idtb_max_cap(proc->handle_table)) {
        proc->out_handle = NULL_HANDLE; 
    } else {
        proc->out_handle = out;
    }

    DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
}

KS_SYSCALL fernos_error_t ks_out_write(kernel_state_t *ks, const void *u_src, size_t len, size_t *u_written) {
    fernos_error_t err;

    if (!(ks->schedule.head)) {
        return FOS_E_STATE_MISMATCH;
    }

    thread_t *thr = (thread_t *)(ks->schedule.head);
    process_t *proc = thr->proc;

    handle_state_t *hs = idtb_get(proc->handle_table, proc->out_handle);
    if (!hs) { // dummy write implementation.
        if (!u_src) {
            DUAL_RET(thr, FOS_E_BAD_ARGS, FOS_E_SUCCESS);
        }

        if (u_written) {
            err = mem_cpy_to_user(proc->pd, u_written, &len, sizeof(size_t), NULL);
            DUAL_RET_FOS_ERR(err, thr);
        }

        DUAL_RET(thr, FOS_E_SUCCESS, FOS_E_SUCCESS);
    }

    // If the handle is mapped, default to handle write.
    return hs_write(hs, u_src, len, u_written);
}



