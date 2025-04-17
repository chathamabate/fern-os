


#pragma once

#include "s_mem/allocator.h"
#include "s_util/err.h"
#include "s_data/list.h"

/**
 * A wait queue is intended to be used for managing asleep threads in the kernel, but should not
 * actually need any internal thread information to work! For that reason, it is in this shared
 * module, and not in a kernel module.
 *
 * Since each wait queue implementation may abide by a slightly different interface, the parental
 * interface is pretty sparse. We really just need to be able to remove an item from any wait
 * queue using the same function signature.
 *
 * A concrete wait queue implementation should expose the following behaviors:
 *
 * 1) Enqueue an item.
 * 2) Notify, which should mark 0 or more items as ready to be dequeued.
 * 3) Pop, Which pop an item which is ready from the queue.
 *
 * These three behaviors are not included interface below since they may all require additional
 * arguments/return values.
 */
typedef struct _wait_queue_t wait_queue_t;

typedef struct _wait_queue_impl_t {
    void (*delete_wait_queue)(wait_queue_t *wq);
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
 * Remove all references of the given item from the wait queue. 
 */
static inline void wq_remove(wait_queue_t *wq, void *item) {
    wq->impl->wq_remove(wq, item);
}

/**
 * OPTIONAL.
 *
 * Print some arbitrary debug information about the queue.
 */
static inline void wq_dump(wait_queue_t *wq, void (*pf)(const char *fmt, ...)) {
    if (wq->impl->wq_dump) {
        wq->impl->wq_dump(wq, pf);
    }
}

/**
 * A basic_wait_queue_t is a pretty standard wait queue type.
 *
 * No information is needed about the item beinging enqueued, additionally, no context is output
 * when popping items from the queue.
 */
typedef struct _basic_wait_queue_t {
    allocator_t *al;

    /**
     * The queue of waiting items.
     */
    list_t *wait_q;

    /**
     * The ready queue.
     */
    list_t *ready_q;
} basic_wait_queue_t;

/**
 * A basic wait queue's notify behavior is somewhat customizeable.
 */
typedef uint32_t bwq_notify_mode_t;

/**
 * Ready the item which has been in the queue the longest. (FIFO)
 */
#define BWQ_NOTIFY_NEXT  (0)

/**
 * Ready the last item to be added to the queue. (LIFO)
 */
#define BWQ_NOTIFY_LAST  (1)

/**
 * Ready all items in the queue! (In FIFO Order)
 */
#define BWQ_NOTIFY_ALL   (2)

basic_wait_queue_t *new_basic_wait_queue(allocator_t *al);

static inline basic_wait_queue_t *new_da_basic_wait_queue(void) {
    return new_basic_wait_queue(get_default_allocator());
}

/**
 * Enqueue an item.
 *
 * Should only fail if we run out of resources.
 */
fernos_error_t bwq_enqueue(basic_wait_queue_t *bwq, void *item);

/**
 * Ready certain items depending on what mode is given.
 *
 * Should only fail if we run out of resources while moving items from the waiting items to the
 * ready queue. In this case, we just stop with only some or none of the items being switched 
 * between internal queues.
 */
fernos_error_t bwq_notify(basic_wait_queue_t *bwq, bwq_notify_mode_t mode);

static inline void bwq_notify_next(basic_wait_queue_t *bwq) {
    bwq_notify(bwq, BWQ_NOTIFY_NEXT);
}

static inline void bwq_notify_last(basic_wait_queue_t *bwq) {
    bwq_notify(bwq, BWQ_NOTIFY_LAST);
}

static inline void bwq_notify_all(basic_wait_queue_t *bwq) {
    bwq_notify(bwq, BWQ_NOTIFY_ALL);
}

/**
 * Pop a ready item from the queue.
 *
 * Should return items in the order they were made ready!
 * 
 * Should return FOS_EMPTY if there are no ready items. (NOTE: There still may be waiting items)
 * In this case, NULL should be written to item (if item is given)
 *
 * Returns FOS_SUCCESS if a ready item was successfully popped. If item is given, the popped item
 * will be stored at *item.
 */
fernos_error_t bwq_pop(basic_wait_queue_t *bwq, void **item);



/**
 * A sig_wait_queue_t is a queue which manages threads waiting on signals to arrive.
 */
typedef struct _sig_wait_queue_t {
    // TODO: Fill this in.
    int nop;
} sig_wait_queue_t;

/**
 * Thread t should be woken up when any one of the signals marked in sig mask are received.
 */
//fernos_error_t swq_enqueue(sig_wait_queue_t *swq, thread_t *t, uint32_t sig_mask);

/**
 * This should iterate over the threads in the queue. If t.mask | *sig_vec, then that thread
 * is woken up and given t.mask | *sig_vec, afterwards *sig_vec <= *sig_vec & ~t.mask.
 *
 * This is done repeatedly until the signal vector is empty, or all threads have been searched.
 *
 * In one notify call, no two threads are ever woken up for the same signal!
 */
void swq_notify(sig_wait_queue_t *swq, uint32_t *sig_vec);

