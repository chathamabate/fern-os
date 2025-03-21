
#include "k_sys/page.h"

#include "k_startup/page.h"
#include "os_defs.h"
#include "s_util/misc.h"
#include "s_util/err.h"
#include <stdbool.h>

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
        pd[pdei] = not_present_pt_entry();
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

uint32_t get_num_free_pages(void) {
    return num_free_pages;
}

/**
 * This function maps a static free page to the physical page, page.
 *
 * Returns the page which is currently assigned to said temp free page.
 *
 * If page is the NULL_PHYS_ADDR, the page tmp page will set as not present in the kernel pts.
 *
 * NOTE: This can be a page anywhere, (even in the identity area)
 *
 * NOTE: This does no error checking by design... Use it carefully!
 */
static phys_addr_t assign_tmp_page(uint32_t ti, phys_addr_t page) {
    phys_addr_t old_page = NULL_PHYS_ADDR;

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

    flush_page_cache();

    return old_page;
}

/**
 * Set the static free page's PTE as not present.
 */
/*
static phys_addr_t clear_tmp_page(uint32_t ti) {
    return assign_tmp_page(ti, NULL_PHYS_ADDR);
}
*/

void push_free_page(phys_addr_t page_addr) {
    if (!IS_ALIGNED(page_addr, M_4K)) {
        return;
    }

    // An identity area page can never be pushed onto the free list.
    if (page_addr < IDENTITY_AREA_SIZE) {
        return;
    }

    // Ok now what, I think we always use ti 0.
    phys_addr_t old_tmp_page = assign_tmp_page(0, page_addr);

    // We can push!
    *(phys_addr_t *)(tmp_free_pages[0]) = next_free_page;

    next_free_page = page_addr;
    num_free_pages++;

    assign_tmp_page(0, old_tmp_page);
}

phys_addr_t pop_free_page(void) {
    if (next_free_page == NULL_PHYS_ADDR) {
        return NULL_PHYS_ADDR;
    }

    phys_addr_t old_tmp_page = assign_tmp_page(0, next_free_page);

    phys_addr_t popped_free_page = next_free_page;
    next_free_page = *(phys_addr_t *)(tmp_free_pages[0]);

    num_free_pages--;

    assign_tmp_page(0, old_tmp_page);

    return popped_free_page;
}

fernos_error_t new_page_table(phys_addr_t *pt_addr) {
    if (!pt_addr) {
        return FOS_BAD_ARGS;
    }

    phys_addr_t pt = pop_free_page();
    if (pt == NULL_PHYS_ADDR) {
        return FOS_NO_MEM;
    }

    phys_addr_t old_tmp_page = assign_tmp_page(0, pt);

    pt_entry_t *ptes = (pt_entry_t *)(tmp_free_pages[0]);
    for (uint32_t i = 0; i < 1024; i++) {
        ptes[i] = not_present_pt_entry(); 

    }

    assign_tmp_page(0, old_tmp_page);

    *pt_addr = pt;

    return FOS_SUCCESS;
}

fernos_error_t pt_add_pages(phys_addr_t pt_addr, uint32_t s, uint32_t e, uint32_t *true_e) {
    CHECK_ALIGN(pt_addr, M_4K);

    if (!true_e) {
        return FOS_BAD_ARGS;
    }

    if (s > 1023 || e > 1024 || s > e) {
        return FOS_INVALID_RANGE;
    }

    if (s == e) {
        *true_e = e;
        return FOS_SUCCESS;
    }

    fernos_error_t err = FOS_SUCCESS;

    phys_addr_t old_tmp_page = assign_tmp_page(0, pt_addr);

    pt_entry_t *ptes = (pt_entry_t *)(tmp_free_pages[0]);

    uint32_t i;
    for (i = s; i < e; i++) {
        pt_entry_t *pte = &(ptes[i]);     
        if (pte_get_present(*pte)) {
            err = FOS_ALREADY_ALLOCATED;
            break; // We must break before the incrememnt.
        }

        phys_addr_t new_page = pop_free_page();
        if (new_page == NULL_PHYS_ADDR) {
            err = FOS_NO_MEM;
            break;
        }

        *pte = fos_unique_pt_entry(new_page);
    }

    assign_tmp_page(0, old_tmp_page);

    *true_e = i;
    return err;
}

