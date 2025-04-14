
#include "s_util/err.h"

typedef struct _scheduler_t scheduler_t;

typedef struct _scheduler_impl_t {
    fernos_error_t (*schedule)(scheduler_t *sc, void *item, void *arg);
    void (*delete_scheduler)(scheduler_t *sc);
} scheduler_impl_t;

struct _scheduler_t {
    const scheduler_impl_t * const impl;
};

/**
 * Abstractly "schedule" an item.
 *
 * arg is arbitrary.
 */
static inline fernos_error_t sc_schedule(scheduler_t *sc, void *item, void *arg) {
    if (!sc) {
        return FOS_BAD_ARGS;
    }

    return sc->impl->schedule(sc, item, arg);
}

static inline void delete_scheduler(scheduler_t *sc) {
    if (sc) {
        sc->impl->delete_scheduler(sc);
    }
}

typedef struct _wait_queue_t wait_queue_t;

typedef struct _wait_queue_impl_t {
    void (*delete_wait_queue)(wait_queue_t *wq);
    fernos_error_t (*wq_notify)(wait_queue_t *wq, void *arg);
    fernos_error_t (*wq_enqueue)(wait_queue_t *wq, void *item, void *arg);
    void (*wq_remove)(wait_queue_t *wq, void *item);
} wait_queue_impl_t;

struct _wait_queue_t {
    const wait_queue_impl_t * const impl;
    scheduler_t * const sc;
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
 * This should schedule one, multiple, or none of the items in this wait queue.
 *
 * How it chooses which (if any) to schedule is up to the implementor.
 *
 * Returns FOS_SUCCESS when everything is OK, otherwise some other error code.
 *
 * arg is in arbitrary argument and does not need to be used. 
 */
static inline fernos_error_t wq_notify(wait_queue_t *wq, void *arg) {
    if (!wq) {
        return FOS_BAD_ARGS;
    }

    return wq->impl->wq_notify(wq, arg);
}

/**
 * Add an item to this wait queue.
 *
 * arg is once again an arbitrary argument.
 */
static inline fernos_error_t wq_enqueue(wait_queue_t *wq, void *item, void *arg) {
    if (!wq) {
        return FOS_BAD_ARGS;
    }

    return wq->impl->wq_enqueue(wq, item, arg);
}

/**
 * Remove an item forcefully from the queue without scheduling it.
 */
static inline void wq_remove(wait_queue_t *wq, void *item) {
    if (!wq) {
        return;
    }

    wq->impl->wq_remove(wq, item);
}


