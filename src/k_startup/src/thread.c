
#include "k_startup/thread.h"
#include "k_startup/fwd_defs.h"
#include "k_startup/gdt.h"
#include "s_data/wait_queue.h"
#include "s_mem/allocator.h"
#include "s_util/constraints.h"
#include "s_util/err.h"

thread_t *new_thread(allocator_t *al, thread_id_t tid, process_t *proc, thread_entry_t entry) {
    thread_t *thr = al_malloc(al, sizeof(thread_t));
    if (!thr) {
        return NULL;
    }

    if (tid >= FOS_MAX_THREADS_PER_PROC) {
        return NULL;
    }

    thr->al = al;
    thr->state = THREAD_STATE_DETATCHED;

    thr->next_thread = NULL;
    thr->prev_thread = NULL;

    thr->tid = tid;

    thr->proc = proc;
    thr->wq = NULL;


    thr->ctx.ds = USER_DATA_SELECTOR;
    //thr->esp = esp;

    return thr;
}

/*
thread_t *new_thread(allocator_t *al, thread_id_t tid, process_t *proc,  const uint32_t *esp) {
    thread_t *thr = al_malloc(al, sizeof(thread_t));
    if (!thr) {
        return NULL;
    }

    *(allocator_t **)&(thr->al) = al;
    thr->state = THREAD_STATE_DETATCHED;

    thr->next_thread = NULL;
    thr->prev_thread = NULL;

    *(thread_id_t *)&(thr->tid) = tid;
    *(process_t **)&(thr->proc) = proc;
    thr->wq = NULL;
    thr->esp = esp;

    return thr;
}

void delete_thread(thread_t *thr) {
    thr_detatch(thr);
    al_free(thr->al, thr);
}

fernos_error_t thr_schedule(thread_t *thr, thread_t *p, thread_t *n) {
    if (p && p->next_thread != n) {
        return FOS_BAD_ARGS;
    }

    if (n && n->prev_thread != p) {
        return FOS_BAD_ARGS;
    }

    if (thr->state != THREAD_STATE_DETATCHED) {
        return FOS_STATE_MISMATCH;
    }

    thr->next_thread = n; 
    if (n) {
        n->prev_thread = thr;
    }

    thr->prev_thread = p;
    if (p) {
        p->next_thread = thr;
    }

    return FOS_SUCCESS;
}

fernos_error_t thr_wait(thread_t *thr, wait_queue_t *wq) {
    if (thr->state != THREAD_STATE_DETATCHED) {
        return FOS_STATE_MISMATCH;
    }

    thr->wq = wq;
    thr->state = THREAD_STATE_WAITING;

    return FOS_SUCCESS;
}

void thr_detatch(thread_t *thr) {
    if (thr->state == THREAD_STATE_WAITING) {
        wq_remove(thr->wq, thr); 
    } else if (thr->state == THREAD_STATE_SCHEDULED) {
        if (thr->prev_thread) {
            thr->prev_thread->next_thread = thr->next_thread;
            thr->prev_thread = NULL;
        }

        if (thr->next_thread) {
            thr->next_thread->prev_thread = thr->prev_thread;
            thr->next_thread = NULL;
        }
    }

    thr->state = THREAD_STATE_DETATCHED;
}
*/
