

#include "k_startup/page_helpers.h"
#include "s_util/constraints.h"
#include "k_startup/state.h"
#include "k_startup/thread.h"
#include "k_startup/process.h"
#include "k_startup/page.h"

#include "k_bios_term/term.h"

/*
 * Some helper macros for returning from these calls in both the kernel space and the user thread.
 */

#define DUAL_RET(thr, u_err, k_err) \
    do { \
        (thr)->ctx.eax = u_err; \
        return (k_err); \
    } while (0)

#define DUAL_RET_COND(cond, thr, u_err, k_err) \
    if (cond) { \
        DUAL_RET(thr, u_err, k_err); \
    }

#define DUAL_RET_FOS_ERR(err, thr) \
    if (err != FOS_SUCCESS) { \
        (thr)->ctx.eax = (err);  \
        return FOS_SUCCESS; \
    }

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

    ks->al = al;
    ks->curr_thread = NULL;
    ks->proc_table = pt;
    ks->root_proc = NULL;

    ks->curr_tick = 0;
    ks->sleep_q = twq;

    return ks;
}

void ks_save_ctx(kernel_state_t *ks, user_ctx_t *ctx) {
    if (ks->curr_thread) {
        ks->curr_thread->ctx = *ctx;
    }
}

void ks_schedule_thread(kernel_state_t *ks, thread_t *thr) {
    if (!(ks->curr_thread)) {
        thr->next_thread = thr;
        thr->prev_thread = thr;

        ks->curr_thread = thr;
    } else {
        thr->next_thread = ks->curr_thread;
        thr->prev_thread = ks->curr_thread->prev_thread;

        thr->next_thread->prev_thread = thr;
        thr->prev_thread->next_thread = thr;
    }

    thr->state = THREAD_STATE_SCHEDULED;
}

void ks_deschedule_thread(kernel_state_t *ks, thread_t *thr) {
    if (ks->curr_thread == thr) {
        ks->curr_thread = thr->next_thread;

        // The case where this is the only thread in the schedule!
        if (ks->curr_thread == thr) {
            ks->curr_thread = NULL;
        }
    }

    // These two lines should work fine regardless of how many threads
    // are in the schedule!
    thr->next_thread->prev_thread = thr->prev_thread;
    thr->prev_thread->next_thread = thr->next_thread;

    thr->next_thread = NULL;
    thr->prev_thread = NULL;

    thr->state = THREAD_STATE_DETATCHED;
}

fernos_error_t ks_tick(kernel_state_t *ks) {
    fernos_error_t err;

    bool not_halted = ks->curr_thread != NULL;

    ks->curr_tick++;

    twq_notify(ks->sleep_q, ks->curr_tick);

    thread_t *woken_thread;
    while ((err = twq_pop(ks->sleep_q, (void **)&woken_thread)) == FOS_SUCCESS) {
        // Remove reference to sleep queue.
        woken_thread->wq = NULL;
        woken_thread->state = THREAD_STATE_DETATCHED;

        // Schedule woken thread. 
        ks_schedule_thread(ks, woken_thread);
    }

    if (err != FOS_EMPTY) {
        return err;
    }

    /*
     * We only advance the schedule if we weren't halted when we entered this handler.
     *
     * Otherwise, the order threads are woken up doesn't always match when they execute.
     * (This doesn't really matter that much, but makes execution behavior more predictable)
     */
    if (not_halted) {
        ks->curr_thread = ks->curr_thread->next_thread;
    }

    return FOS_SUCCESS;
}

static fernos_error_t ks_exit_proc_p(kernel_state_t *ks, process_t *proc, 
        proc_exit_status_t status);

static fernos_error_t ks_signal_p(kernel_state_t *ks, process_t *proc, sig_id_t sid);

