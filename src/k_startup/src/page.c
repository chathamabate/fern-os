
#include "k_sys/page.h"

#include "k_startup/page.h"
#include "os_defs.h"
#include "s_util/misc.h"
#include "s_util/err.h"
#include <stdbool.h>
#include <stdlib.h>

/**
 * These are the page tables to always use for addressing into the identity area.
 * Do not change these ever.
 *
 * Also, note that the first pte is always marked to preset so that a NULL derefernce results 
 * in a page fault.
 */
static pt_entry_t identity_pts[NUM_IDENTITY_PTS][1024] __attribute__ ((aligned(M_4K)));

/**
 * The page directory used by the kernel.
 *
 * This can and will be modified to give the kernel access to user task pages
 */
static pt_entry_t kernel_pd[1024] __attribute__ ((aligned(M_4K)));

#define NUM_TMP_FREE_PAGES (3)

/**
 * Free pages which are used inside the kernel to help with page table modification.
 *
 * I am using 3 since, we may need 1 for a page directory, 1 for a page table, and then 1 for
 * a specific data page.
 */
static uint8_t tmp_free_pages[NUM_TMP_FREE_PAGES][M_4K] __attribute__ ((aligned(M_4K)));

/**
 * Can only be accessed after calling `init_free_page_area`.
 * If no free pages are available or an error occured during initialization,
 * this will equal NULL_PHYS_ADDR.
 */
static phys_addr_t next_free_page;

/**
 * The number of free pages in the free page linked list.
 */
static uint32_t num_free_pages;

extern const char _text_start[];
extern const char _text_end[];
 
extern const char _rodata_start[];
extern const char _rodata_end[];

/**
 * Setup the identity_pts array.
 *
 * Returns an error if there was an error with alignments.
 */
static fernos_error_t init_identity_pts(void) {
    CHECK_ALIGN(IDENTITY_AREA_SIZE, M_4M);

    CHECK_ALIGN((phys_addr_t)_text_start, M_4K);
    CHECK_ALIGN((phys_addr_t)_text_end, M_4K);
    CHECK_ALIGN((phys_addr_t)_rodata_start, M_4K);
    CHECK_ALIGN((phys_addr_t)_rodata_end, M_4K);

    // By default, just set all identity pages as whared pages.
    pt_entry_t *ptes = (pt_entry_t *)identity_pts;
    for (uint32_t ptei = 0; ptei < NUM_IDENTITY_PT_ENTRIES; ptei++) {
        ptes[ptei] = fos_shared_pt_entry(ptei * M_4K);
    }

    // NOTE: It has come to my attention that the writeable bit does nothing in
    // supervisor mode... going to leave these loops in here anyway.

    // Set text area as read only.
    const uint32_t text_ptei_start = (phys_addr_t)_text_start / M_4K;
    const uint32_t text_ptei_end = (phys_addr_t)_text_end / M_4K;

    for (uint32_t text_ptei = text_ptei_start; text_ptei < text_ptei_end; text_ptei++) {
        pt_entry_t *text_pte = &(ptes[text_ptei]);
        pte_set_writable(text_pte, 0);
    }

    // Set rodata section as readonly.
    const uint32_t rodata_ptei_start = (phys_addr_t)_rodata_start / M_4K;
    const uint32_t rodata_ptei_end = (phys_addr_t)_rodata_end / M_4K;

    for (uint32_t rodata_ptei = rodata_ptei_start; rodata_ptei < rodata_ptei_end; rodata_ptei++) {
        pt_entry_t *rodata_pte = &(ptes[rodata_ptei]);
        pte_set_writable(rodata_pte, 0);
    }

    // Kernel temp free pages are reserved, not available for general use.
    // They will start as unmapped.

    const uint32_t tmp_ptei_start = (phys_addr_t)tmp_free_pages / M_4K;
    const uint32_t tmp_ptei_end = tmp_ptei_start + NUM_TMP_FREE_PAGES;
    for (uint32_t tmp_ptei = tmp_ptei_start; tmp_ptei < tmp_ptei_end; tmp_ptei++) {
        pt_entry_t *tmp_pte = &(ptes[tmp_ptei]);
        pte_set_present(tmp_pte, 0);
    }

    // Take out NULL page.
    pt_entry_t *null_pte = &(ptes[0]);
    pte_set_present(null_pte, 0);

    return FOS_SUCCESS;
}

