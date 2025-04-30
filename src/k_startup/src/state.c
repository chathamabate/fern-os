

#include "s_util/constraints.h"
#include "k_startup/state.h"
#include "k_startup/thread.h"

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

