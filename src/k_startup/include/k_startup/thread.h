
#pragma once

#include "k_startup/fwd_defs.h"
#include "s_data/wait_queue.h"
#include "s_mem/allocator.h"
#include "s_util/err.h"
#include <stdbool.h>

typedef uint32_t thread_state_t;

/**
 * When the thread is neither scheduled nor waiting.
 */
#define THREAD_STATE_DETATCHED (0)

/**
 * The thread is scheduled! (Which means it could be currently executing)
 */
#define THREAD_STATE_SCHEDULED (1)

/**
 * The thread is part of some wait queue.
 */
#define THREAD_STATE_WAITING   (2)

struct _thread_t {
    /**
     * The allocator used to create this thread!
     */
    allocator_t * const al;

    /**
     * The state of this thread.
     */
    thread_state_t state;

    /**
     * These are used for scheduling only.
     */
    thread_t *next_thread;
    thread_t *prev_thread;

    /**
     * The process local id of this thread.
     */
    const thread_id_t tid;

    /**
     * The process this thread executes out of.
     */
    process_t * const proc;

    /**
     * If this thread is in a executing state, this field will be NULL.
     *
     * Otherwise, this thread MUST belong to some waiting queue.
     * A reference to the queue will be stored here.
     */
    wait_queue_t *wq;

    /**
     * ESP to use when switching back to this thread.
     *
     * When a thread is switched out of, it is expected that `pushad` is called.
     * When we switch back into a thread, we will call `popad` followed by `iret`.
     */
    const uint32_t *esp;
};

/**
 * Returns NULL if insufficient resources are available to allocate.
 */
thread_t *new_thread(allocator_t *al, thread_id_t tid, process_t *proc,  const uint32_t *esp);

/**
 * This call is very simple, it just detatches the thread, and frees the memory used for the
 * thread_t structure. MOST CLEANUP will need to be done by the parent process/OS.
 */
void delete_thread(thread_t *thr);

static inline thread_state_t thr_get_state(thread_t *thr) {
    return thr->state;
}

/**
 * Schedule a thread!
 *
 * Schedule between nodes p and n. If p and n are not adjacent, an error is returned!
 *
 * This thread must be in a detatched state to schedule! otherwise an error is returned!
 */
fernos_error_t thr_schedule(thread_t *thr, thread_t *p, thread_t *n);

/**
 * Set a threads wait queue!
 *
 * Returns an error if not detatched!
 *
 * NOTE: We assume someone else has already added this thread to wq.
 * We cannot do it in this function, because we do not know what concrete wait queue
 * implementation is being used.
 */
fernos_error_t thr_wait(thread_t *thr, wait_queue_t *wq);

/**
 * Bring a thread to the detatched state.
 *
 * If already detatched, do nothing!
 *
 * If scheduled, remove from the schedule list.
 * If waiting, remove from the wait queue.
 */
void thr_detatch(thread_t *thr);
