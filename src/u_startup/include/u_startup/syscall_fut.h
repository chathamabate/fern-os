
#pragma once

#include "s_bridge/shared_defs.h"

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
fernos_error_t sc_fut_register(futex_t *futex);

/**
 * Remove all references to a futex from the kernel.
 *
 * If the given futex exists, it's wait queue will be deleted.
 *
 * All threads waiting on the futex will wake up with FOS_E_STATE_MISMATCH returned.
 */
void sc_fut_deregister(futex_t *futex);

/**
 * This function will check if the futex's value = exp_val from within the kernel.
 *
 * If the values match as expected, the calling thread will be put to sleep.
 * If the values don't match, this call will return immediately with FOS_E_SUCCESS.
 *
 * When a thread is put to sleep it can only be rescheduled by an `sc_fut_wake` call.
 * This will also return FOS_E_SUCCESS.
 *
 * This call returns FOS_E_STATE_MISMATCH if the futex is deregistered mid wait!
 *
 * This call return other errors if something goes wrong or if the given futex doesn't exist!
 */
fernos_error_t sc_fut_wait(futex_t *futex, futex_t exp_val);

/**
 * Wake up one or all threads waiting on a futex. This does not modify the futex value at all.
 *
 * Returns an error if the given futex doesn't exist.
 */
fernos_error_t sc_fut_wake(futex_t *futex, bool all);

