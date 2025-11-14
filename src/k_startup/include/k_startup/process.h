
#pragma once

#include "k_startup/fwd_defs.h"
#include "k_sys/page.h"
#include "s_mem/allocator.h"
#include "s_data/wait_queue.h"
#include "s_data/map.h"
#include "s_block_device/file_sys.h"
#include "s_bridge/app.h"

#include <stdbool.h>

typedef struct _file_handle_state_t file_handle_state_t;

struct _process_t {
    /**
     * Allocator used to alloc this process.
     */
    allocator_t *al;

    /**
     * If this is true, this is a zombie process, no threads are scheduled.
     *
     * Just waiting to be reaped!
     */
    bool exited;

    /**
     * What exit status the process exited with.
     *
     * We shouldn't need an "exited" boolean field since all exited processes
     * will exist in a zombie child list.
     */
    proc_exit_status_t exit_status;

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
     * children will be adopted by the root process.
     */
    list_t *children;

    /**
     * Children of this process who have exited, but are yet to be reaped.
     *
     * When this process exits, all zombie children will be adopted by the root process.
     */
    list_t *zombie_children;

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
     *
     * This is the vector of all pending signals received by this process.
     */
    sig_vector_t sig_vec;

    /**
     * By default, a signal of any type will kill this process.
     *
     * If you'd like a signal to be passed to the signal wait queue, make sure it is marked 
     * allowed via this mask. `1` means allowed, `0` means kill this process.
     */
    sig_vector_t sig_allow;

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
     * This ID table will always have a maximum capcity <= 256. This way ID's can always fit
     * in 8 bits. (i.e. the size of a handle_t)
     *
     * This maps handle_t's -> handle_state_t *'s.
     */
    id_table_t *handle_table;

    /**
     * The default input handle. 
     *
     * NOTE: This is really just an index into `handle_table`. It is OK if `handle_table` is yet to 
     * have an entry for this index. In such a case, default input does nothing.
     */
    handle_t in_handle;

    /**
     * The default output handle.
     *
     * NOTE: This is really just an index into `handle_table`. It is OK if `handle_table` is yet to 
     * have an entry for this index. In such a case, default output does nothing.
     */
    handle_t out_handle;
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
 * Given a process, create a new forked process.
 *
 * The forked process will have pid `cpid` and parent `proc`.
 *
 * NOTE: The given thread `thr` will be the only thread which is copied into this new process.
 * The new child process will always be single threaded! With the copied thread becoming the new
 * main thread.
 *
 * join queue is NOT copied! And The given process `proc` is not edited in
 * ANY WAY! It is your responsibility to register this new process as a child in the old.
 *
 * Handle states are all copied over.
 *
 * Returns FOS_E_ABORT_SYSTEM if some catastrophic error occurs.
 *
 * Uses the same allocator as the given process.
 */
fernos_error_t new_process_fork(process_t *proc, thread_t *thr, proc_id_t cpid, process_t **out);

/**
 * Process destructor!
 *
 * This frees all memory used by proc! (Including the full page directory)
 *
 * This DOES detach all owned threads from schedules and wait queues.
 * This DOES delete handle states!
 *
 * Any non-success error returned from this function is seen as catastrophic!
 */
fernos_error_t delete_process(process_t *proc);

/**
 * Overwrite a process to begin other different execution.
 *
 * FOS_E_BAD_ARGS if `new_pd` or `entry` are NULL. (Given process is unmodified in this case)
 * FOS_E_STATE_MISMATCH if there is no main thread on the given process!
 *
 * `new_pd` is a page directory which points to a new memory space to be used by this process.
 * It should have NO user thread stacks allocated! Remember, we never explicitly allocate
 * the user stacks, we rely on the PF handler to do that!
 * 
 * `pid` is preserved.
 * `pd` is switched with `new_pd`. (then `pd` is deleted)
 * `parent` is preserved.
 * `children` is preserved.
 * `zombie_children` is preserved.
 *  All threads in the thread table are deleted, except for `main_thread`. (This thread becomes
 *  the first thread of this new execution context)
 * `join_queue` is cleared. (Should already be empty tho)
 * `sig_vec` and `sig_allow` are both set to empty.
 * `signal_queue` is cleared. (Should already be empty tho)
 * All handles states except for `in_handle` and `out_handle` are removed from
 * `handle_table`. (and deleted)
 *
 * FOS_E_ABORT_SYSTEM if there is an error deleting one of the non-default handles.
 * All other of the above actions should always succeed.
 *
 * NOTE: On success, just the `main_thr` will exist. And it will be in a detached state!
 */
fernos_error_t proc_exec(process_t *proc, phys_addr_t new_pd, uintptr_t entry, uint32_t arg0,
        uint32_t arg1, uint32_t arg2);

/**
 * Create a thread within a process with the given entry point and argument!
 * The created thread will start in a detached state.
 *
 * If this process doesn't have a main thread yet, the created thread will be set as main.
 *
 * On Success, the a pointer to the created thread is returned.
 * On error, NULL is returned.
 *
 * NOTE: This uses the same allocator which was used for `proc`.
 */
thread_t *proc_new_thread(process_t *proc, uintptr_t entry, uint32_t arg0, uint32_t arg1,
        uint32_t arg2);

/**
 * Given a process and one of its threads, delete the thread entirely from the process!
 *
 * This DOES detach the thread if necessary before deletion!
 *
 * If you'd like the thread's stack pages to be returned, set `return_stack` to true.
 */
void proc_delete_thread(process_t *proc, thread_t *thr, bool return_stack);