void pt_remove_pages(phys_addr_t pt_addr, uint32_t s, uint32_t e) {
    if (!IS_ALIGNED(pt_addr, M_4K)) {
        return;
    }

    if (s > 1023 || e > 1024 || s > e) {
        return;
    }

    if (s == e) {
        return;
    }

    phys_addr_t old_tmp_addr = assign_tmp_page(0, pt_addr);

    pt_entry_t *ptes = (pt_entry_t *)(tmp_free_pages[0]);

    uint32_t i;
    for (i = s; i < e; i++) {
        pt_entry_t *pte = &(ptes[i]);

        if (pte_get_present(*pte) && pte_get_avail(*pte) == UNIQUE_ENTRY) {
            phys_addr_t base = pte_get_base(*pte);

            // If for some reason base is < IDENTITY_AREA_SIZE, just don't push that page as free.
            if (base >= IDENTITY_AREA_SIZE) {
                // It should be impossible for this call to fail, Don't worry about its error.
                push_free_page(base);
            }

            pte_set_present(pte, 0);
        } 
    }

    assign_tmp_page(0, old_tmp_addr);
}

void delete_page_table(phys_addr_t pt_addr) {
    if (!IS_ALIGNED(pt_addr, M_4K)) {
        return;
    }

    if (pt_addr < IDENTITY_AREA_SIZE) {
        return;
    }

    pt_remove_pages(pt_addr, 0, 1024);
    push_free_page(pt_addr);
}

// Oops, I think I delete new page directory??
fernos_error_t new_page_directory(phys_addr_t *pd_addr) {
    if (!pd_addr) {
        return FOS_BAD_ARGS;
    }

    phys_addr_t pd = pop_free_page();

    if (pd == NULL_PHYS_ADDR) {
        *pd_addr = pd;
        return FOS_NO_MEM;
    }

    phys_addr_t old_tmp_page = assign_tmp_page(0, pd);

    _init_pd((pt_entry_t *)(tmp_free_pages[0]));

    assign_tmp_page(0, old_tmp_page);

    *pd_addr = pd;
    return FOS_SUCCESS;
}

fernos_error_t pd_add_pages(phys_addr_t pd_addr, void *s, void *e, void **true_e) {
    CHECK_ALIGN(pd_addr, M_4K);
    CHECK_ALIGN((uint32_t)s, M_4K);
    CHECK_ALIGN((uint32_t)e, M_4K);

    if (!true_e) {
        return FOS_BAD_ARGS;
    }

    if (s == e) {
        *true_e = e;
        return FOS_SUCCESS;
    }

    if ((uint32_t)s > (uint32_t)e) {
        *true_e = NULL_PHYS_ADDR;
        return FOS_INVALID_RANGE;
    }

    fernos_error_t err = FOS_SUCCESS; 

    phys_addr_t old_tmp_page = assign_tmp_page(0, pd_addr);

    pt_entry_t *pdes = (pt_entry_t *)(tmp_free_pages[0]);

    uint32_t is = ((uint32_t)s / M_4K);
    uint32_t ie = ((uint32_t)e / M_4K);

    uint32_t i = is;
    uint32_t be = i + (1024 - (i % 1024)); // batch end.

    while (i < ie) {
        uint32_t pdi = i / 1024;
        pt_entry_t *pde = &(pdes[pdi]);

        if (be > ie) {
            be = ie;
        }

        if (!pte_get_present(*pde)) {
            // If necessary, attempt to add a new page table to our pd.
            phys_addr_t pt_addr; 
            err = new_page_table(&pt_addr);
            if (err != FOS_SUCCESS) {
                break; 
            }

            *pde = fos_unique_pt_entry(pt_addr);
        }

        // We don't allow allocating into a shared page table!
        if (pte_get_avail(*pde) == SHARED_ENTRY) {
            err = FOS_ALREADY_ALLOCATED;
            break;
        }

        // Now, do the allocation! (alloc start)
        uint32_t as = i % 1024;

        // OK, because be is always greater than i. (alloc end)
        uint32_t ae = be % 1024;
        if (be == 0) {
            ae = 1024;
        }

        uint32_t te; // (true end)
        err = pt_add_pages(pte_get_base(*pde), as, ae, &te);
        if (err != FOS_SUCCESS) {
            i += (te - as); // Add the number of pages which were successfully added.
            break;
        }
       
        i = be;
        be += 1024;
    }

    assign_tmp_page(0, old_tmp_page);

    *true_e = (void *)(i * M_4K);

    return FOS_SUCCESS;
}

