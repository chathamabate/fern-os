
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "s_mem/allocator.h"

/**
 * An ID is really just an index into the ID table.
 *
 * For this reason, it is a gaurantee that all ids are less than their table's capacity.
 */
typedef uint32_t id_t;

typedef struct _id_table_entry_t {
    /**
     * Whether or not this entry is in use.
     */
    bool allocated;

    /*
     * The table contains two linked lists, a linked list of free entries, and a linked list 
     * of allocated entries.
     *
     * Thus, all entries belong to one of the two linked lists.
     *
     * The below pointers use max_cap to signify NULL.
     */

    id_t next;
    id_t prev;

    /**
     * The arbitrary data pointer.
     */
    void *data;
} id_table_entry_t;

/**
 * An ID Table is map from id_t's to void *'s.
 *
 * It is used when strong performance is needed when using a group of things which all have unique
 * IDs.
 *
 * The point is that once an ID is given it is never altered until the ID is returned.
 * All operations have about constant time operation.
 */
typedef struct _id_table_t {
    /**
     * What allocator is used by the table.
     */
    allocator_t *al;

    /**
     * It is promised that all ids in this table are less than max_cap.
     */
    uint32_t max_cap;

    /**
     * The current capacity of the table.
     */
    uint32_t cap;

    /**
     * Head entry of the allocated list.
     *
     * (Equals max_cap when there are no allocated entries in the table)
     */
    id_t al_head;

    /**
     * Head of the entry free list.
     *
     * (Equals max_cap when there are no free entries in the table)
     */
    id_t fl_head;

    /**
     * The table.
     */
    id_table_entry_t *tbl;

    /**
     * The iterator. max_cap means end of allocated list has been reached.
     */
    id_t iter;
} id_table_t;

/**
 * Create a new id table with the given max capacity.
 *
 * Returns NULL on error.
 */
id_table_t *new_id_table(allocator_t *al, uint32_t mc);

static inline id_table_t *new_da_id_table(uint32_t mc) {
    return new_id_table(get_default_allocator(), mc);
}

/**
 * NOTE: This does NOT clean up the data pointers.
 *
 * If some cleanup is needed there, that is your responsibility.
 */
void delete_id_table(id_table_t *idtb);

static inline id_t idtb_null_id(id_table_t *idtb) { 
    return idtb->max_cap;
}

/**
 * This is used in the case where you must place a pointer at a specific ID.
 *
 * An error is returned if that id is invalid or already in use!
 */
fernos_error_t idtb_request_id(id_table_t *idtb, id_t id);

/**
 * This pops an ID from the free list and marks its cell as allocated.
 */
id_t idtb_pop_id(id_table_t *idtb);

/**
 * This pushes an ID's cell onto the free list.
 *
 * Does nothing if the cell is already free, or if the id is invalid.
 */
void idtb_push_id(id_table_t *idtb, id_t id);

/**
 * Get the data pointer of a given Cell.
 *
 * Returns NULL if the id is invalid or if its cell isn't allocated.
 */
static inline void *idtb_get(id_table_t *idtb, id_t id) {
    if (id >= idtb->cap || !(idtb->tbl[id].allocated)) {
        return NULL;
    }

    return idtb->tbl[id].data;
}

/**
 * Set the data pointer of a given cell.
 *
 * Does nothing if the given id is invalid or if its cell isn't allocated.
 */
static inline void idtb_set(id_table_t *idtb, id_t id, void *dp) {
    if (id < idtb->cap && idtb->tbl[id].allocated) {
        idtb->tbl[id].data = dp;
    }
}

/**
 * An ID Table will expose an iterator which allows for efficient iterating over all allocated
 * IDs.
 *
 * To use the iterator, first use this endpoint. This should set the iterator to one element in the 
 * table (given it isn't empty)
 *
 * NOTE: This is a stateful call! NEVER EVER edit the table while iterating.
 * If the table IS edited, make sure to call idtb_reset_iterator before iterating again.
 */
void idtb_reset_iterator(id_table_t *idtb);

/**
 * Get the current value of the iterator, will equal the NULL_ID when the end is reached.
 */
id_t idtb_get_iter(id_table_t *idtb);

/**
 * Advances the iterator and returns its new value.
 */
id_t idtb_next(id_table_t *idtb);

/**
 * Helpful function for debugging.
 *
 * No set behavoir, should just output some sort of text based representation using the given pf 
 * function.
 */
void idtb_dump(id_table_t *idtb, void (*pf)(const char *fmt, ...));