/**
 * Factored out helper code for intializing a page directory given we have it's virtual address.
 */
static void _init_pd(pt_entry_t *pd) {
    // Set all as non present to start.
    for (uint32_t pdei = 0; pdei < 1024; pdei++) {
        pt_entry_t *pte = &(pd[pdei]);
        *pte = not_present_pt_entry();
    }

    // Add all identity pages to the directory.
    for (uint32_t i = 0; i < NUM_IDENTITY_PTS; i++) {
        pd[i] = fos_shared_pt_entry((phys_addr_t)(identity_pts[i]));
    }
}

static fernos_error_t init_kernel_pd(void) {
    _init_pd(kernel_pd);
    return FOS_SUCCESS;
}

static fernos_error_t init_free_page_area(void) {
    next_free_page = NULL_PHYS_ADDR;
    num_free_pages = 0;

    CHECK_ALIGN(FREE_PAGE_AREA_START, M_4K);
    CHECK_ALIGN(FREE_PAGE_AREA_END, M_4K);

    if (FREE_PAGE_AREA_START == FREE_PAGE_AREA_END) {
        return FOS_SUCCESS;
    }

    if (FREE_PAGE_AREA_END < FREE_PAGE_AREA_START) {
        return FOS_INVALID_RANGE;
    }

    // Each page points to the next!
    for (phys_addr_t p = FREE_PAGE_AREA_START; p < FREE_PAGE_AREA_END; p += M_4K) {
        *(phys_addr_t *)p = p + M_4K;
    }

    // Last page always points to NULL.
    *(phys_addr_t *)(FREE_PAGE_AREA_END - M_4K) = NULL_PHYS_ADDR;

    next_free_page = FREE_PAGE_AREA_START;
    num_free_pages = (FREE_PAGE_AREA_END - FREE_PAGE_AREA_START) / M_4K;

    return FOS_SUCCESS;
}

fernos_error_t init_paging(void) {
    PROP_ERR(init_identity_pts());
    PROP_ERR(init_kernel_pd());
    PROP_ERR(init_free_page_area());

    set_page_directory((phys_addr_t)kernel_pd);
    enable_paging();

    return FOS_SUCCESS;
}

/**
 * This function maps a static free page to the physical page, page.
 *
 * If old is given, the old physical address mapped at ti will be put into old.
 *
 * If page is the NULL_PHYS_ADDR, the page tmp page will set as not present in the kernel pts.
 *
 * NOTE: This can be a page anywhere, (even in the identity area)
 */
static fernos_error_t assign_tmp_page(uint32_t ti, phys_addr_t page, phys_addr_t *old) {
    fernos_error_t err = FOS_SUCCESS;
    phys_addr_t old_page = NULL_PHYS_ADDR;

    if (err == FOS_SUCCESS && ti >= NUM_TMP_FREE_PAGES) {
        err = FOS_BAD_ARGS;
    }

    if (err == FOS_SUCCESS && !IS_ALIGNED(page, M_4K)) {
        err = FOS_ALIGN_ERROR;
    }

    if (err == FOS_SUCCESS) {
        pt_entry_t *ptes = (pt_entry_t *)identity_pts;
        const uint32_t tmp_page_ptei = (uint32_t)(tmp_free_pages[ti]) / M_4K;  
        pt_entry_t *pte = &(ptes[tmp_page_ptei]); 

        if (pte_get_present(*pte)) {
            old_page = pte_get_base(*pte);
        }

        if (page == NULL_PHYS_ADDR) {
            pte_set_present(pte, 0);
        } else {
            *pte = fos_shared_pt_entry(page); 
        }
    }

    if (old) {
        *old = old_page;
    }

    return err;
}

/**
 * Set the static free page's PTE as not present.
 * 
 * If old is given, 
 */
static fernos_error_t clear_tmp_page(uint32_t ti, phys_addr_t *old) {
    return assign_tmp_page(ti, NULL_PHYS_ADDR, old);
}