void pd_remove_pages(phys_addr_t pd_addr, void *s, void *e) {
    if (!IS_ALIGNED(pd_addr, M_4K)) {
        return;
    }

    if (!IS_ALIGNED((uint32_t)s, M_4K)) {
        return;
    }

    if (!IS_ALIGNED((uint32_t)e, M_4K)) {
        return;
    }

    if (s == e) {
        return;
    }

    if ((uint32_t)s > (uint32_t)e) {
        return;
    }


    phys_addr_t old_tmp_page = assign_tmp_page(0, pd_addr);
    pt_entry_t *pdes = (pt_entry_t *)(tmp_free_pages[0]);

    uint32_t is = (uint32_t)s / M_4K;
    uint32_t ie = (uint32_t)e / M_4K;

    uint32_t i = is;

    // The idea is that we will delete in "batches".
    // No batch being larger than 4MB.
    //
    // Start by setting be to the end of the first batch!
    uint32_t be = i + (1024 - (i % 1024));

    while (i < ie) {
        uint32_t pdi = i / 1024;
        pt_entry_t *pde = &(pdes[pdi]);

        // Did we overshoot the actual end?
        if (be > ie) {
            be = ie;
        }

        if (pte_get_present(*pde) && pte_get_avail(*pde) != SHARED_ENTRY) {
            phys_addr_t pt_addr = pte_get_base(*pde);

            // Start of removal with in the pt.
            uint32_t rs = i % 1024;

            // end of removal within the pt (exclusive)

            // We know be > i, so this is ok.
            uint32_t re = be % 1024;
            if (re == 0) {
                re = 1024; // Because removal is exclusive
            }

            if (rs == 0 && re == 1024) {
                // Are we deleting the full page table??

                delete_page_table(pt_addr);
                pte_set_present(pde, 0);
            } else {
                // Otherwise, just remove a set of pages!

                pt_remove_pages(pt_addr, rs, re);        
            }
        }

        // Advance to next batch.
        i = be;
        be += 1024;
    }

    assign_tmp_page(0, old_tmp_page);
}

void delete_page_directory(phys_addr_t pd_addr) {
    if (!IS_ALIGNED(pd_addr, M_4K)) {
        return;
    }

    if (pd_addr < IDENTITY_AREA_SIZE) {
        return;
    }

    phys_addr_t old_temp_page = assign_tmp_page(0, pd_addr);
    pt_entry_t *pdes = (pt_entry_t *)(tmp_free_pages[0]);

    for (uint32_t i = 0; i < 1024; i++) {
        // Basically just delete all non-shared page tables in this page directory.
        pt_entry_t pde = pdes[i];
        if (pte_get_present(pde) && pte_get_avail(pde) != SHARED_ENTRY) {
            delete_page_table(pte_get_base(pde));
        }
    }

    assign_tmp_page(0, old_temp_page);

    // Finally, through this bad boy on the free list.
    push_free_page(pd_addr);
}
