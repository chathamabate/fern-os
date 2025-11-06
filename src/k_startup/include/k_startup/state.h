
#pragma once

#include "s_data/id_table.h"
#include "s_data/wait_queue.h"
#include "k_sys/page.h"
#include "s_mem/allocator.h"
#include "k_startup/fwd_defs.h"
#include "s_util/constraints.h"
#include "s_bridge/ctx.h"
#include "s_data/map.h"
#include "s_bridge/app.h"
#include "s_data/ring.h"

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
    if ((err) != FOS_E_SUCCESS) { \
        (thr)->ctx.eax = (err);  \
        return FOS_E_SUCCESS; \
    }


/*
 * Design NOTES:
 *
 * The structures within the kenrel are inherently cyclic. For example a process needs
 * knowledge of threads, and threads need knowledge of processes.
 *
 * Originally, I had some unidirectional design where functions in `thread.c` could never modify
 * anything outside `thread.c`. Functions in `process.c` could modify stuff from `process.c`
 * and `thread.c`. However, trying to make this all consistent was confusing and lead to
 * lots of error prone code.
 *
 * Now, threads and process are given hooks for doing the modifications they need in certain
 * situations. For example, a function in `thread.c` should never access kernel state schedule
 * directly. However, threads now inherit from `ring_element`, which give them the ability 
 * to remove themselves from any "ring/schedule" without knowledge of where said schedule lives.
 * The same concept applies to the thread `wq` field, and a process's handle states!
 *
 * Addition (9/1/2025) NOTE that many of these functions are designed specifically to correspond
 * to system calls. Others may simply be helpers which are used within the system call 
 * implementations. When a system call style function (One that acts on the current thread) returns
 * anything other than FOS_E_SUCCESS (in kernel space), the kernel shuts down.
 * The helper functions need a way of differentiating between an error which can be returned
 * to the calling userspace thread, and an error which should shut down the system.
 * When a helper function encounters some unexpected fatal state, it should return 
 * FOS_E_ABORT_SYSTEM. Then users of the function know when an error is allowed, and when an error
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
     * The schedule!
     */
    ring_t schedule;

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
 * FOS_E_IN_USE is returned if the given slot is occupied.
 * FOS_E_INVALID_INDEX is returned if `plg_id` overshoots the end of the table.
 *
 * otherwise FOS_E_SUCCESS is returned.
 */
fernos_error_t ks_set_plugin(kernel_state_t *ks, plugin_id_t plg_id, plugin_t *plg);

/**
 * This function saves the given context into the current thread.
 *
 * Does nothing if there is no current thread.
 */
void ks_save_ctx(kernel_state_t *ks, user_ctx_t *ctx);

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
 * This "Shuts down" the system.
 *
 * i.e. it calls the on shutdown handler of every plugin, then locks up the machine.
 *
 * NOTE: This will likely be called during certain kernel errors. (Or if the root process exits)
 * If there is some ultra fatal system error, this may not even be called, and the system
 * may immediately lock up.
 *
 * IT DOES NOT RETURN!
 */
void ks_shutdown(kernel_state_t *ks);

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
 * All calls should return FOS_E_SUCCESS here in the kernel unless something is very very wrong.
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
 * On Success FOS_E_SUCCESS is returned in both processes, in the child process,
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
 * FSIG_CHLD is sent to the parent process. If this is the root process, the kernel is shutdown!
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
 * process, FOS_E_STATE_MISMATCH is returned to the user.
 * If `cpid` DOES correspond to a child of this process, but is yet to exit, FOS_E_EMPTY is
 * returned to the user.
 *
 * When attempting to reap any zombie process, if there are no zombie children to reap,
 * FOS_E_EMPTY is returned to the user.
 *
 * When a process is "reaped", its exit status is retrieved, and its resources are freed!
 *
 * If `u_rcpid` is given, the pid of the reaped child is written to *u_rcpid.
 * If `u_rces` is given, the exit status of the reaped child is written to *u_rces.
 *
 * On error, FOS_MAX_PROCS is written to *u_rcpid, and PROC_ES_UNSET is written to *u_rces.
 *
 * NOTE: VERY IMPORTANT: It is intended that the user uses this call in combination with a 
 * wait_signal on FSIG_CHLD. However, based on how the kernel works internally, just because
 * the FSIG_CHLD signal bit is set DOES NOT mean there is anyone to reap.
 * In fact, this call `ks_reap_proc` knows NOTHING of the FSIG_CHLD bit.
 */
KS_SYSCALL fernos_error_t ks_reap_proc(kernel_state_t *ks, proc_id_t cpid, proc_id_t *u_rcpid, 
        proc_exit_status_t *u_rces);

