
#pragma once

#include "s_data/id_table.h"
#include "s_data/wait_queue.h"
#include "k_sys/page.h"
#include "s_mem/allocator.h"
#include "k_startup/fwd_defs.h"
#include "s_util/constraints.h"
#include "s_bridge/ctx.h"
#include "s_data/map.h"

#include "s_block_device/file_sys.h"

/*
 * Some helper macros for returning from these calls in both the kernel space and the user thread.
 */

#define DUAL_RET(thr, u_err, k_err) \
    do { \
        (thr)->ctx.eax = u_err; \
        return (k_err); \
    } while (0)

#define DUAL_RET_COND(cond, thr, u_err, k_err) \
    if (cond) { \
        DUAL_RET(thr, u_err, k_err); \
    }

#define DUAL_RET_FOS_ERR(err, thr) \
    if ((err) != FOS_SUCCESS) { \
        (thr)->ctx.eax = (err);  \
        return FOS_SUCCESS; \
    }


/*
 * Design NOTES:
 *
 * The structures within the kenrel are inherently cyclic. For example a process needs
 * knowledge of threads, and threads need knowledge of processes.
 *
 * However, I still tried to implement behaviors in a non-cyclic way.
 * 
 * Functions written in thread.c should only ever modify fields within a thread structure.
 * Even though a thread has a wait queue pointer, no function in thread.c should modify the
 * external wait queue.
 *
 * Similarly, functions in process.c can modify the state of owned threads, but never modify 
 * any higher up state. For example, the schedule.
 *
 * Kernel State -- Can Modify -- > Processes -- Can Modify -- > Threads
 *
 * NOT THE OTHER DIRECTION!
 *
 * Addition (9/1/2025) NOTE that many of these functions are designed specifically to correspond
 * to system calls. Others may simply be helpers which are used within the system call 
 * implementations. When a system call style function (One that acts on the current thread) returns
 * anything other than FOS_SUCCESS (in kernel space), the kernel shuts down.
 * The helper functions need a way of differentiating between an error which can be returned
 * to the calling userspace thread, and an error which should shut down the system.
 * When a helper function encounters some unexpected fatal state, it should return 
 * FOS_ABORT_SYSTEM. Then users of the function know when an error is allowed, and when an error
 * is fatal.
 */

/**
 * Kernels state endpoints which directly correspond to a syscall will have this
 * prefix here in the header files.
 *
 * It doesn't do anything. It just let's you know that said call will assume there is 
 * a current thread, and that thread is the caller.
 */
#define KS_SYSCALL

struct _kernel_state_t {
    /**
     * Allocator to be used by the kernel!
     */
    allocator_t * const al;

    /**
     * The currently executing thread.
     *
     * NOTE: The schedule forms a cyclic doubly linked list.
     *
     * During a context switch curr_thread is set to curr_thread->next.
     */
    thread_t *curr_thread;

    /**
     * Every process will have a globally unique ID!
     */
    id_table_t * const proc_table;

    /**
     * When the root proecess exits, the whole kernel should exit!
     */
    process_t *root_proc;

    /**
     * This counter is initialized as 0 and incremented every time the
     * timer interrupt handler is entered.
     */
    uint32_t curr_tick;

    /**
     * Threads that call the sleep system call will be placed in this queue.
     */
    timed_wait_queue_t * const sleep_q;

    /**
     * For now, plugins are meant to be registered before kicking of the user processes.
     *
     * The number cell occupied by the plugin pointer will be the ID used from userspace to
     * invoke the plugins custom commands.
     *
     * Unused cells will have value NULL. When a plugin errors out and must be deleted, it's cell
     * is also set to NULL after cleanup.
     */
    plugin_t *plugins[FOS_MAX_PLUGINS];
};

/**
 * Create a kernel state!
 *
 * Returns NULL on error.
 */
kernel_state_t *new_kernel_state(allocator_t *al);

static inline kernel_state_t *new_da_kernel_state(void) {
    return new_kernel_state(get_default_allocator());
}

/**
 * Place the given plugin pointer in the plugins table at slot `plg_id`.
 *
 * FOS_IN_USE is returned if the given slot is occupied.
 * FOS_INVALID_INDEX is returned if `plg_id` overshoots the end of the table.
 *
 * otherwise FOS_SUCCESS is returned.
 */
fernos_error_t ks_set_plugin(kernel_state_t *ks, plugin_id_t plg_id, plugin_t *plg);

/**
 * This function saves the given context into the current thread.
 *
 * Does nothing if there is no current thread.
 */
void ks_save_ctx(kernel_state_t *ks, user_ctx_t *ctx);

