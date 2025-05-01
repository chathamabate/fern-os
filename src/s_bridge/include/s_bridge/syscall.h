
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
    thread_id_t *joined;
    void **retval;
} thread_join_arg_t;

/**
 * This deschedules the current thread until one of threads entered in the join vector has exited.
 *
 * This should return an error immediately if the jv is invalid.
 * (jv is invalid if it is 0 or depends only on the current thread)
 *
 * If joined is given, on error, the null id will be written, on success, the id of the joined
 * thread will be written.
 *
 * If re_val is given, on error, null will be written, on success, the value returned by
 * the joined thread will be written.
 *
 * Any error scenario always returns immediately to the calling thread.
 */
#define SCID_THREAD_JOIN (0x103U)
fernos_error_t sc_thread_join(join_vector_t jv, thread_id_t *joined, void **retval);

/**
 * Output a string to the BIOS terminal.
 *
 * (Probably will take this out later)
 */
#define SCID_TERM_PUT_S (0x200U)
void sc_term_put_s(const char *s);
