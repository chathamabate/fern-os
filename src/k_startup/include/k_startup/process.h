
#pragma once

#include "k_startup/fwd_defs.h"
#include "k_sys/page.h"
#include "s_mem/allocator.h"
#include "s_data/wait_queue.h"
#include "s_data/map.h"

struct _process_t {
    /**
     * Allocator used to alloc this process.
     */
    allocator_t *al;

    /**
     * This process's global process id!
     */
    proc_id_t pid;

    /**
     * Physical address of this process's page directory.
     */
    phys_addr_t pd;

    /**
     * A pointer to the parent process, NULL if the root process.
     */
    process_t *parent;

    /**
     * A list<process_t *> which contains all living child processes. If this process exits, these
     * children will be adopted by the root process or by some system daemon.
     */
    //list_t *children;

    /**
     * A children of this process who have exited, but are yet to be reaped.
     *
     * When this process exits, all zombie children will be reaped!
     * (Or maybe given to a kernel structure which reaps when appropriate)
     */
    //list_t *zombie_children;

    /**
     * This is a table of all threads under this process. 
     */
    id_table_t *thread_table;

    /**
     * When a thread quits, its ID will be sent through this queue to find if another
     * thread was waiting. Remember a vector queue only supports 32 different events. So, there can
     * only be a maximum of 32 different threads at any given time.
     *
     * NOTE: 1 thread can wait on multiple threads, however 1 thread cannot be waited
     * on by multiple threads.
     */
    vector_wait_queue_t *join_queue;

    /**
     * all processes have a main thread. When this thread exits, the process exits.
     *
     * (NOTE: There will also be a system call for exiting the process from any thread)
     */
    thread_t *main_thread;


    /**
     * Like in linux, signals will be one form of IPC.
     */
    //uint32_t sig_vec;

    /**
     * By default, a signal of any type will kill this process.
     *
     * If you'd like a signal to be passed to the signal wait queue, make sure it is marked 
     * allowed via this mask. `1` means allowed, `0` means kill this process.
     */
    //uint32_t sig_allow;

    /**
     * When one or more signals arrive, and they're all marked as allow, the signal vector will be
     * passed to this queue. 
     *
     * This queue will deal with waking up the correct threads and editing the signal vector 
     * to reflect which signals were handled.
     *
     * This queue will likely ONLY be notified when new signals arrive. So, before adding a thread 
     * to this queue, you may want to check the signal vector first to see if no waiting is 
     * necessary.
     */
    //vector_wait_queue_t *signal_queue;

    
    /**
     * Originally conditions were stored here, however, upon learning more about synchronization
     * mechanisms, now, futexes are stored here in the process structure.
     *
     * This is a map from userspace addresses (futex_t *)'s -> basic_wait_queue's
     */
    map_t *futexes;
};

/**
 * This allocates a process with basically no information.
 *
 * The main thread will start as NULL.
 *
 * If any allocation fails, NULL is returned.
 */
process_t *new_process(allocator_t *al, proc_id_t pid, phys_addr_t pd, process_t *parent);

static inline process_t *new_da_process(proc_id_t pid, phys_addr_t pd, process_t *parent) {
    return new_process(get_default_allocator(), pid, pd, parent);
}

/**
 * Create a thread within a process with the given entry point and argument!
 * The created thread will start in a detached state.
 *
 * If this process doesn't have a main thread yet, the created thread will be set as main.
 *
 * (Remember, entry and arg should both be userspace pointers!)
 *
 * On Success, FOS_SUCCESS is returned and the created thread is written to *thr.
 * Returns an error if arguments are bad, or if insufficient resources.
 *
 * NOTE: This uses the same allocator which was used for `proc`.
 *
 * On error, NULL is written to *thr.
 */
fernos_error_t proc_create_thread(process_t *proc, thread_t **thr, 
        thread_entry_t entry, void *arg);

/**
 * Given a process and one of its threads, delete the thread entirely from the process!
 *
 * This call does nothing if the given thread is not in an exited state!
 *
 * Also, this call doesn't deal with the join queue at all, that's your responsibility!
 *
 * If you'd like the thread's stack pages to be returned, set `return_stack` to true.
 */
void proc_reap_thread(process_t *proc, thread_t *thr, bool return_stack);