/**
 * Takes a thread and adds it to the schedule!
 *
 * We expect the given thread to be in a detatched state.
 * (However, this is not checked, so be careful!)
 *
 * Undefined behavoir will occur if the same thread appears twice
 * in the schedule! So make sure this never happens!
 *
 * NOTE: this only modifies the current thread field iff there is no current thread!
 */
void ks_schedule_thread(kernel_state_t *ks, thread_t *thr);

/**
 * Remove a scheduled thread from the schedule!
 *
 * Like above, we don't actually check if the given thread is scheduled
 * or not, we just assume it is! (SO BE CAREFUL)
 */
void ks_deschedule_thread(kernel_state_t *ks, thread_t *thr);

/**
 * Attempts to expand the stack of the current thread.
 *
 * new_base must be 4K aligned!
 *
 * If the new base is not in the stack range of the current thread, an error is returned.
 * If there are insufficient resources, an error is returned.
 * If this area is already allocated on the thread stack, this call still succeeds.
 */
fernos_error_t ks_expand_stack(kernel_state_t *ks, void *new_base);

/**
 * This function advances the kernel's tick counter.
 *
 * (This also updates the sleep queue and schedule automatically)
 *
 * Returns an error if there is some issue dealing with the sleep queue.
 */
fernos_error_t ks_tick(kernel_state_t *ks);

/*
 * Many of the below functions assume that the current thread is the one invoking the requested
 * behavior. Thus erors will be returned here in kernel space, and also to the calling thread
 * via the %eax register.
 *
 * All calls should return FOS_SUCCESS here in the kernel unless something is very very wrong.
 * Errors for bad arguments and lack of resources should be forwarded to the user via the
 * current threads %eax register.
 */

/**
 * Forks the process of the current thread.
 *
 * Only the current thread will be copied into the new process.
 * Almost all process state will not be copied (i.e. Futexes and the Join Queue)
 *
 * The forked process will have its only thread scheduled, and will be set as a child
 * of the current process.
 *
 * On Success FOS_SUCCESS is returned in both processes, in the child process,
 * FOS_MAX_PROCS is written to *u_cpid, in the parent process the new child's pid is written to
 * *u_cpid.
 *
 * Returns an error in the calling process if insufficient resources or some other misc error.
 *
 * NOTE: u_cpid is optional and a userspace pointer.
 */
KS_SYSCALL fernos_error_t ks_fork_proc(kernel_state_t *ks, proc_id_t *u_cpid);

/** 
 * Exit the current process. (Becoming a zombie process)
 *
 * FSIG_CHLD is sent to the parent process. If this is the root process, FOS_ABORT_SYSTEM is 
 * returned.
 *
 * All living children of this process are now orphans, they are adopted by the root process.
 * All zombie children of this process are now zombie orphas, also adopted by the root process.
 *
 * NOTE: if zombie orphans are added to the root process, the root process will also get a
 * FSIG_CHLD.
 *
 * This function is automatically called when returning from a proceess's main thread.
 */
KS_SYSCALL fernos_error_t ks_exit_proc(kernel_state_t *ks, proc_exit_status_t status);

/**
 * Reap a zombie child process! 
 *
 * `cpid` is the pid of the process we want to reap. If `cpid` is FOS_MAX_PROCS, this will reap ANY 
 * child process!
 * 
 * When attempting to reap a specific process, if `cpid` doesn't correspond to a child of this 
 * process, FOS_STATE_MISMATCH is returned to the user.
 *
 * When attempting to reap any zombie process, if there are no zombie children to reap,
 * FOS_EMPTY is returned to the user.
 *
 * When a process is "reaped", its exit status is retrieved, and its resources are freed!
 *
 * If `u_rcpid` is given, the pid of the reaped child is written to *u_rcpid.
 * If `u_rces` is given, the exit status of the reaped child is written to *u_rces.
 *
 * On error, FOS_MAX_PROCS is written to *u_rcpid, and PROC_ES_UNSET is written to *u_rces.
 */
KS_SYSCALL fernos_error_t ks_reap_proc(kernel_state_t *ks, proc_id_t cpid, proc_id_t *u_rcpid, 
        proc_exit_status_t *u_rces);

/** 
 * Send a signal to a process with pid `pid`.
 *
 * If `pid` = FOS_MAX_PROCS, the signal is sent to the current process's parent.
 *
 * An error is returned if the given signal id is invalid, or if the receiving process
 * cannot be found! 
 *
 * You are allowed to send a signal to yourself!
 *
 * By "sending a signal" this set the given signal's bit in the current process's signal vector.
 * If the signal is not allowed, the current process is forcefully exited.
 * If the signal is allowed, threads waiting on said signal will be woken up.
 *
 * (NOTE: that if the signal bit is already set, this call is essentially a nop)
 */
KS_SYSCALL fernos_error_t ks_signal(kernel_state_t *ks, proc_id_t pid, sig_id_t sid);

