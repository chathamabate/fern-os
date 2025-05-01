
#pragma once

#include "s_data/id_table.h"
#include "s_data/wait_queue.h"
#include "k_sys/page.h"
#include "s_mem/allocator.h"
#include "k_startup/fwd_defs.h"
#include "s_bridge/ctx.h"

struct _kernel_state_t {
    /**
     * Allocator to be used by the kernel!
     */
    allocator_t *al;

    /**
     * The currently executing thread.
     *
     * NOTE: The schedule forms a cyclic doubly linked list.
     *
     * During a context switch curr_thread is set to curr_thread->next.
     *
     * TODO: What should happen when there is no current thread??
     */
    thread_t *curr_thread;

    /**
     * Every process will have a globally unique ID!
     */
    id_table_t *proc_table;

    /**
     * When the root proecess exits, the whole kernel should exit!
     */
    process_t *root_proc;

    /**
     * This counter is initialized as 0 and incremented every time the
     * timer interrupt handler is entered.
     */
    uint32_t curr_tick;

    /**
     * Threads that call the sleep system call will be placed in this queue.
     */
    timed_wait_queue_t *sleep_q;
};

/**
 * Create a kernel state with basically no details.
 *
 * Returns NULL on error.
 */
kernel_state_t *new_kernel_state(allocator_t *al);

static inline kernel_state_t *new_da_kernel_state(void) {
    return new_kernel_state(get_default_allocator());
}

/**
 * This function saves the given context into the current thread.
 *
 * Undefined behavior if there is no current thread.
 */
void ks_save_curr_thread_ctx(kernel_state_t *ks, user_ctx_t *ctx);

/**
 * Takes a thread and adds it to the schedule!
 *
 * We expect the given thread to be in a detatched state.
 * (However, this is not checked, so be careful!)
 *
 * Undefined behavoir will occur if the same thread appears twice
 * in the schedule! So make sure this never happens!
 */
void ks_schedule_thread(kernel_state_t *ks, thread_t *thr);

/**
 * Remove a scheduled thread from the schedule!
 *
 * Like above, we don't actually check if the given thread is scheduled
 * or not, we just assume it is! (SO BE CAREFUL)
 */
void ks_deschedule_thread(kernel_state_t *ks, thread_t *thr);

/**
 * Take the current thread, deschedule it, and add it it to the sleep wait queue.
 *
 * (Undefined behavior if there is no current thread)
 *
 * Returns an error if there are insufficient resources. In this case the thread will 
 * remain scheduled.
 */
fernos_error_t ks_sleep_curr_thread(kernel_state_t *ks, uint32_t ticks);

/**
 * Spawn new thread in the current process using entry and arg.
 *
 * On success, if tid is given, the new thread's id is writen to *tid.
 * On error, if tid is given, the null thread id is written to tid.
 *
 * The created thread is added to the schedule!
 *
 * Undefined behavior if no current thread.
 */
fernos_error_t ks_branch_curr_thread(kernel_state_t *ks, thread_id_t *tid, 
        thread_entry_t entry, void *arg);

