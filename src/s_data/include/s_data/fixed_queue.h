
#pragma once

#include "s_mem/allocator.h"

/**
 * A fixed queue will be a queue with a fixed capacity that never grows.
 * (If you want a growing queue, I'd recommend just using a list)
 */
typedef struct _fixed_queue_t {
    allocator_t * const al;

    const size_t cell_size;
    const size_t capacity;

    /**
     * sizeof(*buffer) will always be `cell_size * capacity`.
     *
     * NOTE: this buffer will be used in a cyclic manner.
     */
    uint8_t * const buffer;

    /**
     * The index of the first occupied cell.
     */
    size_t start;

    /**
     * The index of the last occupied cell (inclusive)
     */
    size_t end;

    /**
     * If `start == end`, the buffer is empty iff `empty == false`.
     * When `start != end`, this field will ALWAYS be false!
     */
    bool empty;
} fixed_queue_t;

/**
 * Create a new fixed queue.
 * Returns NULL if allocation fails or if `cs == 0` or `cap == 0`.
 */
fixed_queue_t *new_fixed_queue(allocator_t *al, size_t cs, size_t cap);

static inline fixed_queue_t *new_da_fixed_queue(size_t cs, size_t cap) {
    return new_fixed_queue(get_default_allocator(), cs, cap);
}

/**
 * Delete a fixed queue.
 */
void delete_fixed_queue(fixed_queue_t *fq); 

static inline bool fq_is_empty(fixed_queue_t *fq) {
    return fq->empty;
}

/**
 * Enqueue an element.
 *
 * Returns `true` if the element is enqueued.
 * Returns `false` if the element was not enqueued. This only happens when the queue is full.
 *
 * If `writeover` is `true`, the element is always enqueued. That is, the oldest element in the
 * queue is written over. `true` is always returned in this case.
 */
bool fq_enqueue(fixed_queue_t *fq, const void *ele, bool writeover);

/**
 * Remove the oldest element in the queue and write its contents to `*dest`.
 *
 * Returns `true` if an element was dequeued.
 * Returns `false` if the queue is empty. Thus, no element could be dequeued.
 */
bool fq_poll(fixed_queue_t *fq, void *dest);


