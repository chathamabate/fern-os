
#pragma once

#include <stdint.h>
#include "k_startup/fwd_defs.h"
#include "s_mem/allocator.h"
#include "s_data/wait_queue.h"
#include "s_data/ring.h"
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
     * A thread will also be a ring element. (The ring being a schedule)
     */
    ring_element_t super;

    /**
     * The state of this thread.
     */
    thread_state_t state;

    /**
     * The process local id of this thread.
     */
    thread_id_t tid;

    /**
     * This keeps track of how much this thread's stack is allocated.
     *
     * It is your choice to use this or not, It is set to NULL initially.
     */
    void *stack_base;

    /**
     * The process this thread executes out of.
     */
    process_t * const proc;

    /**
     * When a thread is in the waiting state, this field is populated.
     * Otherwise, this field is NULL.  
     */
    wait_queue_t *wq;

    /**
     * When a thread is woken up, you'll likely want to know some information about why it went
     * to sleep. For example, maybe you were trying to read to some userspace buffer.
     *
     * These fields may be used differently depending on why a thread went to sleep!
     *
     * NOTE: This field used to be a single userspace pointer. I have upgraded it to a set
     * of arbitrary values to allow for more flexibility.
     */
    uint32_t wait_ctx[6];

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
 * This DOES NOT allocate into the owning process's page directory.
 * It set the %esp and stack_base to the very end of the stack area associated with `tid`.
 *
 * We'd expect that when this thread starts executing, a page fault occurs immediately, which
 * would allocate the very first stack page!
 *
 * Also, this function does not edit the parent process in any way. It it your responsibility
 * to actually place the created thread into the process's thread table!
 *
 * Lastly, threads assume the same allocator has the parent process.
 */
thread_t *new_thread(process_t *proc, thread_id_t tid, uintptr_t entry, 
        uint32_t arg0, uint32_t arg1, uint32_t arg2);

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
 * If a thread is scheduled, the thread is descheduled.
 * If a thread is waiting, the thread is removed from its wait queue.
 * (It's wait queue is set to NULL, and context set to 0... SO BE CAREFUL)
 * If the thread is already detached, do nothing.
 * If the thread is in an exited state, do nothing.
 */
void thread_detach(thread_t *thr);

/**
 * Add a thread to schedule `r`. 
 * If `thr` is not detached, `thread_detach` will be called before `thr` is added to `r`.
 */
void thread_schedule(thread_t *thr, ring_t *r);

/**
 * Delete a thread.
 *
 * NOTE: Before deleting, the thread is detached from its wait queue or schedule!
 *
 * NOTHING is done to the process's page directory!
 */
void delete_thread(thread_t *thr);