fernos_error_t ks_fork_proc(kernel_state_t *ks, proc_id_t *u_cpid) {
    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    thread_t *thr = ks->curr_thread;
    process_t *proc = thr->proc;

    // Reserve an ID for the child process.

    const proc_id_t NULL_PID = idtb_null_id(ks->proc_table);

    proc_id_t cpid = NULL_PID;
    process_t *child = NULL;
    fernos_error_t push_err = FOS_UNKNWON_ERROR;

    // Request child process ID.
    cpid = idtb_pop_id(ks->proc_table);

    // Create the child process.
    if (cpid != NULL_PID) {
        child = new_process_fork(proc, thr, cpid);
    }

    // Push child on to children list.
    if (child) {
        push_err = l_push_back(proc->children, &child);
    }

    // Now check for errors.
    if (cpid == NULL_PID || !child || push_err != FOS_SUCCESS) {
        idtb_push_id(ks->proc_table, cpid);
        delete_process(child);

        if (u_cpid) {
            cpid = FOS_MAX_PROCS;
            mem_cpy_to_user(proc->pd, u_cpid, &cpid, 
                    sizeof(proc_id_t), NULL);
        }

        // Since adding the child to the child list was our last step, it is impossible
        // that we are in this function if pushing the child to the list succeeded.
        //
        // Thus, we don't need to pop anything from our child list in this error case.

        DUAL_RET(thr, FOS_NO_MEM, FOS_SUCCESS);
    }

    // Success!

    idtb_set(ks->proc_table, cpid, child);

    ks_schedule_thread(ks, child->main_thread);

    if (u_cpid) {
        mem_cpy_to_user(proc->pd, u_cpid, &cpid, 
                sizeof(proc_id_t), NULL);
    }

    DUAL_RET(thr, FOS_SUCCESS, FOS_SUCCESS);
}

static fernos_error_t ks_exit_proc_p(kernel_state_t *ks, process_t *proc, 
        proc_exit_status_t status) {
    fernos_error_t err;

    if (ks->root_proc == proc) {
        term_put_fmt_s("\n[System Exited with Status 0x%X]\n", status);
        lock_up();
    }

    // First what do we do? Detatch all threads plz!

    const thread_id_t NULL_TID = idtb_null_id(proc->thread_table);

    idtb_reset_iterator(proc->thread_table);
    for (id_t iter = idtb_get_iter(proc->thread_table); iter != NULL_TID;
            iter = idtb_next(proc->thread_table)) {
        thread_t *worker =  idtb_get(proc->thread_table, iter);

        if (worker->state == THREAD_STATE_SCHEDULED) {
            ks_deschedule_thread(ks, worker);
        } else if (worker->state == THREAD_STATE_WAITING) {
            wq_remove((wait_queue_t *)(worker->wq), worker);
            worker->wq = NULL;
            worker->u_wait_ctx = NULL;
            worker->state = THREAD_STATE_DETATCHED;
        }
    }

    // If we have zombie children, we will need to signal the root process!
    bool signal_root = l_get_len(proc->zombie_children) > 0;

    // Next, we add all living and zombie children to the root process.

    err = l_push_back_all(ks->root_proc->children, proc->children);
    if (err != FOS_SUCCESS) {
        return err;
    }

    err = l_push_back_all(ks->root_proc->zombie_children, proc->zombie_children);
    if (err != FOS_SUCCESS) {
        return err;
    }

    // Now remove our process from the parent's children list, and move it to the parent's
    // zombie list!

    if (!l_pop_ele(proc->parent->children, &proc, l_ptr_cmp, false)) {
        return FOS_STATE_MISMATCH; // Something very wrong if we can't find this process in
                                   // the paren't children list.
    }

    // Now we are no longer in a living children list, mark exited.
    proc->exited = true;
    proc->exit_status = status;

    // Add to parents zombie children list!
    err = l_push_back(proc->parent->zombie_children, &proc);
    if (err != FOS_SUCCESS) {
        return err;
    }

    err = ks_signal(ks, proc->parent->pid, FSIG_CHLD);
    if (err != FOS_SUCCESS) {
        return err;
    }

    if (signal_root) {
        err = ks_signal(ks, ks->root_proc->pid, FSIG_CHLD);
        if (err != FOS_SUCCESS) {
            return err;
        }
    }

    // This call does not return to any user thread!
    return FOS_SUCCESS;
}

