
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "s_mem/allocator.h"

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

// My brain is like totally fried tbh... Not sure what to do about this...
