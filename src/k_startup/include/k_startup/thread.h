
#pragma once

#include <stdint.h>
#include "k_startup/fwd_defs.h"
#include "s_mem/allocator.h"
#include "s_data/wait_queue.h"

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
    allocator_t *al;

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
    thread_id_t tid;

    /**
     * The process this thread executes out of.
     */
    process_t *proc;

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
 * Allocate a new thread with basically no details.
 */
thread_t *new_blank_thread(allocator_t *al);



