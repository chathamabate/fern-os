
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "s_util/err.h"
#include "s_bridge/shared_defs.h"

/**
 * Trigger a system call from user space.
 *
 * id -> %eax
 * arg0 -> %ecx
 * arg1 -> %edx
 * arg2 -> %esi (restored after call)
 * arg3 -> %edi (restored after call)
 *
 * NOTE: This assumes syscalls are mapped as interrupt 48.
 */
int32_t trigger_syscall(uint32_t id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);


/*
 * In this file, SCID is short for SYSCALL ID.
 */

/**
 * Fork the current process.
 *
 * Only the calling thread is copied over into the child process.
 * Multithreading state is not copied (i.e. Futexes and the join queue)
 * Signal vector is not copied. The created process starts with no received signals.
 * Deep copies of each file handle are made for the child process!
 *
 * On error, an error is returned just in the calling process.
 *
 * On Success, FOS_E_SUCCESS is returned in BOTH processes.
 * In the parent process the child's pid is written to *cpid.
 * In the child process FOS_MAX_PROCS is written to *cpid.
 *
 * The cpid argument is optional.
 * 
 * Returns an error if there are insufficient resources!
 * On error, FOS_MAX_PROCS is written to *cpid.
 */
fernos_error_t sc_proc_fork(proc_id_t *cpid);

/**
 * Exit the current process. (Becoming a zombie process)
 *
 * FSIG_CHLD is sent to the parent process. If this is the root process, the system should exit.
 *
 * All living children of this process are now orphans, they are adopted by the root process.
 * All zombie children of this process are now zombie orphas, also adopted by the root process.
 * All open file handles will remain open until this process is reaped.
 *
 * NOTE: if zombie orphans are added to the root process, the root process will also get a
 * FSIG_CHLD.
 *
 * This function is automatically called when returning from a proceess's main thread.
 */
void sc_proc_exit(proc_exit_status_t status);

/**
 * Reap a zombie child process! 
 *
 * `cpid` is the pid of the process we want to reap. If `cpid` is FOS_MAX_PROCS, this will reap ANY 
 * child process!
 * 
 * When attempting to reap a specific process, if `cpid` doesn't correspond to a child of this 
 * process, FOS_E_STATE_MISMATCH is returned to the user.
 *
 * When attempting to reap any zombie process, if there are no zombie children to reap,
 * FOS_E_EMPTY is returned to the user.
 *
 * When a process is "reaped", its exit status is retrieved, and its resources are freed!
 * All previously opened file handles are closed!
 *
 * If `rcpid` is given, the pid of the reaped child is written to *rcpid.
 * If `rces` is given, the exit status of the reaped child is written to *rces.
 *
 * On error, FOS_MAX_PROCS is written to *rcpid, and PROC_ES_UNSET is written to *rces.
 */
fernos_error_t sc_proc_reap(proc_id_t cpid, proc_id_t *rcpid, proc_exit_status_t *rces);

/**
 * Send a signal to a process with pid `pid`.
 *
 * If `pid` = FOS_MAX_PROCS, the signal is sent to this process's parent.
 *
 * An error is returned if the given signal id is invalid, or if the receiving process
 * cannot be found!
 *
 * You are allowed to signal yourself!
 */
fernos_error_t sc_signal(proc_id_t pid, sig_id_t sid);

/**
 * By default, a process doesn't allow any signals to be received.
 * That is, when a signal of any type is received, the process exits.
 *
 * This can be changed, by setting a process's signal allow vector.
 * Set bits in this vector represent signals which will NOT kill the process when received.
 *
 * NOTE: If there are pending signals which the new vector does not allow, the process will exit!
 *
 * Returns the old value of the signal vector.
 */
sig_vector_t sc_signal_allow(sig_vector_t sv);

/**
 * Sleeps the cureent thread until any of the signals described in sv are receieved.
 * If an apt signal is pending, this call will not sleep the current thread and instead
 * return immediately.
 *
 * On success, FOS_E_SUCCESS is returned. If `sid` is given, the recieved signal is written to
 * `*sid`. The pending bit of the received signal is cleared.
 *
 * An error is returned if sv is 0. (Or something goes wrong inside the kernel)
 * On error, 32 is written to *sid.
 */
fernos_error_t sc_signal_wait(sig_vector_t sv, sig_id_t *sid);

/**
 * Clears all bits which are set in the given signal vector `sv`.
 *
 * This is useful when you can confirm some operation is complete and that no further signals
 * will be received, BUT you may not know the state of the signal vector.
 *
 * For example, when reaping child processes. All processes may be reapped, but there still
 * may be a lingering FSIG_CHLD bit set in the signal vector.
 */
void sc_signal_clear(sig_vector_t sv);

/**
 * Exit the current thread.
 *
 * If this thread is the "main thread", the full process will exit.
 * This function is called automatically when returning from a thread's
 * entry procedure.
 *
 * If this thread is the "main thread" of the "root process", the entire OS should abort.
 *
 * This call does not return in the current context.
 */
void sc_thread_exit(void *retval);

/**
 * Have the current thread sleep for at least `ticks` timer interrupts.
 *
 * Remember, this only sleeps the calling thread. Other threads in the parent process will be 
 * left untouched.
 */
