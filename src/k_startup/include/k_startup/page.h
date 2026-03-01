
#pragma once

#include "k_sys/page.h"
#include "os_defs.h"
#include "s_util/misc.h"
#include "s_util/err.h"
#include <stdbool.h>
#include <stdint.h>

/*
 * Here are all the section layouts as defined in the linker script.
 *
 * NOTE: end is EXCLUSIVE!
 *
 * The ends is os_defs are INCLUSIVE!
 */

extern uint8_t _sys_tables_start[];
extern uint8_t _sys_tables_end[];

extern uint8_t _ro_shared_start[]; 
extern uint8_t _ro_shared_end[]; 

extern uint8_t _ro_kernel_start[]; 
extern uint8_t _ro_kernel_end[]; 

extern uint8_t _ro_user_start[]; 
extern uint8_t _ro_user_end[]; 

extern uint8_t _bss_shared_start[]; 
extern uint8_t _bss_shared_end[]; 
extern uint8_t _data_shared_start[]; 
extern uint8_t _data_shared_end[]; 

extern uint8_t _bss_kernel_start[]; 
extern uint8_t _bss_kernel_end[]; 
extern uint8_t _data_kernel_start[]; 
extern uint8_t _data_kernel_end[]; 

extern uint8_t _bss_user_start[]; 
extern uint8_t _bss_user_end[]; 
extern uint8_t _data_user_start[]; 
extern uint8_t _data_user_end[]; 

extern uint8_t _static_area_end[];


/**
 * These values are used in the available bits of a page table entry. NOTE: They have
 * no meaning for entries in page directories. Other than the kernel page tables, 
 * all page tables will be placed in arbitrary pages! (Not shared or strictly identity)
 *
 * An Identity Entry, is an entry which points to an identity page. This is a page
 * who's physical and virtual addresses are always equal. This page should NEVER be added
 * to the free list, and always placed at the correct virtual address.
 *
 * A unique entry points to a page which is only refernced by this page table. If this page table
 * is deleted, the refernced page should be returned to the page free list.
 *
 * A shared entry points to a non-identity page which is referenced by many page tables.
 * These functions offer little support for shared entries. It is the job the kernel to manage
 * such pages correctly.
 */

#define IDENTITY_ENTRY (0)
#define UNIQUE_ENTRY   (1)
#define SHARED_ENTRY   (2)

static inline pt_entry_t fos_present_pt_entry(phys_addr_t base, bool user, bool writeable) {
    pt_entry_t pte = not_present_pt_entry();

    // I consider this FOS specific because FOS doesn't use privilege levels.
    // All pages are ring 0. (UGH, now this must change???) MID!!!

    pte_set_present(&pte, 1);
    pte_set_base(&pte, base);
    pte_set_user(&pte, user ? 1 : 0);
    pte_set_writable(&pte, writeable ? 1 : 0);

    return pte;
}

static inline pt_entry_t fos_identity_pt_entry(phys_addr_t base, bool user, bool writeable) {
    pt_entry_t pte = fos_present_pt_entry(base, user, writeable);

    pte_set_avail(&pte, IDENTITY_ENTRY);

    return pte;
}

static inline pt_entry_t fos_unique_pt_entry(phys_addr_t base, bool user, bool writeable) {
    pt_entry_t pte = fos_present_pt_entry(base, user, writeable);

    pte_set_avail(&pte, UNIQUE_ENTRY);

    return pte;
}