fernos_error_t ks_exit_proc(kernel_state_t *ks, proc_exit_status_t status) {
    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    return ks_exit_proc_p(ks, ks->curr_thread->proc, status);
}

fernos_error_t ks_reap_proc(kernel_state_t *ks, proc_id_t cpid, 
        proc_id_t *u_rcpid, proc_exit_status_t *u_rces) {
    fernos_error_t err;

    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    thread_t *thr = ks->curr_thread;
    process_t *proc = thr->proc;

    process_t *rproc = NULL;
    fernos_error_t user_err = FOS_UNKNWON_ERROR;

    // First we retrieve what zombie child we want to reap (and also remove it from the zombie list)
    // If we cannot find an applicable zombie child process, user_err will be set to the correct
    // error code.

    if (cpid == FOS_MAX_PROCS) {
        // We are reaping any process!

        // Default to error case at first.
        user_err = FOS_EMPTY;

        if (l_get_len(proc->zombie_children) > 0) {
            err = l_pop_front(proc->zombie_children, &rproc);

            // Failing to pop from the zombie list is seen as a fatal kernel error.
            // Should never really happen though.
            if (err != FOS_SUCCESS) {
                return err;
            }

            user_err = FOS_SUCCESS;
        } 
    } else {

        // Default to error case at first.
        user_err = FOS_STATE_MISMATCH;

        // We are reaping a specific process!
        rproc = idtb_get(ks->proc_table, cpid);

        if (rproc && rproc->parent == proc && rproc->exited == true) {
            bool removed = l_pop_ele(proc->zombie_children, &rproc, l_ptr_cmp, false);

            if (!removed) {
                // Something is very wrong if the described process is not in the zombie list.
                return FOS_STATE_MISMATCH;
            }

            user_err = FOS_SUCCESS;
        } 
    }

    proc_id_t rcpid = FOS_MAX_PROCS;
    proc_exit_status_t rces = PROC_ES_UNSET;

    // Ok, at this point, user_err is either FOS_SUCCESS, meaning we have a freestanding zombie
    // process stored in rproc which we can reap!
    //
    // Or, we couldn't find an apt process to reap.

    if (user_err == FOS_SUCCESS) {
        // REAP!
        
        rcpid = rproc->pid;
        rces = rproc->exit_status;

        delete_process(rproc);
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
    
    DUAL_RET(thr, user_err, FOS_SUCCESS);
}

static fernos_error_t ks_signal_p(kernel_state_t *ks, process_t *proc, sig_id_t sid) {
    fernos_error_t err;

    if (!proc || sid >= 32) {
        return FOS_BAD_ARGS;
    }

    // If it is already set, we do nothing!
    if (proc->sig_vec & (1 << sid)) {
        return FOS_SUCCESS;
    }

    // Set the signal bit no matter what!
    proc->sig_vec |= (1 << sid);

    // Ok, but is this signal even allowed in the first place??
    // If not, force exit the process.
    if (!(proc->sig_allow & (1 << sid))) {
        return ks_exit_proc_p(ks, proc, PROC_ES_SIGNAL);
    }

    // Otherwise, we notify our wait queue!
    
    err = vwq_notify(proc->signal_queue, sid, VWQ_NOTIFY_FIRST);
    if (err != FOS_SUCCESS) {
        return err;
    }

    thread_t *woken_thread;
    err = vwq_pop(proc->signal_queue, (void **)&woken_thread, NULL);

    if (err == FOS_SUCCESS) {
        // We woke a thread up!
        
        // Clear our bit real quick.
        proc->sig_vec &= ~(1 << sid);

        sig_id_t *u_sid = woken_thread->u_wait_ctx;

        // Detach thread.
        woken_thread->u_wait_ctx = NULL;
        woken_thread->wq = NULL;
        woken_thread->state = THREAD_STATE_DETATCHED;

        // Schedule thread and return correct values!
        ks_schedule_thread(ks, woken_thread);

        if (u_sid) {
            mem_cpy_to_user(woken_thread->proc->pd, u_sid, &sid, 
                    sizeof(sig_id_t), NULL);
        }

        woken_thread->ctx.eax = FOS_SUCCESS;
    } else if  (err != FOS_EMPTY) {
        // If the error was FOS_EMPTY, this is no big deal!
        return err;
    }

    return FOS_SUCCESS;
}

fernos_error_t ks_signal(kernel_state_t *ks, proc_id_t pid, sig_id_t sid) {
    fernos_error_t err;

    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    thread_t *thr = ks->curr_thread;

    if (pid == thr->proc->pid) {
        DUAL_RET(thr, FOS_BAD_ARGS, FOS_SUCCESS);
    }

    process_t *recv_proc;

    if (pid == FOS_MAX_PROCS) {
        recv_proc = thr->proc->parent;
    } else {
        recv_proc = idtb_get(ks->proc_table, pid);
    }

    if (!recv_proc) {
        // We couldn't find who to send to...
        DUAL_RET(thr, FOS_STATE_MISMATCH, FOS_SUCCESS);
    }

    err = ks_signal_p(ks, recv_proc, sid);
    if (err != FOS_SUCCESS) {
        return err;
    }
    
    DUAL_RET(thr, FOS_SUCCESS, FOS_SUCCESS);
}

fernos_error_t ks_allow_signal(kernel_state_t *ks, sig_vector_t sv) {
    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH; 
    }

    thread_t *thr = ks->curr_thread;
    process_t *proc = thr->proc;

    proc->sig_allow = sv;

    // Are there pending signals which are not allowed??
    if ((~(proc->sig_allow)) & proc->sig_vec) {
        return ks_exit_proc_p(ks, proc, PROC_ES_SIGNAL);
    }

    DUAL_RET(thr, FOS_SUCCESS, FOS_SUCCESS);
}

