#pragma once

#include <stdint.h>
#include "s_bridge/shared_defs.h"

/**
 * When a new thread is created, this entry routine is what is first run.
 * It wraps whatever function the thread is meant to run.
 *
 * NOTE: 10/22/25 - I am making thread entry more generic. That is, the thread entry point
 * given in %eax, can really have any function type. 3 arguments will be pushed onto the stack
 * before the given entry is called. What these args are can be decided depending on the 
 * type of thread being created. (For example, the first user process thread? A new thread in
 * an existing process? Or maybe the new main thread when executing a user app?)
 *
 * It expects arguments to be passed via registers.
 *
 * %eax - const void *entry - The function to call.
 * %ebx - uint32_t arg0
 * %ecx - uint32_t arg1
 * %edx - uint32_t arg2
 *
 * The given function pointer given should return a 32-bit value.
 * (But it can also return nothing)
 */
void thread_entry_routine(void);

/**
 * Entry point for the first user process.
 */
proc_exit_status_t user_main(void);

