
#pragma once

#include "k_sys/page.h"
#include "s_data/id_table.h"

// My second kernel organization attempt.

/*
 * Ok, and how would I wait on something like a process dying?
 * Or joining a thread??
 *
 * Ever think about that one??
 * Does that fit into this "condition" model??
 *
 * I wish there was one easy way to figure out thread sleeping.
 *
 * Maybe a thread sleeps with some ID and reason?
 *
 * For example, (JOIN, Thread ID) = I am sleeping until a thread with this ID exits.
 * or           (REAP, 0/PID) = I am sleeping until a process dies.
 *
 * Eh, wouldn't it be better to have some sort of listeners... to prevent all need for iteration?
 * What if a listener exits? Well, then I don't really know tbh...
 *
 * Ok, this isn't such a bad idea tbh...
 * Conditions could still come later... (As a pseudo IPC?)
 *
 * A listener is a thread which is asleep, waiting for something to happen.
 * It's someone else's responsibility to wake up the thread when the time is right.
 *
 * How do I make this work???
 *
 * A problem is that ID's will be reused, If we just pass around IDs, it's hard to confirm we are
 * speaking to the original ID holder...
 *
 * Well, what if we promise that the thread/process ID won't be reused until being reaped?
 * That's an idea... 
 *
 * When a process/thread dies, they can define what??? beneficiary, who should be notified of the 
 * death. If there is no beneficiary, then what? We just sit there anyway.
 *
 * Maybe conditions can come later???
 */

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

