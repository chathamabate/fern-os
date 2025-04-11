
#pragma once

#include "s_mem/allocator.h"
#include "s_util/err.h"

/**
 * These attributes are used to configure how the simple heap allocator functions.
 * 
 * All attributes are required to be specified!
 */
typedef struct _simple_heap_attrs_t {
    /**
     * Start of the allocation region. (4K Aligned)
     */
    void *start; 

    /**
     * Exclusive end of the allocation region. (4K Aligned)
     */
    const void *end;

    /**
     * Functions used for requesting and returning memory.
     */
    mem_manage_pair_t mmp;

    /*
     * There will be two free lists for performance.
     *
     * A "large" free list, which holds all free blocks with sizes above a certain value.
     * And a "small" free list which hold all other free blocks.
     */

    /**
     * All free blocks with size less than this value will be in the small free list.
     * All other free blocks will be in the large free list.
     */
    size_t small_fl_cutoff;

    /**
     * How many entires to search in the small free list before switching to the large
     * free list. (0 means the entire small free list is searched)
     */
    size_t small_fl_search_amt;

    /**
     * How many entries in the large free list to search before just allocating new pages.
     * (0 means the entire large free list is searched)
     */
    size_t large_fl_search_amt;

    /*
     * When the request mem call fails (Meaning we have occupied the entire region)....
     *
     * 1) If attempting to allocate a large block, the entire large free list is searched before
     * returning NULL.
     *
     * 2) If attempting to allocate a small block (and the large list is empty), the entire small
     * list is searched before returning NULL.
     */
} simple_heap_attrs_t;

/**
 * A simple heap allocator is my first allocator version.
 * It's designed to be used for managing a memory heap within a region of a predefined size.
 *
 * It is expected that the entire region is unallocated when this function is called.
 * The simple_heap_allocator will request memory as needed.
 */
allocator_t *new_simple_heap_allocator(simple_heap_attrs_t attrs);