/** 
 * Set the current process's signal allow vector. Returns old signal vector value in the 
 * current thread.
 *
 * NOTE: If there are pending signals which the new vector does not allow, the process will exit!
 */
KS_SYSCALL fernos_error_t ks_allow_signal(kernel_state_t *ks, sig_vector_t sv);

/**
 * Sleeps the cureent thread until any of the signals described in sv are receieved.
 * If an apt signal is pending, this call will not sleep the current thread.
 *
 * On success, FOS_SUCCESS is returned to the user. 
 * If `u_sid` is given, the recieved signal is written to *u_sid`. 
 * The pending bit of the received signal is cleared.
 *
 * A user error is returned if sv is 0. 
 * On user error, 32 is written to *u_sid.
 */
KS_SYSCALL fernos_error_t ks_wait_signal(kernel_state_t *ks, sig_vector_t sv, sig_id_t *u_sid);

/**
 * Take the current thread, deschedule it, and add it it to the sleep wait queue.
 *
 * Kernel error if there is no current thread.
 *
 * User error if there are insufficient resources. In this case the thread will 
 * remain scheduled.
 */
KS_SYSCALL fernos_error_t ks_sleep_thread(kernel_state_t *ks, uint32_t ticks);

/**
 * Spawn new thread in the current process using entry and arg.
 *
 * u_tid should either be NULL or a pointer into user space.
 *
 * The created thread is added to the schedule!
 *
 * Kernel error if there is no current thread!
 *
 * User error if entry is NULL, or if there are insufficient resources to spawn thre thread.
 */
KS_SYSCALL fernos_error_t ks_spawn_local_thread(kernel_state_t *ks, thread_id_t *u_tid, 
        thread_entry_t entry, void *arg);

/**
 * Exits the current thread of the current process.
 *
 * This detatches the current thread from the schedule.
 *
 * When a non-main thread is exited, It's id is sifted through its parent's join_queue to see 
 * if any thread was waiting on its exit. If a thread was waiting, it will be removed from 
 * the join queue and scheduled!
 *
 * If the current thread is the "main thread" of a process, the entire process should exit.
 * If the current thread is the "main thread" of the root process, FOS_ABORT_SYSTEM is returned.
 *
 * Kernel error if there is no current thread, or if something goes wrong with the exit.
 */
KS_SYSCALL fernos_error_t ks_exit_thread(kernel_state_t *ks, void *ret_val);

/**
 * Have the current thread join on a given join vector.
 *
 * "join'ing" means to not be scheduled again until one of the vectors outlined in jv has exited.
 * 1 thread can wait on multiple threads, but multiple threads should never wait on the same thread!
 *
 * If one of the threads listed in the join vector has already exited, but is yet to be joined,
 * the current thread will remain scheduled!
 * 
 * user error if the join vector is invalid.
 *
 * NOTE: VERY VERY IMPORTANT!
 * Remeber, u_join_ret is a userspace pointer. It is intended to be stored in the thread wait
 * context field until wake up!
 *
 * This is kinda hacky, but just remember, the userspace verion of this call essentially blocks.
 * This call here though in kernel space doesn't!
 */
KS_SYSCALL fernos_error_t ks_join_local_thread(kernel_state_t *ks, join_vector_t jv, 
        thread_join_ret_t *u_join_ret);

/**
 * Regsiter a futex in the current process.
 *
 * user error if futex is null or already in use, Or if there are insufficient resources.
 *
 * Remember, u_futex is a pointer into userspace!
 */
KS_SYSCALL fernos_error_t ks_register_futex(kernel_state_t *ks, futex_t *u_futex);

/**
 * Deregister a futex of the current process.
 *
 * Doesn't return a user error.
 *
 * If threads are currently waiting on this futex, they are rescheduled with return value
 * FOS_STATE_MISMATCH.
 *
 * Returns an error if there is some issue with the deregister.
 */
KS_SYSCALL fernos_error_t ks_deregister_futex(kernel_state_t *ks, futex_t *u_futex);

/**
 * Confirm the given value of the futex = exp_val. If not, returns user success immediately.
 * If the value is equal, the current thread is descheduled.
 *
 * The descheduled thread will only be woken up with a call to wake, Or if the futex is destroyed.
 * If the futex is destroyed, all waiting threads are rescheduled with user return value of
 * FOS_STATE_MISMATCH.
 *
 * Returns user error if arguements are invalid.
 */
KS_SYSCALL fernos_error_t ks_wait_futex(kernel_state_t *ks, futex_t *u_futex, futex_t exp_val);

/**
 * Wake up one or all threads waiting on a futex.
 *
 * Returns user error if arguements are invalid.
 */
KS_SYSCALL fernos_error_t ks_wake_futex(kernel_state_t *ks, futex_t *u_futex, bool all);
