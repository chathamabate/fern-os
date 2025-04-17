


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

/*
 * Basic Wait Queue.
 */

/**
 * A basic_wait_queue_t is a pretty standard wait queue type.
 *
 * No information is needed about the item beinging enqueued, additionally, no context is output
 * when popping items from the queue.
 */
typedef struct _basic_wait_queue_t {
    wait_queue_t super;

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

/*
 * Vector Ready Queue.
 */

/**
 * How waiting items are stored.
 */
typedef struct _vwq_wait_pair_t {
    void *item;
    uint32_t ready_mask;
} vwq_wait_pair_t;

/**
 * How ready items are stored.
 */
typedef struct _vwq_ready_pair_t {
    void *item;
    uint8_t ready_id;
} vwq_ready_pair_t;

/**
 * The idea of a vector wait queue is that a set of 32 different events can affect the queue.
 *
 * Each enqueued item is paired with a 32 bit vector which determines which of the events can
 * "ready" the item.
 */
typedef struct _vector_wait_queue_t {
    wait_queue_t super;

    allocator_t *al;

    /**
     * The queue of waiting items. 
     *
     * list<vwq_wait_pair_t>
     */
    list_t *wait_q;

    /**
     * The ready queue.
     *
     * list<vwq_ready_pair_t>
     */
    list_t *ready_q;
} vector_wait_queue_t;

/**
 * This ready ID is returned when there is nothing to pop from the ready queue. It represents
 * an invalid ready ID.
 */
#define VWQ_NULL_READY_ID (32)

/**
 * Like the basic wait queue, the vector wait queue also has different notify modes.
 */
typedef uint32_t vwq_notify_mode_t;

/**
 * Notify the first waiting item which accepts the given id. (Remember by "first" we me the oldest
 * item in the waiting queue, FIFO)
 */
#define VWQ_NOTIFY_FIRST (0)

/**
 * Notify ALL waiting items which accept the given id. (Loaded onto the ready queue in FIFO order)
 */
#define VWQ_NOTIFY_ALL   (1)

/**
 * Returns NULL in case of insufficient resources.
 */
vector_wait_queue_t *new_vector_wait_queue(allocator_t *al);

static inline vector_wait_queue_t *new_da_vector_wait_queue(void) {
    return new_vector_wait_queue(get_default_allocator());
}

/**
 * Enqueue an item in the vector wait queue.
 *
 * The `ready_mask` is 32-bit value which describes which ids can ready this item when its in the
 * wait queue.
 *
 * An error is returned if there aren't sufficient resources to enqueue item, or if the ready mask
 * is 0.
 */
fernos_error_t vwq_enqueue(vector_wait_queue_t *vwq, void *item, uint32_t ready_mask);

/**
 * Notify waiting items depending on the given ready_id.
 * ready_id MUST be in the range 0-31. Only threads which have the bit (1 << ready_id) set in
 * their ready mask are able to be readied. Which ones are readied depends on the mode set.
 *
 * An error is returned if we don't have sufficient resources. In this case the notification 
 * procedure will stop. All items which were readied will remain in the ready queue. All items
 * which failed to be readied will remain in the waiting queue.
 *
 * An error is also returned if ready_id >= 32.
 */
fernos_error_t vwq_notify(vector_wait_queue_t *vwq, uint8_t ready_id, vwq_notify_mode_t mode);

static inline fernos_error_t vwq_notify_first(vector_wait_queue_t *vwq, uint8_t ready_id) {
    return vwq_notify(vwq, ready_id, VWQ_NOTIFY_FIRST);
}

static inline fernos_error_t vwq_notify_all(vector_wait_queue_t *vwq, uint8_t ready_id) {
    return vwq_notify(vwq, ready_id, VWQ_NOTIFY_ALL);
}

/**
 * Pop an item off the ready queue.
 *
 * Returns FOS_EMPTY if there are no ready items. In this case, NULL should be written to item 
 * (if given), and VWQ_NULL_READY_ID should be written to ready_id (if given).
 *
 * Returns FOS_SUCCESS if there is at least one item in the ready queue. In this case, the
 * item is popped from the ready queue. If item is given, the popped item is written at *item.
 * If ready_id is given, the ready_id which readied the popped item will be written at *ready_id.
 */
fernos_error_t vwq_pop(vector_wait_queue_t *vwq, void **item, uint8_t *ready_id);
