
#pragma once

#include "k_sys/page.h"
#include "s_data/id_table.h"

/**
 * A unique ID of a running or zombie process (Only uses first 16 of the full 32 bits)
 */
typedef id_t proc_id_t;

/**
 * A unique ID of a running or zombie thread (Only uses first 16 of the full 32 bits)
 *
 * NOTE: This is only "unique" among all threads of an individual process.
 * Threads that belong to different processes can share then same thread id.
 */
typedef id_t thread_id_t;

/**
 * A unique ID of a running or zombie thread (Uses full 32 bits)
 *
 * NOTE: This is GLOBALLY UNIQUE. 
 *
 * Should = (proc_id << 16) | (thread_id)
 */
typedef id_t global_thread_id_t;

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

    /**
     * When a process dies, its resources must be "reaped". Typically by who ever initially spawned
     * the process. (But really can be anyone)
     *
     * If a process A tries to reap another process B while B is still running, A will be marked as
     * the "beneficiary". When B does exit, A will be notified. 
     *
     * What happens when a beneficiary exits before the child dies??
     * Then what?? Maybe the parent organization is really much better??
     * Should there be some sort of process wide stuff here???
     * Might that be better for this design??
     * All processes have some sort of signal flag??
     * All processes have children??
     * I do like the idea of children here tbh...
     * Probably a little better IMO..
     */
    global_thread_id_t beneficiary;
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

    /**
     * Similar to a process, one thread can wait on another thread to return. The waiting thread
     * is also called a "beneficiary".
     *
     * Unlike with processes though, the beneficiary is not global. One thread can only wait on 
     * another if they both belong to the same process.
     */
    thread_id_t beneficiary;
} thread_t;

// If that process dies, then what?? Well, it'll wait until being reaped...