static inline pt_entry_t fos_shared_pt_entry(phys_addr_t base, bool user, bool writeable) {
    pt_entry_t pte = fos_present_pt_entry(base, user, writeable);

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
 * This function assumes that all values in constraints.h area valid.
 *
 * NOTE: The first page directory used will be `kernel_pd`.
 */
fernos_error_t init_paging(void);

/**
 * Get the physical address of the kernel page table.
 */
phys_addr_t get_kernel_pd(void);

/**
 * When paging is initialized, a memory space is set up as a template for all user processes.
 *
 * The page directory of this memory space is always private. However, using this function,
 * a copy can be returned. Returns NULL_PHYS_ADDR if the copy fails.
 *
 * NOTE: The copy has NO pages allocated in the user stack area.
 */
phys_addr_t pop_initial_user_pd_copy(void);

/**
 * The number of free kernel pages exposed. MUST BE A POWER OF 2.
 */
#define NUM_FREE_KERNEL_PAGES (4)

extern uint8_t free_kernel_pages[NUM_FREE_KERNEL_PAGES][M_4K];

/**
 * This asigns the kernel page at index slot, to the page at physical address p.
 * returns whatever page was previously in said slot.
 */
phys_addr_t assign_free_page(uint32_t slot, phys_addr_t p);

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
 * Create a new page table. Returns NULL_PHYS_ADDR on error.
 */
phys_addr_t new_page_table(void);

/**
 * Delete a page table.
 *
 * This returns all UNIQUE pages pointed to by the table to the free list.
 */
void delete_page_table(phys_addr_t pt);

/**
 * Allocate a range within a page table.
 *
 * NOTE: if shared is `true` all entries added will be "shared" entries.
 * When `pd` is deleted, such entries will NOT be deleted! (Extra management is required!)
 *
 * Returns FOS_E_ALREADY_ALLOCATED if we hit an entry which is already allocated.
 * Returns FOS_E_NO_MEM if we run out of physical pages.
 *
 * e is the exlusive end of the range. (0 <= s < 1024), (0 <= e <= 1024)
 * The final index reached during allocation is always written to *true_e.
 * On success, *true_e will always equal e.
 */
fernos_error_t pt_alloc_range(phys_addr_t pt, bool user, bool shared, uint32_t s, uint32_t e, uint32_t *true_e);

/**
 * Free a range within a page table.
 * Pages marked UNIQUE are returned to the free list.
 * s and e follow the same rules as described in pt_alloc_range.
 */
void pt_free_range(phys_addr_t pt, uint32_t s, uint32_t e);

/**
 * Create a new page directory. Returns NULL_PHYS_ADDR on error.
 *
 * NOTE: This just a page directory with no filled entries.
 */
phys_addr_t new_page_directory(void);

/**
 * Add pages (and page table if necessary) to a page directory.
 * The pages will always be marked as writeable. 
 *
 * FOS_E_ALIGN_ERROR if `pd`, `s`, or `e` aren't 4K aligned.
 * FOS_E_BAD_ARGS if `true_e` is NULL.
 * FOS_E_INVALID_RANGE if `e` < `s`.
 *
 * NOTE: if shared is `true` all entries added will be "shared" entries.
 * When `pd` is deleted, such entries will NOT be deleted! (Extra management is required!)
 *
 * Otherwise, pages are attempted to be allocated.
 * If all pages are allocated, FOS_E_SUCCESS is returned.
 * If we run out of memory, FOS_E_NO_MEM is returned.
 * If we run into an already page, FOS_E_ALREADY_ALLOCATED is returned.
 * In these three cases, the true end of the allocated section is written to `*true_e`.
 */
fernos_error_t pd_alloc_pages(phys_addr_t pd, bool user, bool shared, void *s, const void *e, const void **true_e);

/*
 * NOTE neither pd_free_pages nor delete_page_directory clean up shared pages.
 *
 * If you want your kernel to support pages shared between spaces you must write your own deletion
 * logic before calling these functions.
 */

/**
 * Remove all removeable pages from s to e in the given page directory.
 *
 * NOTE: Given addresses must ALWAYS be 4K aligned (This is the case for all functions in the 
 * header file, but still)
 */
void pd_free_pages(phys_addr_t pd, void *s, const void *e);

/**
 * Take the physical address of a page directory and clean up all of it's pages, page tables, and
 * finally, the page directory itself!
 */
void delete_page_directory(phys_addr_t pd);

static inline fernos_error_t alloc_pages(void *s, const void *e, const void **true_e) {
    return pd_alloc_pages(get_kernel_pd(), false, false, s, e, true_e); 
}

static inline void free_pages(void *s, const void *e) {
    pd_free_pages(get_kernel_pd(), s, e);
}

