
#pragma once

#include "s_util/err.h"
#include "s_bridge/shared_defs.h"

/**
 * Create a new sempahore!
 *
 * A semaphore is just an integer stored within the kernel. Threads across processes 
 * can decrement and increment the integer as a form of synchronization.
 * A common analogy is that a semaphore is akin to a number of "hall-passes".
 * When the integer has a value of 0, decrementing will block the current thread.
 * Similar to a student waiting for a hall pass to be returned.
 *
 * `sem` is where to store the id of the new semaphore.
 * `num_passes` is the maximum value of the semaphore's internal counter.
 *
 * Returns FOS_E_BAD_ARGS is `sem` is NULL.
 * Returns FOS_E_BAD_ARGS if `num_passes` is 0.
 * Returns FOS_E_NO_SPACE if the sempahore table is already full.
 * Returns FOS_E_NO_MEM if we fail to allocate the sempahore state.
 *
 * Returns FOS_E_SUCCESS on success. Only in this case is `sem` written to.
 */
fernos_error_t sc_shm_new_semaphore(sem_id_t *sem, uint32_t num_passes);

/**
 * This attempts to decrement the sempahore's internal pass counter!
 *
 * If the pass counter is 0, this call blocks the current thread until pass counter becomes
 * non-zero.
 *
 * Returns FOS_E_BAD_ARGS if the given semaphore id doesn't reference a semaphore available to 
 * the calling process.
 * Returns FOS_E_NO_MEM if we fail to add our thread to the wait queue in the waiting case.
 * Returns FOS_E_STATE_MISMATCH if the calling process closes the semaphore while this thread is 
 * waiting.
 * Returns FOS_E_SUCCESS when the semaphore has been successfully decremented.
 */
fernos_error_t sc_shm_sem_dec(sem_id_t sem);

/**
 * Increment a semaphore!
 *
 * If the semaphore's current value is zero, then incrementing will wake up one waiting thread!
 * (If there are any)
 * 
 * This is a destructor style call, and thus will return nothing!
 * Just remember that incrementing will NEVER push the semaphore pass count past it's max value!
 *
 * ALSO: Anyone can increment or decrement whenever. A thread which never called decrement
 * is allowed to call increment. So, be careful!
 */
void sc_shm_sem_inc(sem_id_t sem);

/**
 * Close a semaphore!
 *
 * This is a destructor style call, and thus returns nothing.
 *
 * NOTE: The underlying semaphore is only actually deleted if its reference count reaches 0.
 * NOTE: If the calling process has threads which are currently in the given semaphore's 
 * wait queue, said threads are woken up with return code `FOS_E_STATE_MISMATCH`.
 */
void sc_shm_close_semaphore(sem_id_t sem);
