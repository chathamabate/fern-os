
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "s_util/err.h"

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
    void *(*entry)(void *arg);
    void *arg;
} thread_spawn_arg_t;

/**
 * Spawn a thread with the given entry point and argument!
 *
 * Returns FOS_SUCCESS on success.
 *
 * Returns an error otherwise. (The thread is not spawned in this case)
 */
#define SCID_THREAD_SPAWN (0x102U)
fernos_error_t sc_thread_spawn(void *(*entry)(void *arg), void *arg);

/**
 * Output a string to the BIOS terminal.
 *
 * (Probably will take this out later)
 */
#define SCID_TERM_PUT_S (0x200U)
void sc_term_put_s(const char *s);
