
#pragma once

#include "k_startup/page.h"
#include "k_sys/page.h"
#include "s_bridge/app.h"

/*
 * Helper functions to use when paging is enabled.
 */

/**
 * Copy the contents of physical page at source, to physical page at dest.
 */
void page_copy(phys_addr_t dest, phys_addr_t src);

/**
 * Get the physical page mapped at index pi!
 *
 * NULL_PHYS_ADDR if not mapped!
 */
phys_addr_t pd_get_underlying_p(phys_addr_t pd, uint32_t pi);

/**
 * ptr is an address into a memory space described by pd.
 *
 * This returns the physical address of the page referenced by pointer.
 *
 * NOTE: If ptr is not allocated in the memory space, NULL_PHYS_ADDR is returned.
 */
phys_addr_t pd_get_underlying(phys_addr_t pd, const void *ptr);

/**
 * Given the physical address of two page tables, copy page table entries with in range [s, e)
 * from `src_pt` to `dest_pt`. 
 *
 * If a page table entry is shared or identity, just a shallow copy of the entry is performed.
 * If a page table entry is unique, a new page will be allocated and mapped in `dest_pt`.
 * (A deep copy)
 *
 * FOS_E_BAD_ARGS is returned if `dest_pt` is NULL_PHYS_ADDR or `src_pt` is NULL_PHYS_ADDR.
 * FOS_E_INVALID_RANGE is returned if `s` and `e` form an invalid range.
 * FOS_E_NO_MEM is returned if a deep copy of a unique page fails due to lack of pages in the
 * free list.
 * FOS_E_ALREADY_ALLOCATED is returned if an entry within the given range is marked present in
 * `dest_pt`.
 *
 * On failure, `dest_pt` is returned to its initial state before return.
 *
 * On success, FOS_E_SUCCESS is returned.
 */
fernos_error_t pt_copy_range(phys_addr_t dest_pt, phys_addr_t src_pt, uint32_t s, uint32_t e);

/**
 * Create a deep copy of a page table.
 *
 * Returns NULL_PHYS_ADDR if there is insufficient memory.
 */
phys_addr_t copy_page_table(phys_addr_t pt);

/**
 * Deep copy a range of addresses from one page directory to another.
 * (Remember, this means unique pages are given deep copies in the dest_pd, whereas
 * shared and identity pages just have their entries copied over)
 *
 * FOS_E_INVALID_RANGE if `[pi_s, pi_e)` is an invalid range.
 * FOS_E_ALREADY_ALLOCATED if we run into a present entry within `dest_pd`.
 * FOS_E_NO_MEM if we run out of memory during the copy.
 * FOS_E_SUCCESS if the entire range was successfully copied from `src_pd` to `dest_pd`.
 *
 * If this fails, `dest_pd` will be returned to its original state before return.
 */
fernos_error_t pd_copy_range_p(phys_addr_t dest_pd, phys_addr_t src_pd, uint32_t pi_s, uint32_t pi_e);

/**
 * Wrapper around `pd_copy_range_p`.
 *
 * FOS_E_ALIGN_ERROR if `pd`, `s`, or `e` aren't 4K aligned.
 * FOS_E_INVALID_RANGE if `e` < `s`.
 */
fernos_error_t pd_copy_range(phys_addr_t dest_pd, phys_addr_t src_pd, void *s, const void *e);

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

/**
 * Efficiently set the contents of an area within a different userspace.
 *
 * Returns an error if the user dest buffer is not entirely mapped.
 * Returns an error if args are bad.
 *
 * On Success, the entire area has been set.
 * On Error, some or none of the area was set.
 *
 * If `set` is given, the number of bytes set will be written to `*set`. On success, this will
 * always be `bytes`.
 */
fernos_error_t mem_set_to_user(phys_addr_t user_pd, void *user_dest, uint8_t val, uint32_t bytes,
        uint32_t *set);

/**
 * Create a fresh user application memory space!
 * This uses a copy of the initial user page directory as a starting point. This way the user app
 * gets a fresh copy of the s_* data/bss and u_* data/bss.
 *
 * FOS_E_BAD_ARGS if `ua` is NULL or doesn't have at least one loadable area.
 * FOS_E_BAD_ARGS if `abs_ab_len` is greater than the size of the apps argument area.
 * FOS_E_ALIGN_ERROR if any of the loadable areas has a load_position which is NOT 4K aligned.
 * (Note, the size of each area need not be 4K aligned)
 * FOS_E_INVALID_RANGE if any of the loadable areas overlap OR if the entry point is not inside
 * a loaded area OR if any of the loadable areas are outside the FOS APP area.
 *
 * Other error may be returned.
 *
 * On success, FOS_E_SUCCESS is returned, the physical address of the new page directory is written
 * to `*out`.
 *
 * NOTE: `*out` is only written to on success.
 *
 * NOTE: VERY IMPORTANT: `abs_ab` is a pointer to an ABSOLUTE args block with offset
 * FOS_APP_ARGS_AREA_START. This is assumed and not checked. (Also, if `abs_ab_len` is 0, `abs_ab`
 * can be NULL)
 *
 * NOTE: This DOES NOT allocate any thread stacks!! When creating a new process with this memory
 * space, it is your responsibility to allocate a stack!
 */
fernos_error_t new_user_app_pd(const user_app_t *ua, const void *abs_ab, size_t abs_ab_len,
        phys_addr_t *out);

/**
 * Copy a user app which lives in a different memory space into this memory space.
 *
 * This is a deep copy into a NEW user app object here in this space.
 * `al` is used to allocate the new user app object.
 *
 * Returns NULL on error.
 */
user_app_t *ua_copy_from_user(allocator_t *al, phys_addr_t pd, const user_app_t *u_ua);
