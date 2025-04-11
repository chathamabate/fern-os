
#pragma once

#include "k_sys/page.h"
#include <stdint.h>
#include <stdbool.h>

#define MAX_THREADS_PER_TASK 0x10U

/**
 * The id of a thread within a process. This is unique across all threads within a process, but not
 * all threads globally. (Use only first 16 bits)
 */
typedef uint32_t thread_id_t;

/**
 * ID of a process. Unique across all processs in the operating system. (Use only first 16 bits)
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


typedef struct _thread_t {
    /**
     * A Thread ID is unique per process.
     *
     * No two threads in a single process can have the same ID.
     * However, threads in different processs can have the same ID.
     */
    uint32_t thread_id;

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
     */
    uint32_t *esp;

    /**
     * At some point, we may have a notion of a thread waiting/sleeping until something occurs.
     * That would be nice!
     */
} thread_t;


typedef struct _process_t {
    /**
     * Every process will have its own page directory. (It's own independent memory space)
     *
     * Threads of a process will all operate in this single space to allow for easy communication
     * between threads.
     */
    phys_addr_t pd_addr;

    thread_t threads[MAX_THREADS_PER_TASK];
} process_t;

