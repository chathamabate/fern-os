
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "s_util/err.h"
#include "s_bridge/shared_defs.h"

/*
 * In this file, SCID is short for SYSCALL ID.
 */

/**
 * A generic argument useable for any system call will takes in a buffer.
 *
 * Remember complex arguments must always be copied from userspace to kernel space
 * before being used!
 */
typedef struct _buffer_arg_t {
    size_t buf_size;
    void *buf;
} buffer_arg_t;

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
#define SCID_THREAD_EXIT (0x100U)
void sc_thread_exit(void *retval);

/**
 * Have the current thread sleep for at least `ticks` timer interrupts.
 *
 * Remember, this only sleeps the calling thread. Other threads in the parent process will be 
 * left untouched.
 */
#define SCID_THREAD_SLEEP (0x101U)
void sc_thread_sleep(uint32_t ticks);

typedef struct _thread_spawn_arg_t {
    thread_id_t *tid;

    void *(*entry)(void *arg);
    void *arg;
} thread_spawn_arg_t;

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
#define SCID_THREAD_SPAWN (0x102U)
fernos_error_t sc_thread_spawn(thread_id_t *tid, void *(*entry)(void *arg), void *arg);

typedef struct _thread_join_arg_t {
    join_vector_t jv;

    /**
     * When the joining thread is woken up, this pointer will be used to pass information beck
     * to userspace before being rescheduled.
     */
    thread_join_ret_t *join_ret;
} thread_join_arg_t;

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
#define SCID_THREAD_JOIN (0x103U)
fernos_error_t sc_thread_join(join_vector_t jv, thread_id_t *joined, void **retval);


/**
 * Create a new process unique condition.
 *
 * Returns error if insufficient resources or if cid is NULL.
 */
#define SCID_COND_NEW    (0x200U)
fernos_error_t sc_cond_new(cond_id_t *cid);

/**
 * Delete a condition.
 *
 * All threads previously waiting on this condition will be rescheduled with return code
 * FOS_STATE_MISMATCH.
 *
 * Returns error if cid is invalid.
 */
#define SCID_COND_DELETE (0x201U)
fernos_error_t sc_cond_delete(cond_id_t cid);

typedef struct _cond_notify_arg_t {
    cond_id_t cid;
    cond_notify_action_t action;
} cond_notify_arg_t;

/**
 * Notify any or all waiting threads on a specific condition.
 *
 * By "any" I just mean one arbitrary thread.
 *
 * When a thread is notified it is added back to the schedule. Where it is added into the schedule
 * is not gauranteed, so keep this in mind!
 *
 * Returns and error if cid or action are invalid.
 */
#define SCID_COND_NOTIFY (0x202U)
fernos_error_t sc_cond_notify(cond_id_t cid, cond_notify_action_t action);

/**
 * Wait to be notified.
 *
 * When this thread is successfully notified, FOS_SUCCESS is returned from this function.
 * FOS_STATE_MISMATCH is returned if the condition is deleted during a waiting period.
 *
 * Also returns an error if cid is invalid.
 */
#define SCID_COND_WAIT   (0x203U)
fernos_error_t sc_cond_wait(cond_id_t cid);

/**
 * Output a string to the BIOS terminal.
 *
 * (Probably will take this out later)
 */
#define SCID_TERM_PUT_S (0x400U)
void sc_term_put_s(const char *s);
