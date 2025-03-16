
#pragma once

#include "k_sys/page.h"
#include "os_defs.h"
#include "s_util/misc.h"
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
 * These are the page tables to always use for addressing into the identity area.
 * Do not change these ever.
 *
 * Also, note that the first pte is always marked to preset so that a NULL derefernce results 
 * in a page fault.
 */
extern pt_entry_t identity_pts[NUM_IDENTITY_PTS][1024];

/**
 * The page directory used by the kernel.
 *
 * This can and will be modified to give the kernel access to user task pages
 */
extern pt_entry_t kernel_pd[1024];

/**
 * A free page which is used inside the kernel to help with page table modification.
 */
extern uint8_t free_page[M_4K];




// Returns 0 on success, 1 on Failure.
//int init_paging(void);

