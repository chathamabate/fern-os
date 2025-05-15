
#pragma once

#include <stdint.h>
#include "k_startup/fwd_defs.h"
#include "s_mem/allocator.h"
#include "s_data/wait_queue.h"
#include "s_data/destructable.h"
#include "s_bridge/ctx.h"
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

/**
 * The thread has exited and is holding its resources until joined.
 */
#define THREAD_STATE_EXITED    (3)

struct _thread_t {
    /**
     * The state of this thread.
     */
    thread_state_t state;

    /**
     * These are used for scheduling only.
     *
     * We expect when threads are scheduled, that they belong to a cyclic
     * doubly linked list. (i.e. these two pointers should never be NULL when scheduled)
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
     * When a thread is in the waiting state, this field is populated.
     * Otherwise, this field is NULL.
     */
    wait_queue_t *wq;

    /**
     * When a thread is woken up, you'll likely want to return write some information to its
     * memory space. This pointer is just an arbitrary user pointer for this use.
     *
     * When the thread is not in a waiting state, this field should always be NULL.
     */
    void *u_wait_ctx;

    /**
     * The context to use when switching back to this thread.
     */
    user_ctx_t ctx;

    /**
     * When a thread exits, its return value is stored here.
     *
     * This will likely be a straight integral value or a pointer into userspace.
     */
    void *exit_ret_val;
};

/**
 * Create a new thread!
 *
 * Returns NULL if we have insufficient memory to create the thread. Or if arguments are invalid.
 *
 * This DOES NOT allocate into the owning process's page directory, that is your responsibility.
 * This will set %esp in the created thread to the correct position given its TID.
 *
 * Also, this function does not edit the parent process in any way. It it your responsibility
 * to actually place the created thread into the process's thread table!
 *
 * Lastly, threads assume the same allocator has the parent process.
 */
thread_t *new_thread(process_t *proc, thread_id_t tid, thread_entry_t entry, void *arg);

/**
 * Copy a given thread.
 *
 * The created thread is always in a detached stated! The only data which is actually copied
 * is the given thread's TID and context!
 *
 * new_proc will be the process of this copied thread! cr3 in the new thread's context
 * will be replaced with the page directory of the given process.
 *
 * NOTE: This assumes that the given process's page directory already has the correct stack pages
 * allocated!
 *
 * NULL is returned in case of error.
 */
thread_t *new_thread_copy(thread_t *thr, process_t *new_proc);

/**
 * This call is very simple/dangerous.
 *
 * Given any thread structure, it just frees it, nothing is done to the page directory.
 * Additionally, no checks are done on the thread's state.
 *
 * If you want this to be removed from some wait queue or schedule, you must do this yourself
 * before calling this function!
 */
void delete_thread(thread_t *thr);
