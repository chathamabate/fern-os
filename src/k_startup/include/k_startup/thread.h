
#pragma once

#include <stdint.h>
#include "k_startup/fwd_defs.h"
#include "s_mem/allocator.h"
#include "s_data/wait_queue.h"
#include "s_bridge/ctx.h"

/**
 * When a new thread is created, its entry point must follow this interface.
 */
typedef void *(*thread_entry_t)(void *arg);

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
     * The context to use when switching back to this thread.
     */
    user_ctx_t ctx;
};

thread_t *new_thread(allocator_t *al, thread_id_t tid, process_t *proc, thread_entry_t entry);

static inline thread_t *new_da_thread(thread_id_t tid, process_t *proc, thread_entry_t entry) {
    return new_thread(get_default_allocator(), tid, proc, entry);
}



