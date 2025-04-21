
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

extern uint8_t _init_kstack_start[];
extern uint8_t _init_kstack_end[];


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
 *
 * NOTE: In my pt_entry helpers below, I take a writeable bool. Since I am always running in 
 * supervisor role, this option is pointless. However, I will leave it in to remind the user
 * that certain pages shouldn't be written to.
 */

#define IDENTITY_ENTRY (0)
#define UNIQUE_ENTRY   (1)
#define SHARED_ENTRY   (2)

/**
 * Later we may want to introduce a concept of shared memory, not now tho!
 * (This is one page which is referenced by multiple page tables)
 */

static inline pt_entry_t fos_present_pt_entry(phys_addr_t base, bool writeable) {
    pt_entry_t pte;

    // I consider this FOS specific because FOS doesn't use privilege levels.
    // All pages are ring 0.

    pte_set_present(&pte, 1);
    pte_set_base(&pte, base);
    pte_set_user(&pte, 0);
    pte_set_writable(&pte, writeable ? 1 : 0);

    return pte;
}

static inline pt_entry_t fos_identity_pt_entry(phys_addr_t base, bool writeable) {
    pt_entry_t pte = fos_present_pt_entry(base, writeable);

    pte_set_avail(&pte, IDENTITY_ENTRY);

    return pte;
}

static inline pt_entry_t fos_unique_pt_entry(phys_addr_t base, bool writeable) {
    pt_entry_t pte = fos_present_pt_entry(base, writeable);

    pte_set_avail(&pte, UNIQUE_ENTRY);

    return pte;
}

static inline pt_entry_t fos_shared_pt_entry(phys_addr_t base, bool writeable) {
    pt_entry_t pte = fos_present_pt_entry(base, writeable);

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
 * Get the physical address of the kernel page table.
 */
phys_addr_t get_kernel_pd(void);

/**
 * When paging is initialized, a memory space is set up to be used by the very first user process.
 * To switch into this space, you just need the page directory physical address, and the 
 * stack pointer to start at.
 *
 * This function gives those. 
 *
 * NOTE: This function is only meant to be used once. An error will be returned if you try to
 * call this function two times.
 */
fernos_error_t pop_initial_user_info(phys_addr_t *upd, const uint32_t **uesp);

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
 * Create a new page directory. Returns NULL_PHYS_ADDR on error.
 *
 * NOTE: This just a page directory with no filled entries.
 */
phys_addr_t new_page_directory(void);

/**
 * Create a deep copy of a page directory.
 *
 * Returns NULL_PHYS_ADDR if there is insufficient memory.
 *
 * Remember, when a page is marked Identity or Shared, said page is not copied. Just the reference
 * to the page is copied. When a page is marked unique, than a full copy is done.
 */
phys_addr_t copy_page_directory(phys_addr_t pd);

/**
 * Add pages (and page table if necessary) to a page directory.
 * The pages will always be marked as writeable.
 *
 * If we run out of memory, FOS_NO_MEM is returned.
 * If we run into an already page, FOS_ALREADY_ALLOCATED is returned.
 *
 * Use true_e to determine how many pages were actually allocated in case of error.
 */
fernos_error_t pd_alloc_pages(phys_addr_t pd, void *s, const void *e, const void **true_e);

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
    return pd_alloc_pages(get_page_directory(), s, e, true_e); 
}

static inline void free_pages(void *s, const void *e) {
    pd_free_pages(get_page_directory(), s, e);
}

/**
 * Copy the contents of physical page at source, to physical page at dest.
 */
void page_copy(phys_addr_t dest, phys_addr_t src);

/**
 * ptr is an address into a memory space described by pd.
 *
 * This returns the physical address of the page referenced by pointer.
 *
 * NOTE: If ptr is not allocated in the memory space, NULL_PHYS_ADDR is returned.
 */
phys_addr_t get_underlying_page(phys_addr_t pd, const void *ptr);

/**
 * Copy the contents from a buffer in a different memory space, to a buffer in this memory space.
 *
 * Returns an error if the user src buffer is not entirely mapped.
 * Returns an error if arguments are bad.
 *
 * If `copied` is given, writes the number of successfully copied bytes to *copied.
 * On Success, *copied will always equal bytes.
 */
fernos_error_t mem_cpy_from_user(void *dest, phys_addr_t user_pd, const void *user_src, 
        uint32_t bytes, uint32_t *copied);

/**
 * Copy the contents of a buffer in this memory space to a buffer in a different memory space.
 *
 * Returns an error if the user dest buffer is not entirely mapped.
 * Returns an error if args are bad.
 *
 * If `copied` is given, writes the number of successfully copied bytes to *copied.
 * On Success, *copied will always equal bytes.
 */
fernos_error_t mem_cpy_to_user(phys_addr_t user_pd, void *user_dest, const void *src, 
        uint32_t bytes, uint32_t *copied);