fernos_error_t ks_wait_signal(kernel_state_t *ks, sig_vector_t sv, sig_id_t *u_sid) {
    fernos_error_t err;
    
    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH; 
    }

    thread_t *thr = ks->curr_thread;
    process_t *proc = thr->proc;

    const sig_id_t NULL_SID = 32;

    if (sv == 0) {
        if (u_sid) {
            mem_cpy_to_user(proc->pd, u_sid, &NULL_SID, 
                    sizeof(sig_id_t), NULL);
        }

        DUAL_RET(thr, FOS_BAD_ARGS, FOS_SUCCESS);
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

            DUAL_RET(thr, FOS_SUCCESS, FOS_SUCCESS);
        }
    }

    // We didn't find a match, looks like we'll need to wait!

    err = vwq_enqueue(proc->signal_queue, thr, sv);
    if (err != FOS_SUCCESS) {
        return err;
    }

    ks_deschedule_thread(ks, thr);
    thr->wq = (wait_queue_t *)(proc->signal_queue);
    thr->state = THREAD_STATE_WAITING;
    thr->u_wait_ctx = u_sid;

    return FOS_SUCCESS;
}

fernos_error_t ks_sleep_thread(kernel_state_t *ks, uint32_t ticks) {
    fernos_error_t err;

    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    thread_t *thr = ks->curr_thread;

    err = twq_enqueue(ks->sleep_q, (void *)thr, 
            ks->curr_tick + ticks);
    if (err != FOS_SUCCESS) {
        return err;
    }

    // Only deschedule one we know our thread was added successfully to the wait queue!
    ks_deschedule_thread(ks, thr);

    thr->wq = (wait_queue_t *)(ks->sleep_q);
    thr->state = THREAD_STATE_WAITING;

    DUAL_RET(thr, FOS_SUCCESS, FOS_SUCCESS);
}

fernos_error_t ks_spawn_local_thread(kernel_state_t *ks, thread_id_t *u_tid, 
        thread_entry_t entry, void *arg) {
    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    thread_t *thr = ks->curr_thread;
    process_t *proc = thr->proc;

    const thread_id_t NULL_TID = idtb_null_id(proc->thread_table);

    DUAL_RET_COND(!entry, thr, FOS_BAD_ARGS, FOS_SUCCESS);

    thread_t *new_thr = proc_new_thread(proc, entry, arg);

    if (!new_thr) {
        thr->ctx.eax = FOS_UNKNWON_ERROR;

        if (u_tid) {
            mem_cpy_to_user(proc->pd, u_tid, &NULL_TID, sizeof(thread_id_t), NULL);
        }

        return FOS_SUCCESS;
    }

    // Schedule our new thread!
    ks_schedule_thread(ks, new_thr);

    thread_id_t new_tid = new_thr->tid;

    if (u_tid) {
        mem_cpy_to_user(proc->pd, u_tid, &new_tid, sizeof(thread_id_t), NULL);
    }

    DUAL_RET(thr, FOS_SUCCESS, FOS_SUCCESS);
}

