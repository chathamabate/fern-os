
#pragma once

#include <stdint.h>
#include "k_startup/fwd_defs.h"
#include "s_mem/allocator.h"
#include "s_data/wait_queue.h"
#include "s_bridge/ctx.h"

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
     * A wait context isn't a bad idea, but how do we make this idea safe/well-designed
     */

     
    // Might it also be cool to have some sort of wait context??
    // Like something which should be done when waking up??
    // That would be pretty cool ngl??
    //
    // Why don't we just write to the registers though?
    // I feel like this could be more portable??
    // Nahhh... I don't think so!
    thread_id_t *u_joined_tid;
    void **u_joined_retval;

    /**
     * The context to use when switching back to this thread.
     */
    user_ctx_t ctx;
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
 * TBH, might end up deleting this endpoint... Thread deletion will involve a lot of kernel specific things.
 * Delete a thread!
 *
 * If this thread is in a wait queue, it is removed from that wait queue.
 * If this thread is scheduled, it is removed from the schedule list.
 * NOTE: When cleaning up a thread, you will likely need to perform some kernel specific
 * operations before calling this function!
 *
 * This DOES NOT edit the parent process or the overall kernel.
 */
void delete_thread(thread_t *thr);