/**
 * Execute a user application in the current process.
 *
 * This call overwrites the calling process by loading a binary dynamically!
 *
 * NOTE: `u_abs_ab` must be a pointer to an ABSOLUTE args block! That is all pointers within
 * the block must assume the block begins at FOS_APP_ARGS_AREA_START.
 * SEE: `args_block_make_absolute`.
 *
 * On success, this call does NOT return to the user process, it enters the given app's main function
 * providing the given args as parameters. 
 * In this case, all child and zombie processes of the calling process are adopted by the root 
 * process. 
 * All non-default handles are deleted.
 * All threads except the main thread are deleted. (The main thread is reset to the entry point
 * of given user app)
 * 
 * A failure to create the new page director for the given application, the given process remains
 * ENTIRELY in tact and an error is returned to userspace.
 *
 * Errors at other points in this function may halt the system.
 */
KS_SYSCALL fernos_error_t ks_exec(kernel_state_t *ks, user_app_t *u_ua, const void *u_abs_ab,
        size_t u_abs_ab_len);

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
 * On success, FOS_E_SUCCESS is returned to the user. 
 * If `u_sid` is given, the recieved signal is written to *u_sid`. 
 * The pending bit of the received signal is cleared.
 *
 * A user error is returned if sv is 0. 
 * On user error, 32 is written to *u_sid.
 */
KS_SYSCALL fernos_error_t ks_wait_signal(kernel_state_t *ks, sig_vector_t sv, sig_id_t *u_sid);


/**
 * Clears all bits which are set in the given signal vector `sv`.
 *
 * This is useful when you can confirm some operation is complete and that no further signals
 * will be received, BUT you may not know the state of the signal vector.
 *
 * For example, when reaping child processes. All processes may be reapped, but there still
 * may be a lingering FSIG_CHLD bit set in the signal vector.
 */
KS_SYSCALL fernos_error_t ks_signal_clear(kernel_state_t *ks, sig_vector_t sv);

/**
 * Request memory in a process's free area.
 *
 * FOS_E_BAD_ARGS if `u_true_e` is NULL.
 * FOS_E_ALIGN_ERROR if `s` or `e` aren't 4K aligned.
 * FOS_E_INVALID_RANGE if `e` < `s` OR if `s` or `e` are outside the free area.
 *
 * In the above three cases, this call does nothing.
 *
 * Otherwise, memory will be attempted to be allocated.
 *
 * FOS_E_SUCCESS is returned if the entire allocation succeeds.
 * If we run out of memory, FOS_E_NO_MEM is returned.
 * If we run into an already page, FOS_E_ALREADY_ALLOCATED is returned.
 * In all of these cases, the end of the last allocated page is written to `*u_true_e`
 */
KS_SYSCALL fernos_error_t ks_request_mem(kernel_state_t *ks, void *s, const void *e, const void **u_true_e);

/**
 * Returns memory in the proccess's free area.
 *
 * Does nothing if `s` or `e` aren't 4K aligned, `e` < `s`, or `s` or `e` are outside
 * the process free area.
 *
 * Returns nothing to the user thread.
 */
KS_SYSCALL fernos_error_t ks_return_mem(kernel_state_t *ks, void *s, const void *e);

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
 * If the current thread is the "main thread" of the root process, FOS_E_ABORT_SYSTEM is returned.
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
 * Set the default input handle of the calling process. If the given input handle is invalid,
 * this will set the defualt Input handle to the NULL_HANDLE.
 *
 * Always returns FOS_E_SUCCESS in both kernel and userspace.
 */
KS_SYSCALL fernos_error_t ks_set_in_handle(kernel_state_t *ks, handle_t in);

/**
 * Read from default input handle.
 *
 * If the default input handle is not currently initialized this returns FOS_E_EMPTY to userspace.
 */
KS_SYSCALL fernos_error_t ks_in_read(kernel_state_t *ks, void *u_dest, size_t len, size_t *u_readden);

/**
 * Wait on default input handle.
 *
 * If the default input handle is not currently initialized this returns FOS_E_EMPTY to userspace.
 */
KS_SYSCALL fernos_error_t ks_in_wait(kernel_state_t *ks);

/**
 * Set the default output handle of the calling process. If the given output handle is invalid,
 * this will set the defualt output handle to the NULL_HANDLE.
 *
 * Always returns FOS_E_SUCCESS in both kernel and userspace.
 */
KS_SYSCALL fernos_error_t ks_set_out_handle(kernel_state_t *ks, handle_t out);

/**
 * Write to default output handle.
 *
 * If the default output handle is not currently initialized this returns acts as if all bytes
 * were successfully written.
 */
KS_SYSCALL fernos_error_t ks_out_write(kernel_state_t *ks, const void *u_src, size_t len, size_t *u_written);


