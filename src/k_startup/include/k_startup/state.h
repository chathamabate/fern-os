
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
 * Does nothing if there is no current thread.
 */
void ks_save_ctx(kernel_state_t *ks, user_ctx_t *ctx);

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
 * This function advances the kernel's tick counter.
 *
 * (This also updates the sleep queue and schedule automatically)
 *
 * Returns an error if there is some issue dealing with the sleep queue.
 */
fernos_error_t ks_tick(kernel_state_t *ks);

/**
 * Take the current thread, deschedule it, and add it it to the sleep wait queue.
 *
 * Returns an error if there is no current thread.
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
 * Returns error if there is no current thread!
 */
fernos_error_t ks_spawn_local_thread(kernel_state_t *ks, thread_id_t *tid, 
        thread_entry_t entry, void *arg);

/**
 * Exits the current thread of the current process.
 *
 * This detatches the current thread from the schedule.
 *
 * When a non-main thread is exited, It's id is sifted through its parent's join_queue to see 
 * if any thread was waiting on its exit. If a thread was waiting, it will be removed from 
 * the join queue and scheduled!
 *
 * If the current thread is the "main thread" of a process, the entire process should exit.
 * If the current thread is the "main thread" of the root process, FOS_ABORT_SYSTEM is returned.
 *
 * Returns other errors if there is no current thread, or if something goes wrong with the exit.
 */
fernos_error_t ks_exit_curr_thread(kernel_state_t *ks, void *ret_val);

/**
 * Have the current thread join on a given join vector.
 *
 * "join'ing" means to not be scheduled again until one of the vectors outlined in jv has exited.
 * 1 thread can wait on multiple threads, but multiple threads should never wait on the same thread!
 *
 * If one of the threads listed in the join vector has already exited, but is yet to be joined,
 * the current thread will remain scheduled!
 * 
 * Returns an error if the join vector is invalid.
 *
 * NOTE: VERY VERY IMPORTANT!
 * u_tid and u_retval are USERSPACE pointers!!
 * They will be saved until the joining thread wakes up. Before being rescheduled, the tid
 * and return value of the joined thread are written to userspace at these addresses!
 *
 * This is kinda hacky, but just remember, the userspace verion of this call essentially blocks.
 * This call here though in kernel space doesn't!
 */
fernos_error_t ks_join_local_thread(kernel_state_t *ks, join_vector_t jv, 
        thread_id_t *u_tid, void **u_retval);
