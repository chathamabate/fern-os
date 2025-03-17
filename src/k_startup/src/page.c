
#include "k_sys/page.h"

#include "k_startup/page.h"
#include "os_defs.h"
#include "s_util/misc.h"
#include "s_util/err.h"

pt_entry_t identity_pts[NUM_IDENTITY_PTS][1024] __attribute__ ((aligned(M_4K)));
pt_entry_t kernel_pd[1024] __attribute__ ((aligned(M_4K)));

uint8_t free_page[M_4K] __attribute__ ((aligned(M_4K)));

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

    // By default, just set all identity pages as writeable.
    pt_entry_t *ptes = (pt_entry_t *)identity_pts;
    for (uint32_t ptei = 0; ptei < NUM_IDENTITY_PT_ENTRIES; ptei++) {
        pt_entry_t *pte = &(ptes[ptei]); 

        pte_set_base(pte, ptei * M_4K);
        pte_set_user(pte, 0);
        pte_set_present(pte, 1);
        pte_set_writable(pte, 1);
    }

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

    // Take out NULL page.
    pt_entry_t *null_pte = &(ptes[0]);
    pte_set_present(null_pte, 0);

    return FOS_SUCCESS;
}

static fernos_error_t init_kernel_pd(void) {
    // Set all as non present to start.
    for (uint32_t ptei = 0; ptei < 1024; ptei++) {
        pt_entry_t *pte = &(kernel_pd[ptei]);
        pte_set_present(pte, 0);
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

phys_addr_t next_free_page;

static fernos_error_t init_free_page_area(void) {
    next_free_page = NULL_PHYS_ADDR;

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

