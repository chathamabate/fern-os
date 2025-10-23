
#pragma once

#include <stddef.h>

typedef struct _ring_t ring_t;
typedef struct _ring_element_t ring_element_t;

/**
 * A ring is an light weight doubly cyclic linked list meant to be embedded into
 * other structures.
 *
 * NOTE: The fields of `ring_t` and `ring_element_t` are extremely simple and designed to be read
 * directly. However, the helper functions below should be used for when modifying a ring.
 */
struct _ring_t {
    /**
     * Number of elements in the ring.
     */
    size_t len;

    ring_element_t *head;
};

struct _ring_element_t {
    ring_t *r;

    ring_element_t *next;
    ring_element_t *prev;
};

/**
 * Initialize a ring. The ring starts as empty.
 */
void init_ring(ring_t *r);

/**
 * Initialize a ring element. (All NULL at first)
 */
void init_ring_element(ring_element_t *re);

/**
 * Advance the head of the ring to the next element.
 */
static inline void ring_advance(ring_t *r) {
    if (r->head) {
        r->head = r->head->next;
    }
}

/**
 * Remove the given element from the ring.
 * Does nothing if the element is already detached.
 *
 * If this element is the current head of the ring, 
 * the ring's head will bedome `re->next`.
 */
void ring_element_detach(ring_element_t *re);

/**
 * Add an element a ring. 
 *
 * If `re` is already in a ring, it is detached first.
 *
 * `re` will be placed directly behind the head of `r`.
 * If `r` has no head, `re` becomes the head.
 */
void ring_element_attach(ring_element_t *re, ring_t *r);


