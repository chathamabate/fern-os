
#pragma once

#include "k_sys/page.h"
#include "os_defs.h"
#include "s_util/misc.h"
#include "s_util/err.h"
#include <stdint.h>

/*
 * Here are all the section layouts as defined in the linker script.
 *
 * NOTE: end is EXCLUSIVE!
 *
 * The ends is os_defs are INCLUSIVE!
 */

extern const char _ro_shared_start[]; 
extern const char _ro_shared_end[]; 

extern const char _ro_kernel_start[]; 
extern const char _ro_kernel_end[]; 

extern const char _ro_user_start[]; 
extern const char _ro_user_end[]; 

extern const char _bss_shared_start[]; 
extern const char _bss_shared_end[]; 
extern const char _data_shared_start[]; 
extern const char _data_shared_end[]; 

extern const char _bss_kernel_start[]; 
extern const char _bss_kernel_end[]; 
extern const char _data_kernel_start[]; 
extern const char _data_kernel_end[]; 

extern const char _bss_user_start[]; 
extern const char _bss_user_end[]; 
extern const char _data_user_start[]; 
extern const char _data_user_end[]; 

/**
 * These values are used in the available bits of a page table entry. NOTE: They have
 * no meaning for entries in page directories. Other than the kernel page tables, 
 * all page tables will be placed in arbitrary pages! (Not shared or strictly identity)
 *
 * An Identity Entry, is an entry which points to an identity page. This is a page
 * who's physical and virtual addresses are equal. This page should NEVER be added
 * to the free list, and always placed at the correct virtual address.
 *
 * A unique entry points to a page which is only refernced by this page table. If this page table
 * is deleted, the refernced page should be returned to the page free list.
 */

#define IDENTITY_ENTRY (0)
#define UNIQUE_ENTRY   (1)

/**
 * Later we may want to introduce a concept of shared memory, not now tho!
 * (This is one page which is referenced by multiple page tables)
 */
// #define SHARED_ENTRY (2)

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

static inline pt_entry_t fos_identity_pt_entry(phys_addr_t base) {
    pt_entry_t pte = fos_present_pt_entry(base);

    pte_set_avail(&pte, IDENTITY_ENTRY);

    return pte;
}

static inline pt_entry_t fos_unique_pt_entry(phys_addr_t base) {
    pt_entry_t pte = fos_present_pt_entry(base);

    pte_set_avail(&pte, UNIQUE_ENTRY);

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
 * Number of free pages left. (Useful for testing)
 */
uint32_t get_num_free_pages(void);

/**
 * If the given address is blatantly invalid, it will simply be ignored.
 */
void push_free_page(phys_addr_t page_addr);

/**
 * Attempt to pop a free page from the free page linked list.
 *
 * NULL_PHYS_ADDR is returned if there are no pages to pop.
 */
phys_addr_t pop_free_page(void);

/**
 * Create a new page table, all entries will start as not present.
 */
fernos_error_t new_page_table(phys_addr_t *pt_addr);

/**
 * Add new pages to the given page table! (s and e are indecies into a page table, e is exclusive)
 *
 * If an error is returned, it is possible that not all pages were allocated.
 * Always check the `true_e` argument when SUCCESS isn't returned to see how much progress was
 * made in the allocation.
 *
 * If *true_e == e, all pages were allocated!
 */
fernos_error_t pt_add_pages(phys_addr_t pt_addr, uint32_t s, uint32_t e, uint32_t *true_e);

/**
 * Remove pages from a page table. Pages marked UNIQUE will be added back to the free list.
 */
void pt_remove_pages(phys_addr_t pt_addr, uint32_t s, uint32_t e);

/**
 * Delete a page table. 
 */
void delete_page_table(phys_addr_t pt_addr);


/**
 * Create a new default page directory and store it's physical address at pd_addr.
 *
 * NOTE: This is just a directory where all identity page tables are mapped, and all other page
 * entries are marked non-present.
 */
fernos_error_t new_page_directory(phys_addr_t *pd_addr);

/**
 * Add pages (and page table if necessary) to a page directory.
 *
 * If we run out of memory, FOS_NO_MEM is returned.
 * If we run into an already page, FOS_ALREADY_ALLOCATED is returned.
 *
 * Use true_e to determine how many pages were actually allocated in case of error.
 */
fernos_error_t pd_add_pages(phys_addr_t pd_addr, void *s, void *e, void **true_e);

/**
 * Remove all removeable pages from s to e in the given page directory.
 *
 * NOTE: Given addresses must ALWAYS be 4K aligned (This is the case for all functions in the 
 * header file, but still)
 */
void pd_remove_pages(phys_addr_t pd_addr, void *s, void *e);

/**
 * Take the physical address of a page directory and clean up all of it's pages, page tables, and
 * finally, the page directory itself!
 */
void delete_page_directory(phys_addr_t pd_addr);
