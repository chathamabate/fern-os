
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "s_util/err.h"
#include "s_bridge/shared_defs.h"

/*
 * In this file, SCID is short for SYSCALL ID.
 */

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
 * Returns FOS_SUCCESS on success.
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
 * Returns an error if there are insufficient resources, or if futex is NULL.
 */
fernos_error_t sc_futex_register(futex_t *futex);

/**
 * Remove all references to a futex from the kernel.
 *
 * If the given futex exists, it's wait queue will be deleted.
 *
 * All threads waiting on the futex will wake up with FOS_STATE_MISMATCH returned.
 */
void sc_futex_deregister(futex_t *futex);

/**
 * This function will check if the futex's value = exp_val from within the kernel.
 *
 * If the values match as expected, the calling thread will be put to sleep.
 * If the values don't match, this call will return immediately with FOS_SUCCESS.
 *
 * When a thread is put to sleep it can only be rescheduled by an `sc_futex_wake` call.
 * This will also return FOS_SUCCESS.
 *
 * This call returns FOS_STATE_MISMATCH if the futex is deregistered mid wait!
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
