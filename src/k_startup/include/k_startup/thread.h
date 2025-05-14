
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
     * No longer needed, inferred from parent process.
     */
    // allocator_t *al;

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
 * This function will also allocate the initial stack page for the created thread in the parent proc's
 * page directory. NOTE: This function does NOT fail if the stack area is already allocated.
 * So be careful! Make sure one thread is only ever using one stack area!
 *
 * Also, this function does not edit the parent process in anyway. It it your responsibility
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
 * NULL is returned in case of error.
 */
thread_t *new_thread_copy(thread_t *thr, process_t *new_proc);

/**
 * If the given thread is not in the exited state, this call does nothing!
 *
 * When in the exited state, this call simply deletes the thread's resources.
 * This does not modify the parent process at all, that's your responsibility!
 *
 * If you'd like to free this thread's stack pages, set return stack to true!
 */
void reap_thread(thread_t *thr, bool return_stack);
