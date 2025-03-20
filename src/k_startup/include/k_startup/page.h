
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
 * These values are used in the available bits of a page table entry.
 *
 * A unique entry points to a page which is only refernced by this page table. If this page table
 * is deleted, the refernced page should be returned to the page free list.
 *
 * A shared entry points to a page which is referenced by other page tables. If this page table
 * is deleted, the referenced page should NOT be returned to the page free list.
 */

#define UNIQUE_ENTRY (0)
#define SHARED_ENTRY (1)

static inline pt_entry_t fos_present_pt_entry(phys_addr_t base) {
    pt_entry_t pte;

    // I consider this FOS specific because FOS doesn't use privilege levels.
    // All pages are ring 0.

    pte_set_present(&pte, 1);
    pte_set_base(&pte, base);
    pte_set_user(&pte, 0);
    pte_set_writable(&pte, 1);

    return pte;
}

static inline pt_entry_t fos_unique_pt_entry(phys_addr_t base) {
    pt_entry_t pte = fos_present_pt_entry(base);

    pte_set_avail(&pte, UNIQUE_ENTRY);

    return pte;
}

static inline pt_entry_t fos_shared_pt_entry(phys_addr_t base) {
    pt_entry_t pte = fos_present_pt_entry(base);

    pte_set_avail(&pte, SHARED_ENTRY);

    return pte;
}

/*
 * NOTE: There will be a page directory stored in static memory which must always be loaded when
 * the kernel thread is running! 
 *
 * `init_paging` sets up the kernel page directory and loads it as the directory to use
 * when paging is first enabled!
 */

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
 * Create a new page table, all entries will start as not present.
 */
fernos_error_t new_page_table(phys_addr_t *pt_addr);

/**
 * Delete a page table. 
 */
fernos_error_t delete_page_table(phys_addr_t pt_addr);

/**
 * Create a new default page directory and store it's physical address at pd_addr.
 *
 * NOTE: This is just a directory where all identity page tables are mapped, and all other page
 * entries are marked non-present.
 */
fernos_error_t new_page_directory(phys_addr_t *pd_addr);

/**
 * Take the physical address of a page directory and clean up all of it's pages, page tables, and
 * finally, the page directory it self!
 */
fernos_error_t delete_page_directory(phys_addr_t pd_addr);

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
 * NOTE: In the current implementation, completely empty page tables are themselves left allocated.
 * So, if you deallocate a full 4MB region, the page table for that 4MB area will only contain 
 * not present entries, but it will remain allocated and in the parent page directory.
 *
 * Error is only returned if given arguemnts are invalid somehow.
 */
fernos_error_t free_pages(phys_addr_t pd, void *start, void *end);