fernos_error_t ks_exit_thread(kernel_state_t *ks, void *ret_val) {
    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    fernos_error_t err;

    thread_t *thr = ks->curr_thread;
    process_t *proc = thr->proc;

    // In case of main thread, just defer to process exit.
    if (thr == proc->main_thread) {
        return ks_exit_proc(ks, (proc_exit_status_t)ret_val);
    }

    // First, let's deschedule our current thread.

    ks_deschedule_thread(ks, thr);

    thr->exit_ret_val = ret_val;
    thr->state = THREAD_STATE_EXITED;

    thread_id_t tid = thr->tid;

    err = vwq_notify_first(proc->join_queue, (uint8_t)tid);

    // This really should never happen.
    if (err != FOS_SUCCESS) {
        return err;
    }

    thread_t *joining_thread;

    err = vwq_pop(proc->join_queue, (void **)&joining_thread, NULL);

    // No one was waiting on our thread so we just leave here successfully.
    // Remember, we don't reap our thread until someone else joins on it!
    if (err == FOS_EMPTY) {
        return FOS_SUCCESS;
    }

    // Some sort of pop error?
    if (err != FOS_SUCCESS) {
        return err;
    }

    // Ok, we found a joining thread, let's remove all information about its waiting state,
    // write return values, then schedule it!

    joining_thread->wq = NULL;
    thread_join_ret_t *u_ret_ptr = (thread_join_ret_t *)(joining_thread->u_wait_ctx);
    joining_thread->u_wait_ctx = NULL;
    joining_thread->state = THREAD_STATE_DETATCHED;

    if (u_ret_ptr) {
        thread_join_ret_t local_ret = {
            .retval = thr->exit_ret_val, 
            .joined = tid
        };

        mem_cpy_to_user(proc->pd, u_ret_ptr, &local_ret, sizeof(thread_join_ret_t), NULL);
    }
    joining_thread->ctx.eax = FOS_SUCCESS;

    ks_schedule_thread(ks, joining_thread);

    // Ok finally, since our exited thread is no longer needed
    proc_delete_thread(proc, thr, true);

    return FOS_SUCCESS;
}

fernos_error_t ks_join_local_thread(kernel_state_t *ks, join_vector_t jv, 
        thread_join_ret_t *u_join_ret) {
    fernos_error_t err;

    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    thread_t *thr = ks->curr_thread;
    process_t *proc = thr->proc;

    DUAL_RET_COND(
        jv == 0 || jv == (1U << thr->tid),
        thr, FOS_BAD_ARGS, FOS_SUCCESS
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

        DUAL_RET(thr, FOS_SUCCESS, FOS_SUCCESS);
    }

    // Here, we searched all threads and didn't find one to join on.
    // So, The current thread must wait!

    err = vwq_enqueue(proc->join_queue, thr, jv);
    DUAL_RET_FOS_ERR(err, thr);

    // Here we successfully queued our thread!
    // Now we can safely deschedule it!

    ks_deschedule_thread(ks, thr);
    thr->wq = (wait_queue_t *)(proc->join_queue);
    thr->u_wait_ctx = u_join_ret; // Save where we will eventually return to!
    thr->state = THREAD_STATE_WAITING;

    return FOS_SUCCESS;
}

fernos_error_t ks_register_futex(kernel_state_t *ks, futex_t *u_futex) {
    fernos_error_t err;

    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    thread_t *thr = ks->curr_thread;
    process_t *proc = thr->proc;

    err = proc_register_futex(proc, u_futex);
    DUAL_RET(thr, err, FOS_SUCCESS);
}

