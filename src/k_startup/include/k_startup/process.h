
#pragma once

#include "k_startup/fwd_defs.h"
#include "s_data/wait_queue.h"
#include "k_sys/page.h"
#include "s_data/id_table.h"
#include "s_data/list.h"

struct _process_t {
    /**
     * This process's global process id!
     */
    proc_id_t pid;

    /**
     * A pointer to the parent process, NULL if the root process.
     */
    process_t *parent;

    /**
     * A list<process_t *> which contains all living child processes. If this process exits, these
     * children will be adopted by the root process or by some system daemon.
     */
    list_t *children;

    /**
     * A children of this process who have exited, but are yet to be reaped.
     *
     * When this process exits, all zombie children will be reaped!
     * (Or maybe given to a kernel structure which reaps when appropriate)
     */
    list_t *zombie_children;

    /**
     * This is a table of all threads under this process. (Maximum of 32 threads)
     */
    id_table_t *thread_table;

    /**
     * When a thread quits, its ID will be sent through this queue to find which other threads
     * were waiting. Remember a vector queue only supports 32 different events. So, there can
     * only be a maximum of 32 different threads at any given time.
     *
     * NOTE: Might change this later.
     */
    vector_wait_queue_t *join_queue;

    /**
     * all processes have a main thread. When this thread exits, the process exits.
     *
     * (NOTE: There will also be a system call for exiting the process from any thread)
     */
    thread_t *main_thread;

    /**
     * Physical address of this process's page directory.
     */
    phys_addr_t pd;

    /**
     * Like in linux, signals will be one form of IPC.
     */
    uint32_t sig_vec;

    /**
     * By default, a signal of any type will kill this process.
     *
     * If you'd like a signal to be passed to the signal wait queue, make sure it is marked 
     * allowed via this mask. `1` means allowed, `0` means kill this process.
     */
    uint32_t sig_allow;

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
    vector_wait_queue_t *signal_queue;

    /**
     * Conditions will be local to each process. A thread from one process can NEVER wait on a 
     * condition from another process.
     *
     * This is essentially a map<Local Condition ID, basic_wait_queue_t *>
     *
     * If a condition is deleted while threads are waiting, all threads should be woken up with 
     * some sort of status code.
     */
    id_table_t *conds;
};