fernos_error_t push_free_page(phys_addr_t page_addr) {
    // An identity area page can never be pushed onto the free list.
    if (page_addr < IDENTITY_AREA_SIZE) {
        return FOS_IDENTITY_OVERWRITE;
    }

    // Ok now what, I think we always use ti 0.
    phys_addr_t old_tmp_page = NULL_PHYS_ADDR;

    PROP_ERR(assign_tmp_page(0, page_addr, &old_tmp_page));

    // We can push!
    *(phys_addr_t *)(tmp_free_pages[0]) = next_free_page;

    next_free_page = page_addr;
    num_free_pages++;

    PROP_ERR(assign_tmp_page(0, old_tmp_page, NULL));

    return FOS_SUCCESS;
}

fernos_error_t pop_free_page(phys_addr_t *page_addr) {
    if (!page_addr) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err = FOS_SUCCESS;
    phys_addr_t old_tmp_page = NULL_PHYS_ADDR;
    phys_addr_t popped_free_page = NULL_PHYS_ADDR;

    if (err == FOS_SUCCESS && next_free_page == NULL_PHYS_ADDR) {
        err = FOS_NO_MEM; 
    }

    if (err == FOS_SUCCESS) {
        err = assign_tmp_page(0, popped_free_page, &old_tmp_page);
    }

    if (err == FOS_SUCCESS) {
        // Here we successfully did the pop!

        popped_free_page = next_free_page;

        next_free_page = *(phys_addr_t *)(tmp_free_pages[0]);
        num_free_pages--;


        err = assign_tmp_page(0, old_tmp_page, NULL);
    }

    *page_addr = popped_free_page; 
    return err;
}

fernos_error_t new_page_table(phys_addr_t *pt_addr) {
    if (!pt_addr) {
        return FOS_BAD_ARGS;
    }

    phys_addr_t pt;

    fernos_error_t err = FOS_SUCCESS;

    if (err == FOS_SUCCESS) {
        err = pop_free_page(&pt);
    }

    if (err == FOS_SUCCESS) {
        err = assign_tmp_page(ti, pt);
    }

    if (err == FOS_SUCCESS) {
        pt_entry_t *ptes = (pt_entry_t *)(tmp_free_pages[ti]);
        for (uint32_t i = 0; i < 1024; i++) {
            ptes[i] = not_present_pt_entry(); 
        }
    }

    (void)clear_tmp_page(ti);

    *pt_addr = err == FOS_SUCCESS ? pt : NULL_PHYS_ADDR;

    return err;
}

fernos_error_t delete_page_table(phys_addr_t pt_addr) {
    return FOS_SUCCESS;
}

fernos_error_t new_page_directory(phys_addr_t *pd_addr) {
    return FOS_SUCCESS;
}

fernos_error_t delete_page_directory(phys_addr_t pd_addr) {
    return FOS_SUCCESS;
}

