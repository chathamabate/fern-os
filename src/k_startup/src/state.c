

#include "s_util/constraints.h"
#include "k_startup/state.h"
#include "k_startup/thread.h"
#include "k_startup/process.h"

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

void ks_save_curr_thread_ctx(kernel_state_t *ks, user_ctx_t *ctx) {
    ks->curr_thread->ctx = *ctx;
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

fernos_error_t ks_sleep_curr_thread(kernel_state_t *ks, uint32_t ticks) {
    fernos_error_t err;

    thread_t *thr = ks->curr_thread;

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

fernos_error_t ks_branch_curr_thread(kernel_state_t *ks, thread_id_t *tid,
        thread_entry_t entry, void *arg) {
    if (!entry) {
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

