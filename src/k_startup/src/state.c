

#include "k_startup/page_helpers.h"
#include "s_util/constraints.h"
#include "k_startup/state.h"
#include "k_startup/thread.h"
#include "k_startup/process.h"
#include "k_startup/page.h"

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

fernos_error_t ks_sleep_curr_thread(kernel_state_t *ks, uint32_t ticks) {
    fernos_error_t err;

    thread_t *thr = ks->curr_thread;

    if (!thr) {
        return FOS_BAD_ARGS;
    }

    err = twq_enqueue(ks->sleep_q, (void *)thr, 
            ks->curr_tick + ticks);

    if (err != FOS_SUCCESS) {
        return err;
    }

    // Only deschedule one we know our thread was added successfully to the wait queue!
    ks_deschedule_thread(ks, thr);
    thr->wq = (wait_queue_t *)(ks->sleep_q);

    return FOS_SUCCESS;
}

fernos_error_t ks_spawn_local_thread(kernel_state_t *ks, thread_id_t *tid, 
        thread_entry_t entry, void *arg) {
    if (!entry || !tid || !(ks->curr_thread)) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err;

    process_t *curr_proc = ks->curr_thread->proc;

    thread_t *new_thr;

    err = proc_create_thread(curr_proc, &new_thr, entry, arg);

    if (err != FOS_SUCCESS) {
        if (tid) {
            *tid = idtb_null_id(curr_proc->thread_table);
        }

        return err;
    }

    // Schedule our new thread!
    ks_schedule_thread(ks, new_thr);

    if (tid) {
        *tid = new_thr->tid;
    }

    return FOS_SUCCESS;
}

fernos_error_t ks_exit_curr_thread(kernel_state_t *ks, void *ret_val) {
    if (!(ks->curr_thread)) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err;

    thread_t *thr = ks->curr_thread;
    process_t *proc = thr->proc;

    // First, let's deschedule our current thread.

    ks_deschedule_thread(ks, thr);

    thr->exit_ret_val = ret_val;
    thr->state = THREAD_STATE_EXITED;

    if (thr == ks->root_proc->main_thread) {
        // TODO.
        return FOS_NOT_IMPLEMENTED;
    }

    if (thr == proc->main_thread) {
        // TODO.
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
    if (err == FOS_EMPTY) {
        return FOS_SUCCESS;
    }

    // Some sort of pop error?
    if (err != FOS_SUCCESS) {
        return err;
    }

    // Ok, we found a joining queue, let's remove all information about its waiting state,
    // write return values, then schedule it!

    joining_thread->wq = NULL;

    thread_join_ret_t *u_ret_ptr = (thread_join_ret_t *)(joining_thread->u_wait_ctx);
    joining_thread->u_wait_ctx = NULL;

    thread_join_ret_t local_ret = {
        .retval = thr->exit_ret_val, 
        .joined = tid
    };

    mem_cpy_to_user(proc->pd, u_ret_ptr, &local_ret, sizeof(thread_join_ret_t), NULL);
    joining_thread->ctx.eax = FOS_SUCCESS;

    ks_schedule_thread(ks, joining_thread);

    // Ok finally, since our exited thread is no longer needed

    return FOS_SUCCESS;
}

fernos_error_t ks_join_local_thread(kernel_state_t *ks, join_vector_t jv, 
        thread_join_ret_t *u_join_ret) {
    return FOS_NOT_IMPLEMENTED;
}

