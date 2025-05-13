

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
        // Prepare to be scheduled!
        woken_thread->next_thread = NULL;
        woken_thread->prev_thread = NULL;
        woken_thread->state = THREAD_STATE_DETATCHED;

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

fernos_error_t ks_sleep_thread(kernel_state_t *ks, uint32_t ticks) {
    fernos_error_t err;

    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    thread_t *thr = ks->curr_thread;

    err = twq_enqueue(ks->sleep_q, (void *)thr, 
            ks->curr_tick + ticks);

    DUAL_RET_FOS_ERR(err, thr);

    // Only deschedule one we know our thread was added successfully to the wait queue!
    ks_deschedule_thread(ks, thr);
    thr->wq = (wait_queue_t *)(ks->sleep_q);

    return FOS_SUCCESS;
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

    fernos_error_t err;

    thread_t *new_thr;

    err = proc_create_thread(proc, &new_thr, entry, arg);

    if (err != FOS_SUCCESS) {
        thr->ctx.eax = err;

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

    // First, let's deschedule our current thread.

    ks_deschedule_thread(ks, thr);

    thr->exit_ret_val = ret_val;
    thr->state = THREAD_STATE_EXITED;

    if (thr == ks->root_proc->main_thread) {
        // TODO. (System Exit)
        return FOS_NOT_IMPLEMENTED;
    }

    if (thr == proc->main_thread) {
        // TODO. (Process Exit)
        return FOS_NOT_IMPLEMENTED;
    }

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
    proc_reap_thread(proc, thr, true);

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
        proc_reap_thread(proc, joinable_thread, true);

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
    thr->u_wait_ctx = u_join_ret; // Save where we will eventually return to!
    thr->wq = (wait_queue_t *)(proc->join_queue);

    return FOS_SUCCESS;
}

fernos_error_t ks_register_futex(kernel_state_t *ks, futex_t *u_futex) {
    fernos_error_t err;

    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    thread_t *thr = ks->curr_thread;
    process_t *proc = thr->proc;
    map_t *fm = proc->futexes;

    // Never register a NULL futex.
    if (!u_futex) {
        thr->ctx.eax = FOS_BAD_ARGS;
        return FOS_SUCCESS;
    }

    // Check if the given futex is already in the map.

    if (mp_get(fm, &u_futex)) {
        thr->ctx.eax = FOS_ALREADY_ALLOCATED;
        return FOS_SUCCESS;
    }

    // Try creating the futex wait queue and placing it in the map.

    basic_wait_queue_t *fwq = new_basic_wait_queue(proc->al);
    if (!fwq) {
        thr->ctx.eax = FOS_NO_MEM;
        return FOS_SUCCESS;
    }

    err = mp_put(fm, &u_futex, &fwq);
    if (err != FOS_SUCCESS) {
        delete_wait_queue((wait_queue_t *)fwq);

        thr->ctx.eax = err;
        return FOS_SUCCESS;
    }

    // Ok, I think this is a success!

    thr->ctx.eax = FOS_SUCCESS;
    return FOS_SUCCESS;
}

fernos_error_t ks_deregister_futex(kernel_state_t *ks, futex_t *u_futex) {
    fernos_error_t err;

    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    thread_t *thr = ks->curr_thread;
    process_t *proc = thr->proc;

    basic_wait_queue_t *wq;

    basic_wait_queue_t **t_wq = (basic_wait_queue_t **)mp_get(proc->futexes, &u_futex);
    if (!t_wq) {
        thr->ctx.eax = FOS_INVALID_INDEX;
        return FOS_SUCCESS;
    }

    wq = *t_wq;

    err = bwq_notify_all(wq);
    if (err != FOS_SUCCESS) {
        return err; // Failing to notify is more of a fatal kernel error tbh.
    }

    thread_t *woken_thread;
    while ((err = bwq_pop(wq, (void **)&woken_thread)) == FOS_SUCCESS) {
        // Prepare to be scheduled!
        woken_thread->next_thread = NULL;
        woken_thread->prev_thread = NULL;
        woken_thread->state = THREAD_STATE_DETATCHED;

        woken_thread->ctx.eax = FOS_STATE_MISMATCH;

        ks_schedule_thread(ks, woken_thread);
    }

    // Some sort of error with the popping.
    if (err != FOS_EMPTY) {
        return err;
    }

    // At this point, all previously waiting thread should've been scheduled...
    // Now just delete the wait queue and remove it from the map?

    mp_remove(proc->futexes, &u_futex);
    delete_wait_queue((wait_queue_t *)wq);

    return FOS_SUCCESS;
}

fernos_error_t ks_wait_futex(kernel_state_t *ks, futex_t *u_futex, futex_t exp_val) {
    fernos_error_t err;

    if (!(ks->curr_thread)) {
        return FOS_STATE_MISMATCH;
    }

    thread_t *thr = ks->curr_thread;
    process_t *proc = thr->proc;

    basic_wait_queue_t *wq;


    futex_t act_val;
    err = mem_cpy_from_user(&act_val, proc->pd, u_futex, sizeof(futex_t), NULL);
    if (err != FOS_SUCCESS) { 
        thr->ctx.eax = err;
        return FOS_SUCCESS;
    }

    
    

    return FOS_NOT_IMPLEMENTED;
}

fernos_error_t ks_wake_futex(kernel_state_t *ks, futex_t *u_futex, bool all) {
    return FOS_NOT_IMPLEMENTED;
}

