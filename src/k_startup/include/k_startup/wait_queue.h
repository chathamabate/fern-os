
#pragma once

#include "k_startup/fwd_defs.h"
#include "s_util/err.h"

/**
 * A wait queue is a a collection of threads which are all in a waiting state.
 * When a thread is in a waiting state it belongs to one and only one wait queue.
 * When a thread is in an executing state, it belongs to no wait queues.
 *
 * Since the information needed to enqueue and dequeue threads depends on the context of the wait
 * queue, this interface only defines the signatures which MUST be present for all wait queues.
 *
 * We must be able to remove a thread forcefully from the queue (if the thread exits).
 * And we should also be able to delete the wait queue from just a wait_queue_t *.
 *
 * All concrete subclasses should implement some sort of enqueue function. This function should
 * take a thread (and maybe some more information) and add it to the wait queue.
 *
 * There should also be a notify like function. This function should take some (or no) information,
 * and move one, many, or none of the waiting threads to the executing state.
 *
 * enqueue and notify are not part of the interface because their signatures may differ depending
 * on how a wait queue is intended to be used.
 */
typedef struct _wait_queue_t wait_queue_t;

typedef struct _wait_queue_impl_t {
    void (*delete_wait_queue)(wait_queue_t *wq);
    void (*remove_thread)(wait_queue_t *wq, thread_t *t);
} wait_queue_impl_t;

struct _wait_queue_t {
    const wait_queue_impl_t * const impl;
};

/**
 * Delete the wait queue.
 */
static inline void delete_wait_queue(wait_queue_t *wq) {
    if (wq) {
        wq->impl->delete_wait_queue(wq);
    }
}

/**
 * Remove all references of the given thread from the wait queue. 
 *
 * This should only be used when a thread must be deleted while it is in the waiting state.
 */
static inline void wq_remove_thread(wait_queue_t *wq, thread_t *t) {
    if (wq) {
        wq->impl->remove_thread(wq, t);
    }
}

/**
 * A cond_wait_queue_t, is a wait queue used when a thread is waiting on a condition.
 */
typedef struct _cond_wait_queue_t {
    // Fill in later.
} cond_wait_queue_t;

/**
 * A condition's notify behavior is somewhat customizeable.
 */
typedef uint32_t cond_notify_mode_t;

/**
 * Wake up the thread which has been in the queue the longest. (FIFO)
 */
#define COND_NOTIFY_NEXT  (0)

/**
 * Wake up the last thread to be added to the queue. (LIFO)
 */
#define COND_NOTIFY_LAST  (1)

/**
 * Wake up all threads in the queue!
 */
#define COND_NOTIFY_ALL   (2)

// TODO: Constructor Signature!
// Destructor shouldn't need to be public?

fernos_error_t cwq_enqueue(cond_wait_queue_t *cwq, thread_t *t);

void cwq_notify(cond_wait_queue_t *cwq, cond_notify_mode_t mode);

static inline void cwq_notify_next(cond_wait_queue_t *cwq) {
    cwq_notify(cwq, COND_NOTIFY_NEXT);
}

static inline void cwq_notify_last(cond_wait_queue_t *cwq) {
    cwq_notify(cwq, COND_NOTIFY_LAST);
}

static inline void cwq_notify_all(cond_wait_queue_t *cwq) {
    cwq_notify(cwq, COND_NOTIFY_ALL);
}

