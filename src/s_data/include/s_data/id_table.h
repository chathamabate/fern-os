
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

    union {
        /**
         * If the entry is in use, this hold the arbitrary data pointer.
         */
        void *data;

        /**
         * If the entry is not in use, this holds the id of the next free entry.
         * If there are no more free entries, this holds the max_cap.
         */
        id_t next;
    } cell;
} id_table_entry_t;

/**
 * An ID Table is map from id_t's to void *'s.
 *
 * It is used when strong performance is needed when using a group of things which all have unique
 * IDs.
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
     * Head of the entry free list.
     *
     * (Equals max_cap when there are no free entries in the table)
     */
    id_t fl_head;

    /**
     * The table.
     */
    id_table_entry_t *tbl;
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
 * This pops an ID from the free list and marks its cell as allocated.
 */
id_t idtb_pop_id(id_table_t *idtb);

/**
 * This pushes an ID's cell onto the free list.
 *
 * Does nothing if the cell is already free.
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

    return idtb->tbl[id].cell.data;
}

/**
 * Set the data pointer of a given cell.
 *
 * Does nothing if the given id is invalid or if its cell isn't allocated.
 */
static inline void idtb_set(id_table_t *idtb, id_t id, void *dp) {
    if (id < idtb->cap && idtb->tbl[id].allocated) {
        idtb->tbl[id].cell.data = dp;
    }
}

