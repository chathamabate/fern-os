
#pragma once

#include "s_util/err.h"

typedef struct _wait_queue_t wait_queue_t;

typedef struct _wait_queue_impl_t {
    void (*delete_wait_queue)(wait_queue_t *wq);

    fernos_error_t (*wq_enqueue)(wait_queue_t *wq, void *item, uint32_t nq_arg);
    void (*wq_notify)(wait_queue_t *wq, uint32_t nt_arg);
    fernos_error_t (*wq_pop)(wait_queue_t *wq, void **item, uint32_t *ctx);

    void (*wq_remove)(wait_queue_t *wq, void *item);

    void (*wq_dump)(wait_queue_t *wq, void (*pf)(const char *fmt, ...));
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
 * Enqueue an item. 
 *
 * nq_arg can be used optionally, and need not be treated as a uint32_t. It's just meant to provide
 * custom context as to why this item is being enqueued/when it should be woken up.
 *
 * Should return an error if there is an error enqueueing.
 */
static inline fernos_error_t wq_enqueue(wait_queue_t *wq, void *item, uint32_t nq_arg) {
    return wq->impl->wq_enqueue(wq, item, nq_arg); 
}

/**
 * Notify the queue of some event. The event can be described with nt_arg. 
 * NOTE: nt_arg need not be the same type as nq_arg. It is entirely up to the implementor how these
 * args are interpreted.
 *
 * The intention is that this call will result in certain items being marked as ready.
 * When an item is marked ready, it is able to be popped with `wq_pop`.
 */
static inline void wq_notify(wait_queue_t *wq, uint32_t nt_arg) {
    wq->impl->wq_notify(wq, nt_arg);
}

/**
 * Pops an item which has been marked ready from the queue.
 *
 * The popped item is stored at *item. Also, a context can be output at *ctx.
 * If item is NULL, the pop should still be carried out, just without saving the output.
 *
 * If there are not ready items, FOS_EMPTY should be returned.
 * On success, FOS_SUCCESS should be returned.
 * Other errors can be returned if there were some other issue with popping.
 */
static inline fernos_error_t wq_pop(wait_queue_t *wq, void **item, uint32_t *ctx) {
    return wq->impl->wq_pop(wq, item, ctx);
}


/**
 * At any time, we should be able to remove an item from the queue regardless of whether it's 
 * marked ready or not. The idea here is that if an item is independenly destroyed somewhere else,
 * we will want to remove all references to the item from other structures (like this one).
 */
static inline void wq_remove(wait_queue_t *wq, void *item) {
    wq->impl->wq_remove(wq, item);
}


/**
 * OPTIONAL.
 *
 * Dump some debug information about the wait queue.
 */
static inline void wq_dump(wait_queue_t *wq, void (*pf)(const char *fmt, ...)) {
    if (wq->impl->wq_dump) {
        wq->impl->wq_dump(wq, pf);
    }
}

