
#include "k_startup/fwd_defs.h"
#include "s_util/err.h"

/**
 * A wait queue is a collection of thread ids which are all waiting.
 *
 * The queue has the ability to take waiting threads and reschedule them!
 *
 * It is the user's responsibility to gaurantee a thread is never in more than one wait queue.
 */
typedef struct _wait_queue_t wait_queue_t;

typedef struct _wait_queue_impl_t {
    void (*delete_wait_queue)(wait_queue_t *wq);
    fernos_error_t (*wq_notify)(wait_queue_t *wq, void *arg);
    fernos_error_t (*wq_enqueue)(wait_queue_t *wq, glbl_thread_id_t gtid, void *arg);
    void (*wq_remove)(wait_queue_t *wq, glbl_thread_id_t gtid);
} wait_queue_impl_t;

struct _wait_queue_t {
    const wait_queue_impl_t * const impl;
};

/**
 * Delete the wait queue, this is ok if wq is NULL.
 */
static inline void delete_wait_queue(wait_queue_t *wq) {
    if (wq) {
        wq->impl->delete_wait_queue(wq);
    }
}

/**
 * This should schedule one, multiple, or none of the threads in this wait queue.
 *
 * How it chooses which (if any) to schedule is up to the implementor.
 *
 * Returns FOS_SUCCESS when everything is OK, otherwise some other error code.
 *
 * arg is in arbitrary argument and does not need to be used. 
 */
static inline fernos_error_t wq_notify(wait_queue_t *wq, void *arg) {
    return wq->impl->wq_notify(wq, arg);
}

/**
 * Add a thread to this wait queue.
 *
 * Undefined behavior if the thread already belongs to another wait queue!
 *
 * arg is once again an arbitrary argument.
 */
static inline fernos_error_t wq_enqueue(wait_queue_t *wq, glbl_thread_id_t gtid, void *arg) {
    return wq->impl->wq_enqueue(wq, gtid, arg);
}

/**
 * Remove a thread from the queue forcefully.
 *
 * This is to only be used when a thread is cleaned up before it is scheduled.
 * (When a process exits, it must remove all references to its threads)
 *
 * Does nothing if the given thread id does not belong to the queue.
 */
static inline void wq_remove(wait_queue_t *wq, glbl_thread_id_t gtid) {
    wq->impl->wq_remove(wq, gtid);
}