fernos_error_t ks_deregister_futex(kernel_state_t *ks, futex_t *u_futex) {
    fernos_error_t err;

    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    thread_t *thr = ks->curr_thread;
    process_t *proc = thr->proc;

    basic_wait_queue_t *wq = proc_get_futex_wq(proc, u_futex);
    DUAL_RET_COND(!wq, thr, FOS_INVALID_INDEX, FOS_SUCCESS);

    err = bwq_notify_all(wq);
    if (err != FOS_SUCCESS) {
        return err; // Failing to notify is more of a fatal kernel error tbh.
    }

    thread_t *woken_thread;
    while ((err = bwq_pop(wq, (void **)&woken_thread)) == FOS_SUCCESS) {
        woken_thread->wq = NULL;
        woken_thread->ctx.eax = FOS_STATE_MISMATCH;
        woken_thread->state = THREAD_STATE_DETATCHED;

        ks_schedule_thread(ks, woken_thread);
    }

    // Some sort of error with the popping.
    if (err != FOS_EMPTY) {
        return err;
    }

    // At this point, all previously waiting thread should've been scheduled...
    // Now it is safe to call `proc_deregister_futex`

    proc_deregister_futex(proc, u_futex);

    DUAL_RET(thr, FOS_SUCCESS, FOS_SUCCESS);
}

fernos_error_t ks_wait_futex(kernel_state_t *ks, futex_t *u_futex, futex_t exp_val) {
    fernos_error_t err;

    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    thread_t *thr = ks->curr_thread;
    process_t *proc = thr->proc;

    // Get the wait queue.
    basic_wait_queue_t *wq = proc_get_futex_wq(proc, u_futex);
    DUAL_RET_COND(!wq, thr, FOS_INVALID_INDEX, FOS_SUCCESS);

    // Read the futexes actual value.
    futex_t act_val;
    err = mem_cpy_from_user(&act_val, proc->pd, u_futex, sizeof(futex_t), NULL);

    // If a futex is in a futex map, it must be accessible, otherwise a fatal error.
    if (err != FOS_SUCCESS) {
        return err;
    }

    // Immediate return if values don't match!
    DUAL_RET_COND(act_val != exp_val, thr, FOS_SUCCESS, FOS_SUCCESS);

    // Ok, here the actual and expected value match. 
    // We can add our thread to the wait queue!

    err = bwq_enqueue(wq, thr);
    DUAL_RET_FOS_ERR(err, thr);
    
    ks_deschedule_thread(ks, thr);

    thr->wq = (wait_queue_t *)wq;
    thr->u_wait_ctx = NULL; // No context info needed for waiting on a futex.
    thr->state = THREAD_STATE_WAITING;

    return FOS_SUCCESS;
}

fernos_error_t ks_wake_futex(kernel_state_t *ks, futex_t *u_futex, bool all) {
    fernos_error_t err;

    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    thread_t *thr = ks->curr_thread;
    process_t *proc = thr->proc;

    // Get the wait queue.
    basic_wait_queue_t *wq = proc_get_futex_wq(proc, u_futex);
    DUAL_RET_COND(!wq, thr, FOS_INVALID_INDEX, FOS_SUCCESS);

    bwq_notify_mode_t mode = all ? BWQ_NOTIFY_ALL : BWQ_NOTIFY_NEXT;

    err = bwq_notify(wq, mode);
    if (err != FOS_SUCCESS) {
        return err; // fatal kernel error.
    }

    // Regardless of the mode, we'll just wake up in a loop. In the case of "NOTIFY_NEXT",
    // this loop should only iterate at most once.

    thread_t *woken_thread;
    while ((err = bwq_pop(wq, (void **)&woken_thread)) == FOS_SUCCESS) {
        woken_thread->wq = NULL;
        woken_thread->ctx.eax = FOS_SUCCESS;
        woken_thread->state = THREAD_STATE_DETATCHED;

        ks_schedule_thread(ks, woken_thread);
    }

    if (err != FOS_EMPTY) {
        return err;
    }

    DUAL_RET(thr, FOS_SUCCESS, FOS_SUCCESS);
}

