
#pragma once

#include "k_sys/page.h"
#include "k_startup/fwd_defs.h"
#include "s_data/id_table.h"

// Basically, does the wait queue actually do the scheduling though??
// How does that work, maybe that can be given as an interface??
// Ehh, might be abstraction for now reason... 
//
// Well, let's wait, maybe there is an easy way we can pull this all together soon!
// Could the wait queue come with a scheduler??

// Worth doing all this shit at somepoint tomorrow maybe because it's getting late...

typedef struct _process_t {
    /**
     * ID of this process.
     */
    proc_id_t pid;

    /**
     * The page directory used by this process.
     */
    phys_addr_t pd;

    /**
     * Table of threads. (id_t -> thread_t *)
     */
    id_table_t *threads;

    /**
     * Has this process exited or not.
     */
    bool exited;

    /**
     * Code given by the process when it exits.
     */
    int32_t exit_code;

} process_t;

typedef struct _thread_t {
    /**
     * The process ID of the process this thread belongs to.
     */
    proc_id_t pid;

    /**
     * This threads tid.
     */
    thread_id_t tid;

    // Not needed since it can be inferred from the pid and tid above.
    // global_thread_id_t gtid;

    /**
     * The stack pointer of the thread between context switches.
     */
    void *esp;

    /**
     * Has this thread returned?
     */
    bool returned;

    /**
     * The value returned.
     */
    void *ret_val;
} thread_t;


