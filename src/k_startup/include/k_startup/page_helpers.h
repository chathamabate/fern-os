
#pragma once

#include "k_startup/page.h"
#include "k_sys/page.h"

/*
 * Helper functions to use when paging is enabled.
 */

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
 * Create a deep copy of a page directory.
 *
 * Returns NULL_PHYS_ADDR if there is insufficient memory.
 *
 * Remember, when a page is marked Identity or Shared, said page is not copied. Just the reference
 * to the page is copied. When a page is marked unique, than a full copy is done.
 */
phys_addr_t copy_page_directory(phys_addr_t pd);

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
