
#pragma once

#include "s_bridge/shared_defs.h"
#include "s_util/err.h"


typedef futex_t mutex_t;

/* The three states of a mutex */

/**
 * The mutex is unlocked, and NO threads are waiting.
 */
#define MUT_UNLOCKED             (0)

/**
 * The mutex IS locked, and NO threads are waiting.
 */
#define MUT_LOCKED_UNCONTENDED   (1)

/**
 * The mutex IS locked, and one or more threads MAY be waiting.
 */
#define MUT_LOCKED_CONTENDED     (2)

/*
 * NOTE: This mutex implementation IS NOT FAIR!
 */

/**
 * Initialize a mutex.
 *
 * (This registers it with the kernel, and sets its value to MUT_UNLOCKED)
 *
 * An error will be returned if there are not sufficient resources, or if the given pointer is 
 * invalid.
 */
fernos_error_t init_mutex(mutex_t *mut);

/**
 * Deregister the mutex with the kernel.
 */
void cleanup_mutex(mutex_t *mut);

/**
 * Blocking lock on the given mutex.
 *
 * This should only fail if mut is invalid, or if there are insufficient resources within
 * the kernel.
 */
fernos_error_t mut_lock(mutex_t *mut);

/**
 * Release a mutex.
 *
 * Returns a state mismatch, if the given lock is not in a locked state.
 *
 * (This will wake up a single thread waiting on the lock, if one exists)
 */
fernos_error_t mut_unlock(mutex_t *mut);
