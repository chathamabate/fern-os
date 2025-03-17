
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

/**
 * A free page which is used inside the kernel to help with page table modification.
 */
static uint8_t free_page[M_4K] __attribute__ ((aligned(M_4K)));

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

    // By default, just set all identity pages as writeable and present.
    pt_entry_t *ptes = (pt_entry_t *)identity_pts;
    for (uint32_t ptei = 0; ptei < NUM_IDENTITY_PT_ENTRIES; ptei++) {
        pt_entry_t *pte = &(ptes[ptei]); 

        *pte = not_present_pt_entry();

        pte_set_base(pte, ptei * M_4K);
        pte_set_user(pte, 0);
        pte_set_present(pte, 1);
        pte_set_writable(pte, 1);
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

    // The kernel free page is a reserved area which will have an underlying physical page.
    // However, this underlying physical page should never be used. 
    // Instead, it should be swapped out accordingly with arbitrary pages.
    // We give it memory in the kernel so that we know no data/text will be placed there 
    // (Both physically and virtually)
    //
    // Unless being used, this page's PTE in the kernel should always be not present.
    
    const uint32_t free_page_ptei = (uint32_t)free_page / M_4K;
    pt_entry_t *free_page_pte = &(ptes[free_page_ptei]);
    pte_set_present(free_page_pte, 0);


    // Take out NULL page.
    pt_entry_t *null_pte = &(ptes[0]);
    pte_set_present(null_pte, 0);

    return FOS_SUCCESS;
}

static fernos_error_t init_kernel_pd(void) {
    // Set all as non present to start.
    for (uint32_t ptei = 0; ptei < 1024; ptei++) {
        pt_entry_t *pte = &(kernel_pd[ptei]);
        *pte = not_present_pt_entry();
    }

    // Add all identity pages to the kernel directory.
    for (uint32_t i = 0; i < NUM_IDENTITY_PTS; i++) {
        pt_entry_t *pte = &(kernel_pd[i]);
        pt_entry_t *pt = identity_pts[i];

        pte_set_present(pte, 1);
        pte_set_writable(pte, 1);
        pte_set_user(pte, 0);
        pte_set_base(pte, (phys_addr_t)pt);
    }

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
        return FOS_INVALID_RANGE_ERROR;
    }

    // Each page points to the next!
    for (phys_addr_t p = FREE_PAGE_AREA_START; p < FREE_PAGE_AREA_END; p += M_4K) {
        *(phys_addr_t *)p = p + M_4K;
        num_free_pages++;
    }

    // Last page always points to NULL.
    *(phys_addr_t *)(FREE_PAGE_AREA_END - M_4K) = NULL_PHYS_ADDR;

    next_free_page = FREE_PAGE_AREA_START;

    return FOS_SUCCESS;
}

fernos_error_t init_paging(void) {
    fernos_error_t err;

    PROP_ERR(init_identity_pts(), err);
    PROP_ERR(init_kernel_pd(), err);
    PROP_ERR(init_free_page_area(), err);

    set_page_directory((phys_addr_t)kernel_pd);
    enable_paging();

    return FOS_SUCCESS;
}

fernos_error_t push_free_page(phys_addr_t page_addr) {
    CHECK_ALIGN(page_addr, M_4K);

    if (page_addr < IDENTITY_AREA_SIZE) {
        return FOS_BAD_ARGS;
    }
    
    // To push a free page, we must write the address of the next free page into the first
    // 4 bytes of the given page.
    
    pt_entry_t *ptes = (pt_entry_t *)identity_pts;

    const uint32_t free_page_ptei = (uint32_t)free_page / M_4K;
    pt_entry_t *free_page_pte = &(ptes[free_page_ptei]);
    
    pt_entry_t pte = not_present_pt_entry();

    pte_set_present(&pte, 1);
    pte_set_base(&pte, page_addr);
    pte_set_writable(&pte, 1);
    pte_set_user(&pte, 0);

    // Point the kernel free page to the page which is being pushed!
    *free_page_pte = pte;
    flush_page_cache();
    
    // Write next free page into the start bytes of the this new free page.
    *(phys_addr_t *)free_page = next_free_page;

    // Set this new free page as the head of the free list.
    next_free_page = page_addr;
    num_free_pages++;

    // Remove given free page from kernel page table.
    pte_set_present(free_page_pte, 0);
    flush_page_cache();

    return FOS_SUCCESS;
}

fernos_error_t pop_free_page(phys_addr_t *page_addr) {
    if (!page_addr) {
        return FOS_BAD_ARGS;
    }

    if (next_free_page == NULL_PHYS_ADDR) {
        *page_addr = NULL_PHYS_ADDR;
        return FOS_NO_MEM;
    }  

    // Ok now, we need to extract the next free page from the starting bytes of the page being 
    // popped.

    pt_entry_t *ptes = (pt_entry_t *)identity_pts;
    const uint32_t free_page_ptei = (uint32_t)free_page / M_4K;
    pt_entry_t *free_page_pte = &(ptes[free_page_ptei]);

    pt_entry_t pte = not_present_pt_entry();

    pte_set_base(&pte, next_free_page);
    pte_set_user(&pte, 0);
    pte_set_present(&pte, 1);
    pte_set_writable(&pte, 1);

    *free_page_pte = pte;
    flush_page_cache();

    const phys_addr_t free_page_to_return = next_free_page;

    next_free_page = *(phys_addr_t *)free_page;
    num_free_pages--;

    pte_set_present(free_page_pte, 0);
    flush_page_cache();

    *page_addr = free_page_to_return;
    return FOS_SUCCESS;
}

fernos_error_t allocate_pages(pt_entry_t **pd, void *start, void *end, void **true_end) {
    return FOS_SUCCESS;
}

fernos_error_t free_pages(pt_entry_t **pd, void *start, void *end) {
    return FOS_SUCCESS;
}

