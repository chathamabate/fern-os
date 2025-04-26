#pragma once

#include <stdint.h>

/**
 * When a new thread is created, this entry routine is what is first run.
 * It wraps whatever function the thread is meant to run.
 *
 * It expects arguments to be passed via registers.
 *
 * %eax - thread_entry_t entry - The function to call.
 * %ecx - void *arg - The argument to pass into the function.
 */
void thread_entry_routine(void);

/**
 * Entry point for the first user process.
 */
void *user_main(void *arg);

