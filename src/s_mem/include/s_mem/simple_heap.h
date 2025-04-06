
#pragma once

#include "s_mem/allocator.h"
#include "s_util/err.h"

/**
 * This is a function which will be used by the allocator to request pages of memory.
 * 
 * s and e MUST be 4K aligned.
 * true_e MUST be non-null.
 *
 * If success, the entire range was successfully allocated. (true_e is set to e).
 * An error may occur, if there isn't enough memory. OR, if the region requested already overlaps
 * with an allocated region. In both cases the allocation stops, and true_e is set to the end of 
 * the newly allocated region.
 */
typedef fernos_error_t (*request_mem_ft)(void *s, void *e, void **true_e);

/**
 * A simple heap allocator is my first allocator version.
 * It's designed to be used for managing a memory heap within a region of a predefined size.
 *
 * s and e MUST be 4K aligned. They also most denote a non-empty range.
 * These mark the area which will be occupied by the heap.
 * It is expected that the entire region is unallocated when this function is called.
 * The simple_heap_allocator will request memory as needed using rqm.
 */
allocator_t *new_simple_heap_allocator(void *s, const void *e, request_mem_ft rqm);