fernos_error_t allocate_pages(phys_addr_t pd, void *start, void *end, void **true_end) {
    CHECK_ALIGN(pd, M_4K);
    CHECK_ALIGN((phys_addr_t)start, M_4K);
    CHECK_ALIGN((phys_addr_t)end, M_4K);

    if (!true_end) {
        return FOS_BAD_ARGS;
    }

    *true_end = start;

    if (start == end) {
        return FOS_SUCCESS;
    }

    if ((uint32_t)end < (uint32_t)start) {
        *true_end = NULL_PHYS_ADDR;
        return FOS_INVALID_RANGE;
    }

    if ((uint32_t)start < IDENTITY_AREA_SIZE) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err = FOS_SUCCESS;

    // Always start by loading the page directory into temp page 0.
    PROP_ERR(assign_tmp_page(0, pd));
    pt_entry_t *page_dir = (pt_entry_t *)(tmp_free_pages[0]);

    // Should we start with some begginning moves??
    
    uint32_t loaded_pdi = 1024;
    pt_entry_t *page_table = NULL;

    const uint32_t pi_start = (uint32_t)start / M_4K;
    const uint32_t pi_end = (uint32_t)end / M_4K;

    uint32_t pi;

    for (pi = pi_start; pi < pi_end; pi++) {
        // This should be converted to a pretty fast bit shift tbh.
        uint32_t pdi = pi / 1024;

        // Must we switch to a new page table?
        if (pdi != loaded_pdi) {
            pt_entry_t *pde = &(page_dir[pdi]);
            
            // Is there a page table allocated for this slot yet??
            if (!pte_get_present(*pde)) {
                phys_addr_t pt;

                err = pop_free_page(&pt);
                if (err != FOS_SUCCESS) {
                    break;
                }

                *pde = not_present_pt_entry();
                pte_set_present(pde, 1);
                pte_set_user(pde, 0);
                pte_set_writable(pde, 1);
                pte_set_base(pde, pt);

                // I don't think wee need to flush the TLB here because while we did modify a page
                // table, this page table is not necessary being used by the kernel's page 
                // directory.
            }

            // When we actually put the page table into the kernel's temp pages, this is when
            // we flush the page cache. (which is done by assign free page)
            err = assign_tmp_page(1, pte_get_base(*pde));
            if (err != FOS_SUCCESS) {
               break;
            }

            loaded_pdi = pdi;
            page_table = (pt_entry_t *)(tmp_free_pages[1]);
        }

        // Also should be a quick mask.
        uint32_t pti = pi % 1024;
        pt_entry_t *pte = &(page_table[pti]);

        if (pte_get_present(*pte)) {
            err = FOS_ALREADY_ALLOCATED;
            break;
        }

        // FINALLY ALLOCATE OUR PAGE!!!

        phys_addr_t p;
        err = pop_free_page(&p);
        if (err != FOS_SUCCESS) {
            break;
        }

        *pte = not_present_pt_entry();

        pte_set_base(pte, p);
        pte_set_present(pte, 1);
        pte_set_user(pte, 0);
        pte_set_writable(pte, 1);
    }

    // Try to clear both temporary free pages, don't worry if there was an error.
    (void)clear_tmp_page(1);
    (void)clear_tmp_page(0);

    // Finally tell the user how far we got!

    *true_end = (void *)(pi * M_4K);
    
    return err;
}

fernos_error_t free_pages(phys_addr_t pd, void *start, void *end) {
    CHECK_ALIGN(pd, M_4K);
    CHECK_ALIGN((phys_addr_t)start, M_4K);
    CHECK_ALIGN((phys_addr_t)end, M_4K);

    if (start == end) {
        return FOS_SUCCESS;
    }

    if ((uint32_t)end < (uint32_t)start) {
        return FOS_INVALID_RANGE;
    }

    if ((uint32_t)start < IDENTITY_AREA_SIZE) {
        return FOS_BAD_ARGS;
    }

    fernos_error_t err = FOS_SUCCESS;

    PROP_ERR(assign_tmp_page(0, pd));
    pt_entry_t *page_dir = (pt_entry_t *)(tmp_free_pages[0]);

    uint32_t loaded_pdi = 1024;
    pt_entry_t *page_table = NULL;

    const uint32_t pi_start = (uint32_t)start / M_4K;
    const uint32_t pi_end = (uint32_t)end / M_4K;

    for (uint32_t pi = pi_start; pi < pi_end; pi++) {

        uint32_t pdi = pi / 1024;
        if (pdi != loaded_pdi) {
            pt_entry_t *pde = &(page_dir[pdi]);

            // If not present, sweet, just skip ahead.
            if (!pte_get_present(*pde)) {
                pi += (1024 - 1); 
                continue;
            }

            // Otherwise, let's load it into temp page 1.
            err = assign_tmp_page(1, pte_get_base(*pde));
            if (err != FOS_SUCCESS) {
                break;
            }  

            loaded_pdi = pdi;
            page_table = (pt_entry_t *)(tmp_free_pages[1]);
        }

        uint32_t pti = pi % 1024;
        pt_entry_t *pte = &(page_table[pti]);

        // Only free our page if needed!
        if (pte_get_present(*pte)) {
            phys_addr_t p = pte_get_base(*pte);
            err = push_free_page(p);
            if (err != FOS_SUCCESS) {
                break;
            }

            pte_set_present(pte, 0);
        }
    }

    (void)clear_tmp_page(1);
    (void)clear_tmp_page(0);

    return err;
}
