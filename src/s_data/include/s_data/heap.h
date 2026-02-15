
#pragma once

#include "s_mem/allocator.h"
#include "s_util/hash.h"

typedef struct _heap_t {
    allocator_t * const al;

    /**
     * Binary tree format.
     *
     * [root, root.left, root.right, root.left.left, root.left.right, root.right.left, root.right.right, .....]
     */
    uint8_t *buf;

    /**
     * Size of each cell in `buf`.
     */
    const size_t cell_size;

    /**
     * `buf` has capacity `cell_size` * `buf_cap`.
     */
    size_t buf_cap;

    /**
     * Number of nodes in the heap.
     *
     * (Stored in the first `tree_len` cells of `buf`)
     */
    size_t tree_len;

    /**
     * Used for adding/removing elements from the heap.
     */
    const comparator_ft cmp;
} heap_t;

/**
 * Create a new heap, returns NULL on error.
 */
heap_t *new_heap(allocator_t *al, size_t cs, comparator_ft cmp);
static inline heap_t *new_da_heap(size_t cs, comparator_ft cmp) {
    return new_heap(get_default_allocator(), cs, cmp);
}

/**
 * NOTE: BE CAREFUL, this does not special cleanup on individual cells!
 *
 * (This matters if each cell is a pointer to a dynamic structure which must be freed)
 */
void delete_heap(heap_t *hp);

/**
 * Is this heap empty?
 */
static inline bool hp_is_empty(heap_t *hp) {
    return hp->tree_len == 0;
}

/**
 * Pop the maximum value off the heap.
 *
 * Returns `true` if an element was popped.
 * Returns `false` if the heap is empty, and thus no element could be popped.
 *
 * `out` is optional, if given, and the heap is non-empty, the max will be written to out.
 * Make sure `out` points to a free area of size at least cell size.
 */
bool hp_pop_max(heap_t *hp, void *out);

/**
 * Push a value onto the heap.
 *
 * Returns FOS_E_BAD_ARGS if `val` is NULL.
 * Returns FOS_E_NO_MEM if an allocation fails.
 * Returns FOS_E_SUCCESS if `val` was pushed on the heap!
 *
 * NOTE: `val` the pointer IS NOT stored in the heap.
 * Rather `cell_size` bytes are copied from `*val` into a free cell in the heap buffer.
 *
 * Be careful!
 */
fernos_error_t hp_push(heap_t *hp, const void *val);