void sc_thread_sleep(uint32_t ticks);

/**
 * Spawn a thread with the given entry point and argument!
 *
 * Returns FOS_E_SUCCESS on success.
 *
 * Returns an error otherwise. (The thread is not spawned in this case)
 *
 * If tid is given, the created thread's id is written to tid.
 * On error, the null tid is written, (FOS_MAX_THREADS_PER_PROC)
 */
fernos_error_t sc_thread_spawn(thread_id_t *tid, void *(*entry)(void *arg), void *arg);

/**
 * This deschedules the current thread until one of threads entered in the join vector has exited.
 *
 * This should return an error immediately if the jv is invalid.
 * (jv is invalid if it is 0 or depends only on the current thread)
 *
 * NOTE: No two threads should join on the same thread, however, this is not checked, so be
 * careful! (If two wait on the same thread, only one will ever be woken up!)
 *
 * If joined is given, on error, the null id will be written, on success, the id of the joined
 * thread will be written.
 *
 * If ret_val is given, on error, null will be written, on success, the value returned by
 * the joined thread will be written.
 *
 * Any error scenario always returns immediately to the calling thread.
 */
fernos_error_t sc_thread_join(join_vector_t jv, thread_id_t *joined, void **retval);

/**
 * Register a futex with the kernel.
 *
 * A futex is a number, which is mapped to a wait queue in the kernel.
 *
 * Threads will be able to wait while the futex holds a specific value.
 *
 * Returns an error if there are insufficient resources, if futex is NULL,
 * or if the futex is already in use!
 */
fernos_error_t sc_futex_register(futex_t *futex);

/**
 * Remove all references to a futex from the kernel.
 *
 * If the given futex exists, it's wait queue will be deleted.
 *
 * All threads waiting on the futex will wake up with FOS_E_STATE_MISMATCH returned.
 */
void sc_futex_deregister(futex_t *futex);

/**
 * This function will check if the futex's value = exp_val from within the kernel.
 *
 * If the values match as expected, the calling thread will be put to sleep.
 * If the values don't match, this call will return immediately with FOS_E_SUCCESS.
 *
 * When a thread is put to sleep it can only be rescheduled by an `sc_futex_wake` call.
 * This will also return FOS_E_SUCCESS.
 *
 * This call returns FOS_E_STATE_MISMATCH if the futex is deregistered mid wait!
 *
 * This call return other errors if something goes wrong or if the given futex doesn't exist!
 */
fernos_error_t sc_futex_wait(futex_t *futex, futex_t exp_val);

/**
 * Wake up one or all threads waiting on a futex. This does not modify the futex value at all.
 *
 * Returns an error if the given futex doesn't exist.
 */
fernos_error_t sc_futex_wake(futex_t *futex, bool all);

/**
 * Output a string to the BIOS terminal.
 *
 * (Probably will take this out later)
 */
void sc_term_put_s(const char *s);
void sc_term_put_fmt_s(const char *fmt, ...);

/*
 * Now for handle and plugin system calls!
 *
 * Remember, the behavior of these calls depends on what type of handle is opened and
 * what type of plugin is being referenced!
 */

/**
 * Execute a handle command.
 */
fernos_error_t sc_handle_cmd(handle_t h, handle_cmd_id_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);

/**
 * Clean up the resources corresponding to a given handle.
 */
void sc_handle_close(handle_t h);

/**
 * Write data to a handle, `written` is optional.
 *
 * On success, FOS_E_SUCCESS is returned, meaning some or all of the given data was written.
 * The exact amount written will be stored in `*written`.
 */
fernos_error_t sc_handle_write(handle_t h, const void *src, size_t len, size_t *written);

/**
 * Read data from a handle. `readden` is optional.
 *
 * Returns FOS_E_EMPTY if there is no data to read.
 * Returns FOS_E_SUCCESS if some or all of the requested data is read. The exact amount is 
 * written to `*readden`.
 *
 * (Other errors may be returned)
 */
fernos_error_t sc_handle_read(handle_t h, void *dest, size_t len, size_t *readden);

/**
 * Block until there is data to read from `h`.
 *
 * Returns FOS_E_SUCCESS when there is data to read!
 * Returns FOS_E_EMPTY when there will never be any more data to read from `h`.
 *
 * (Other errors may be returned)
 */
fernos_error_t sc_handle_wait(handle_t h);

/**
 * Execute some plugin specific command.
 */
fernos_error_t sc_plg_cmd(plugin_id_t plg_id, plugin_cmd_id_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);

/*
 * Helpers
 */

/**
 * When using a handle which may write a partial amount on success,
 * this function will call `sc_write` in a loop until all bytes are written!
 * (Or an error is encountered)
 */
fernos_error_t sc_handle_write_full(handle_t h, const void *src, size_t len);

/**
 * When using a handle which may read a partial amount on success,
 * this function will call `sc_read` in a loop until all bytes are written!
 * (Or an error is encountered)
 *
 * NOTE: like `sc_handle_read`, this will return FOS_E_EMPTY if there is no more data to
 * read.
 */
fernos_error_t sc_handle_read_full(handle_t h, void *dest, size_t len);


