
#pragma once

#include "k_sys/page.h"
#include "s_util/err.h"
#include <stdint.h>
#include <stdbool.h>

#define MAX_THREADS_PER_PROC 0x10U
#define MAX_PROCS 0x100U

/**
 * The id of a thread within a process. This is unique across all threads within a process, but not
 * all threads globally. (Use only first 16 bits)
 *
 * (MUST BE LESS THAN MAX_THREADS_PER_PROC)
 */
typedef uint32_t thread_id_t;

/**
 * ID of a process. Unique across all processs in the operating system. (Use only first 16 bits)
 *
 * (MUST BE LESS THAN MAX_PROCS)
 */
typedef uint32_t process_id_t;

/**
 * A Globally unique identifier for a thread within a process. (Uses all 32 bits)
 */
typedef uint32_t glbl_thread_id_t;

static inline thread_id_t gtid_get_lcl_tid(glbl_thread_id_t gtid) {
    return gtid & 0xFFFF; 
}

static inline process_id_t gtid_get_pid(glbl_thread_id_t gtid) {
    return (gtid & 0xFFFF0000) >> 16;
}

static inline glbl_thread_id_t glbl_thread_id(process_id_t pid, thread_id_t tid) {
    return (pid << 16) | tid;
}

static inline glbl_thread_id_t invalid_gtid(void) {
    return glbl_thread_id(MAX_PROCS, MAX_THREADS_PER_PROC);
}

typedef struct _thread_t {
    /**
     * The global thread id for this thread.
     */
    glbl_thread_id_t gtid;

    /**
     * When a thread is created, it should be given a virtual area where the stack will
     * live. The top of the stack will start as stack_end, and progressively grow down
     * to stack_start.
     *
     * NOTE: The area will be allocated pages on demand. At first only one or two pages of the 
     * entire area may actually exist in the memory space.
     *
     * These values will be used to determin if a page fault should grow the stack or kill the
     * process.
     */
    void * const stack_start;
    const void * const stack_end;

    /**
     * The last value of the stack pointer.
     *
     * Right now, it is assumed that a threads context is pushed onto its stack before a context
     * switch. The stack alone should have all necessary information for switch back into this thread.
     */
    uint32_t *esp;

    /**
     * Whether or not the thread has returned or not.
     *
     * If it has returned, it will be joinable.
     */
    bool returned;

    /**
     * The value given by the thread when it returned.
     */
    void *ret_value;
} thread_t;

/**
 * When a thread is created it must be given an entry point.
 *
 * When the thread first starts execution it will start at this entry point with the associated
 * tid and argument passed in.
 *
 * Threads can also return a value, which will be stored in the kernel until the thread is 
 * reaped.
 */
typedef void *(*thread_entry_t)(thread_id_t tid, void *arg);

typedef uint32_t process_exit_code_t;

#define PROC_EC_SUCCESS     (0)
#define PROC_EC_UNKNOWN_ERR (1)

typedef struct _process_t {
    /**
     * The global id for this process.
     */
    process_id_t pid;

    /**
     * Every process will have its own page directory. (It's own independent memory space)
     *
     * Threads of a process will all operate in this single space to allow for easy communication
     * between threads.
     */
    phys_addr_t pd_addr;

    /**
     * Static number of threads for now!
     */
    thread_t threads[MAX_THREADS_PER_PROC];

    /**
     * True when the process has exited and is now ready to reap.
     */
    bool exited;

    /**
     * An exit code explaining why the process exited.
     */
    process_exit_code_t exit_code;

    /**
     * An arbitrary argument that is supplied either by the OS or User which may given context
     * as to why the process exited.
     */
    uint32_t exit_arg;
} process_t;

/**
 * A system, kinda is the state of the operating system.
 */
typedef struct _kernel_system_t {
    /**
     * The global ID of the currently executing thread.
     */
    glbl_thread_id_t curr_thread;

    process_t procs[MAX_PROCS];
} kernel_system_t;

/**
 * This function will copy a thread's process. 
 *
 * NOTE: Only the given thread will be copied into the new process.
 * All other threads will not be copied. Thus, a process created after forking always starts
 * as single threaded.
 *
 * Saves the global thread id of the spawned thread in child_gtid.
 */
fernos_error_t ks_fork(glbl_thread_id_t gtid, glbl_thread_id_t *child_gtid);

fernos_error_t ks_spawn_thread(process_id_t pid, thread_id_t *tid, );

/**
 * This call is meant to still be usable even if I implement signals later on. This will cleanup
 * a process's resources and remove from the system.
 * What if we want a process to end for a reason??
 */
fernos_error_t ks_end_process(process_id_t pid);


