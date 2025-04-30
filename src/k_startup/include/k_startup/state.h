
#pragma once

#include "s_data/id_table.h"
#include "s_data/wait_queue.h"
#include "k_sys/page.h"
#include "s_mem/allocator.h"
#include "k_startup/fwd_defs.h"

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
};

/**
 * Create a kernel state with basically no details.
 *
 * Returns NULL on error.
 */
kernel_state_t *new_blank_kernel_state(allocator_t *al);

static inline kernel_state_t *new_da_blank_kernel_state(void) {
    return new_blank_kernel_state(get_default_allocator());
}
