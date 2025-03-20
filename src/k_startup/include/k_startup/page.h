
#pragma once

#include "k_sys/page.h"
#include "os_defs.h"
#include "s_util/misc.h"
#include "s_util/err.h"
#include <stdint.h>

#define NUM_IDENTITY_PTS        (IDENTITY_AREA_SIZE / M_4M)
#define NUM_IDENTITY_PT_ENTRIES (IDENTITY_AREA_SIZE / M_4K)

/**
 * The area of pages which can be partitioned at runtime.
 *
 * NOTE: In QEMU, even with 4G memory, the highest Gig is reserved for other things.
 * For now, we only use the bottom 3 Gigs.
 *
 * These both must be 4K aligned!
 */

#define FREE_PAGE_AREA_START (IDENTITY_AREA_SIZE)
#define FREE_PAGE_AREA_END (0xC0000000)

/**
 * Initialize all structures required for paging, and enable paging.
 *
 * NOTE: The first page directory used will be `kernel_pd`.
 */
fernos_error_t init_paging(void);

/**
 * An error is returned if the given page_addr is blatantly invalid.
 * (i.e. it's in the identity area)
 *
 * NOTE: If page addr is not in the identity area, it is assumed to be valid, so be careful!
 */
fernos_error_t push_free_page(phys_addr_t page_addr);

/**
 * Attempt to pop a free page from the free page linked list.
 *
 * An error is returned if there are no free pages left, or page_addr is NULL.
 */
fernos_error_t pop_free_page(phys_addr_t *page_addr);

/**
 * This will attempt to allocate pages from start to end in the given page directory.
 *
 * The last page which was not allocated will be stored in `true_end`
 * If true_end = end, the allocation was entirely successful!
 *
 * FOS_NO_MEM means memory was exhausted somehow while performing the allocation.
 * FOS_ALREADY_ALLOCATED means at some point in the given range there already existed an 
 * allocated range.
 *
 * In case of an error, pages which were allocated will NOT be freed. Use `true_end` to determine
 * how much of the range was successfully allocated.
 */
fernos_error_t allocate_pages(phys_addr_t pd, void *start, void *end, void **true_end);

/**
 * This will attempt to free pages from start up until end. 
 *
 * Free pages which already exist in this range will be ignored.
 *
 * Error is only returned if given arguemnts are invalid somehow.
 */
fernos_error_t free_pages(phys_addr_t pd, void *start, void *end);

