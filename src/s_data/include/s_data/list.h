
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "s_util/err.h"
#include "s_mem/allocator.h"

/*
 * Pretty predictable list interface and implementations.
 */

typedef struct _list_t list_t;

typedef struct _list_impl_t {
    void (*delete_list)(list_t *l);

    size_t (*l_get_cell_size)(list_t *l);
    size_t (*l_get_len)(list_t *l);

    void *(*l_get_ptr)(list_t *l, size_t i);

    fernos_error_t (*l_push)(list_t *l, size_t i, const void *src);
    fernos_error_t (*l_pop)(list_t *l, size_t i, void *dest);

    void (*l_reset_iter)(list_t *l);
    void *(*l_get_iter)(list_t *l);
    void *(*l_next_iter)(list_t *l);
    fernos_error_t (*l_push_after_iter)(list_t *l, const void *src);
    fernos_error_t (*l_pop_iter)(list_t *l, void *dest);

    void (*l_dump)(list_t *l, void (*pf)(const char *fmt, ...));
} list_impl_t;

struct _list_t {
    const list_impl_t * const impl;
};

/**
 * Delete the list.
 *
 * If you need to do some special cleanup for each individual cell, it's expected you use the 
 * iterator to clean up, then call this function.
 */
static inline void delete_list(list_t *l) {
    if (l) {
        l->impl->delete_list(l);
    }
}

/**
 * A list is a collection of equally sized cells. This returns the size of just one cell.
 */
static inline size_t l_get_cell_size(list_t *l) {
    return l->impl->l_get_cell_size(l);
}

/**
 * A list is a collection of equally sized cells. This returns the size of just one cell.
 */
static inline size_t l_get_len(list_t *l) {
    return l->impl->l_get_len(l);
}

/**
 * Get a pointer to the element at index i. If there is no index i, this should return NULL.
 *
 * NOTE: This should be a pointer INTO the list, NOT a copy of the element. 
 */
static inline void *l_get_ptr(list_t *l, size_t i) {
    return l->impl->l_get_ptr(l, i);
}

/**
 * Insert an element at index i. i can be the length of the list if you'd like to append the 
 * element at the end.
 *
 * Returns an error if src is NULL, if i is invalid, or if there aren't enough resources to do
 * the push.
 */
static inline fernos_error_t l_push(list_t *l, size_t i, const void *src) {
    return l->impl->l_push(l, i, src);
}

/**
 * Remove an element at index i. 
 *
 * if dest is non-null, copy the element'c contents to dest.
 *
 * Returns an error if i is invalid.
 */
static inline fernos_error_t l_pop(list_t *l, size_t i, void *dest) {
    return l->impl->l_pop(l, i, dest);
}

/* Some push/pop helpers */

static inline fernos_error_t l_push_front(list_t *l, const void *src) {
    return l_push(l, 0, src);
}

static inline fernos_error_t l_pop_front(list_t *l, void *dest) {
    return l_pop(l, 0, dest);
}

static inline fernos_error_t l_push_back(list_t *l, const void *src) {
    return l_push(l, l_get_len(l), src);
}

static inline fernos_error_t l_pop_back(list_t *l, void *dest) {
    return l_pop(l, l_get_len(l) - 1, dest);
}

/*
 * Below are the "iterator" functions, these are all stateful and are to be used during iteration.
 *
 * When iterating, only ever use the calls below to modify the list.
 */

/**
 * Reset the iterator state.
 *
 * Sets the iterator to the first element of the list.
 */
static inline void l_reset_iter(list_t *l) {
    l->impl->l_reset_iter(l);
}

/**
 * Return the current iterator.
 *
 * returns NULL at the end of iteration!
 */
static inline void *l_get_iter(list_t *l) {
    return l->impl->l_get_iter(l);
}

/**
 * Advance the iterator and return its new value.
 *
 * Should return NULL when the end of the list is reached.
 */
static inline void *l_next_iter(list_t *l) {
    return l->impl->l_next_iter(l);
}

/**
 * Push an element into the position directly after the current iterator position. 
 *
 * Returns an error if src is NULL or if insufficient resources. Returns an error if the iterator is
 * NULL!
 */
static inline fernos_error_t l_push_after_iter(list_t *l, const void *src) {
    return l->impl->l_push_after_iter(l, src);
}

/**
 * Remove the current iterator from the list and advance the iterator to the next element.
 *
 * If dest is non-null, copy the popped element into the dest.
 *
 * Returns an error if the iterator is NULL. 
 */
static inline fernos_error_t l_pop_iter(list_t *l, void *dest) {
    return l->impl->l_pop_iter(l, dest);
}

/*
 * Misc/Debug functions.
 */

/**
 * Optional.
 *
 * Intended to dump some representation of the list using the given print function.
 */
static inline void l_dump(list_t *l, void (*pf)(const char *fmt, ...)) {
    if (l->impl->l_dump) {
        l->impl->l_dump(l, pf);
    }
}

typedef struct _linked_list_node_t linked_list_node_t;

/**
 * This is really more of a header. The data starts directly after the node in memory.
 */
struct _linked_list_node_t {
    /**
     * Will be NULL when this is the end of the list.
     */
    linked_list_node_t *next;

    /**
     * Will be NULL when this is the start of the list.
     */
    linked_list_node_t *prev;
};

/**
 * Standard linked list. Good for queues and stacks.
 */
typedef struct _linked_list_t {
    list_t super;

    allocator_t *al;

    size_t len;     
    size_t cs;

    linked_list_node_t *first;
    linked_list_node_t *last;

    linked_list_node_t *iter;
} linked_list_t;

/**
 * Returns NULL on failure to allocate or if cell size is 0.
 */
list_t *new_linked_list(allocator_t *al, size_t cs);

static inline list_t *new_da_linked_list(size_t cs) {
    return new_linked_list(get_default_allocator(), cs);
}


