
#pragma once

#include "k_startup/fwd_defs.h"
#include "s_data/wait_queue.h"

struct _thread_t {
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
    uint32_t *esp;
};
