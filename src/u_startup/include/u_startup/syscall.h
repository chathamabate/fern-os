
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "s_util/err.h"
#include "s_bridge/shared_defs.h"
#include "s_bridge/app.h"
#include "s_mem/allocator.h"

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
 * Signal vector is not copied. The created process starts with no received or allowed signals!
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
 * All open handles will remain open until this process is reaped.
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
 * If `cpid` DOES correspond to a child of this process, but is yet to exit, FOS_E_EMPTY is
 * returned to the user.
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
 *
 * NOTE: VERY IMPORTANT: It is intended that the user uses this call in combination with a 
 * wait_signal on FSIG_CHLD. However, based on how the kernel works internally, just because
 * the FSIG_CHLD signal bit is set DOES NOT mean there is anyone to reap.
 * In fact, this call `ks_reap_proc` knows NOTHING of the FSIG_CHLD bit.
 */
fernos_error_t sc_proc_reap(proc_id_t cpid, proc_id_t *rcpid, proc_exit_status_t *rces);

/**
 * Execute a user application. 
 *
 * This call overwrites the calling process by loading a binary dynamically!
 * 
 * On success, this call does NOT return, it enters the given app's main function
 * providing the given args as parameters.
 * In this case, all child/zombie processes are adopted by the root process.
 * Signal vectors are clears.
 *
 * The only thing really preserved are the default IO handles!
 *
 * NOTE: When this new application is entered, it uses a new memory space which has NO HEAP setup.
 *
 * NOTE: `args_block` is expected to be absolute from FOS_APP_ARGS_AREA_START.
 *
 * On failure, an error is returned, the calling process remains in its original state.
 */
fernos_error_t sc_proc_exec(user_app_t *ua, const void *args_block, size_t args_block_size);

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
 *
 * NOTE: 11/6/25: Unsure if the example above really holds water. 
 */
void sc_signal_clear(sig_vector_t sv);

/**
 * Request memory in this process's free area.
 *
 * FOS_E_BAD_ARGS if `true_e` is NULL.
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
 * In all of these cases, the end of the last allocated page is written to `*true_e`
 */
fernos_error_t sc_mem_request(void *s, const void *e, const void **true_e);

/**
 * Returns memory in this proccess's free area.
 *
 * Does nothing if `s` or `e` aren't 4K aligned, `e` < `s`, or `s` or `e` are outside
 * the process free area.
 */
void sc_mem_return(void *s, const void *e);

/**
 * This is a memory managenment pair which holds the above two
 * system calls. When creating a heap allocator in userspace, you'll likely
 * want to use this pair.
 */
extern const mem_manage_pair_t USER_MMP;

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

/*
 * Now for handle and plugin system calls!
 *
 * Remember, the behavior of these calls depends on what type of handle is opened and
 * what type of plugin is being referenced!
 */

/**
 * Set the default input handle of the calling process. If the given input handle is invalid,
 * this will set the defualt Input handle to the NULL_HANDLE.
 */
void sc_set_in_handle(handle_t in);

/**
 * Read from default input handle.
 *
 * If the default input handle is not currently initialized this returns FOS_E_EMPTY.
 */
fernos_error_t sc_in_read(void *dest, size_t len, size_t *readden);

/**
 * Wait on default input handle.
 * 
 * If the default input handle is not currently initialized this returns FOS_E_EMPTY.
 */
fernos_error_t sc_in_wait(void);

/**
 * Set the default output handle of the calling process. If the given output handle is invalid,
 * this will set the defualt output handle to the NULL_HANDLE.
 */
void sc_set_out_handle(handle_t out);

/**
 * Write to default output handle.
 *
 * If the default output handle is not currently initialized this behaves as if all bytes were
 * successfully written.
 */
fernos_error_t sc_out_write(const void *src, size_t len, size_t *written);

/**
 * Execute a handle specific command.
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
 *
 * Returns FOS_E_BAD_ARGS if `len` = 0.
 */
fernos_error_t sc_handle_write(handle_t h, const void *src, size_t len, size_t *written);

/**
 * Read data from a handle. `readden` is optional.
 *
 * Returns FOS_E_EMPTY if there is no data to read.
 * Returns FOS_E_BAD_ARGS if `len` = 0.
 * Returns FOS_E_SUCCESS if some or all of the requested data is read. The exact amount is 
 * written to `*readden`. This should NOT block.
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
 * Same as `sc_handle_write_full`, but with the default out handle.
 */
fernos_error_t sc_out_write_full(const void *src, size_t len);

/**
 * This won't return until the full given string is written (EXCLUDING THE NT)
 * or an error is encountered.
 */
fernos_error_t sc_handle_write_s(handle_t h, const char *str);

/**
 * Same as `sc_handle_write_s`, but with the default out handle.
 */
fernos_error_t sc_out_write_s(const char *str);

/**
 * Just like `sc_handle_write_s`, but a fmt string is accepted with varargs.
 *
 * Formatting is run on a buffer with size `SC_HANDLE_WRITE_FMT_S_BUFFER_SIZE`.
 * Undefined behavior if the resulting string overflows this buffer!
 */
#define SC_HANDLE_WRITE_FMT_S_BUFFER_SIZE (1024U)
fernos_error_t sc_handle_write_fmt_s(handle_t h, const char *fmt, ...);

/**
 * Same as `sc_handle_write_fmt_s`, but with the default out handle.
 */
fernos_error_t sc_out_write_fmt_s(const char *fmt, ...);

/**
 * Same as `sc_out_write_fmt_s` but with a void return type.
 *
 * This is useful for data structur dump debug functions which require a logging
 * function with a void return type.
 */
void sc_out_write_fmt_s_lf(const char *fmt, ...);

/**
 * When using a handle which may read a partial amount on success,
 * this function will call `sc_read` in a loop until all bytes are written!
 * (Or an error is encountered)
 *
 * NOTE: like `sc_handle_read`, this will return FOS_E_EMPTY if there is no more data to
 * read.
 */
fernos_error_t sc_handle_read_full(handle_t h, void *dest, size_t len);

/**
 * Same as `sc_handle_read_full`, but with the default in handle.
 */
fernos_error_t sc_in_read_full(void *dest, size_t len);


