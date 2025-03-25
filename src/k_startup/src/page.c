
#include "k_sys/page.h"

#include "k_bios_term/term.h"
#include "k_startup/page.h"
#include "os_defs.h"
#include "s_util/misc.h"
#include "s_util/err.h"
#include "s_util/str.h"
#include <stdbool.h>

/*
 * NOTE: Functions starting with `_` in this file denote functions which can only be
 * called before virtual memory is enabled.
 */

/**
 * After paging init, this will contain the physical address of the initialized kernel
 * page directory.
 */
static phys_addr_t kernel_pd = NULL_PHYS_ADDR;

/**
 * After paging init, this will contain the physical address of the page directory
 * to use for the first user task.
 *
 * Once consumed by the first user task, this field should be set back to NULL.
 */
static phys_addr_t first_user_pd = NULL_PHYS_ADDR;

/**
 * A single reserved page in the kernel to be used for page table editing.
 *
 * Always should be marked Identity, never should be added to the free list.
 */
static uint8_t free_kernel_page[M_4K] __attribute__((aligned(M_4K)));

/**
 * After paging is initialized, these structures will be used to modify what
 * the free_kernel_page actually points to.
 * 
 * NOTE: The page table referenced here will not actually be "identity", this
 * 1024 entry space is reserved and mapped to an arbitrary page which contains
 * the correct page table.
 */
static pt_entry_t free_kernel_page_pt[1024] __attribute__((aligned(M_4K)));
static pt_entry_t *free_kernel_page_pte = NULL;

/**
 * This is just the range to use to set up the initial free list.
 * After initialization, pages outside this range can be added to the free list
 * without consequence. (Just be careful)
 */
#define INITIAL_FREE_AREA_START (DYNAMIC_START) 
#define INITIAL_FREE_AREA_END  (EPILOGUE_START) // Exclusive end.

static phys_addr_t free_list_head = NULL_PHYS_ADDR;
static uint32_t free_list_len = 0;

static fernos_error_t _init_free_list(void) {
    CHECK_ALIGN(INITIAL_FREE_AREA_START, M_4K);
    CHECK_ALIGN(INITIAL_FREE_AREA_END, M_4K);

    if (INITIAL_FREE_AREA_START == INITIAL_FREE_AREA_END) {
        return FOS_SUCCESS;
    }

    if (INITIAL_FREE_AREA_END < INITIAL_FREE_AREA_START) {
        return FOS_INVALID_RANGE;
    }

    for (phys_addr_t iter = INITIAL_FREE_AREA_START; iter < INITIAL_FREE_AREA_END; iter += M_4K) {
        *(phys_addr_t *)iter = iter + M_4K;
    }

    // Set final link to NULL.
    *(phys_addr_t *)(INITIAL_FREE_AREA_END - M_4K) = NULL_PHYS_ADDR;
    
    free_list_head = INITIAL_FREE_AREA_START;
    free_list_len = (INITIAL_FREE_AREA_END - INITIAL_FREE_AREA_START) / M_4K;
    
    return FOS_SUCCESS;
}

/**
 * Must be called after _init_free_list.
 *
 * Pops a free page from the free list before paging is enabled.
 */
static phys_addr_t _pop_free_page(void) {
    if (free_list_head == NULL_PHYS_ADDR) {
        term_put_fmt_s("FREE HEAD: %X\n", free_list_head);
        return NULL_PHYS_ADDR;
    }

    phys_addr_t popped = free_list_head;


    free_list_head = *(phys_addr_t *)free_list_head;
    free_list_len--;

    return popped;
}

/**
 * Make a certain range available within a page table.
 *
 * e is exclusive.
 */
static fernos_error_t _place_range(pt_entry_t *pd, phys_addr_t s, phys_addr_t e, 
        bool identity, bool allocate) {
    if (identity && allocate) {
        return FOS_BAD_ARGS;
    }

    term_put_fmt_s("RANGE %X %X\n", s, e);

    CHECK_ALIGN(pd, M_4K);
    CHECK_ALIGN(s, M_4K);
    CHECK_ALIGN(e, M_4K);

    if (s == e) {
        return FOS_SUCCESS;
    }

    if (e < s) {
        return FOS_INVALID_RANGE;
    }

    uint32_t curr_pdi = 1024;
    pt_entry_t *pt = NULL;

    for (phys_addr_t i = s; i < e; i+= M_4K) {
        uint32_t pi = i / M_4K;

        uint32_t pdi = pi / 1024;

        if (pdi != curr_pdi) {
            pt_entry_t *pde = &(pd[pdi]);

            if (!pte_get_present(*pde)) {
                term_put_s("New PT\n");
                phys_addr_t new_page = _pop_free_page();
                if (new_page == NULL_PHYS_ADDR) {
                    return FOS_NO_MEM;
                }

                pt_entry_t *new_pt = (pt_entry_t *)new_page;
                for (uint32_t ti = 0; ti < 1024; ti++) {
                    new_pt[ti] = not_present_pt_entry(); 
                }

                *pde = fos_present_pt_entry((phys_addr_t)new_pt);
            }
            
            curr_pdi = pdi;
            pt = (pt_entry_t *)pte_get_base(*pde);
        } 

        uint32_t pti = pi % 1024;

        if (pte_get_present(pt[pti])) {
            return FOS_ALREADY_ALLOCATED;
        }

        // This is a little confusing, the point is that, the data a bss sections will start
        // kinda as identity. They're virtual and physical addresses will be the same.
        //
        // However, this is not a constant property. Forked user process will have data and bss
        // sections copied into new pages with arbitrary physical addresses.
        //
        // Just because an entry in an initial pd maps directly to the same physical address,
        // doesn't mean that that entry must be marked identity!

        phys_addr_t page_addr = i;
        if (allocate) {
            page_addr = _pop_free_page();
            if (page_addr == NULL_PHYS_ADDR) {
                return FOS_NO_MEM;
            }
        }


        pt[pti] = identity ? fos_identity_pt_entry(page_addr) : fos_unique_pt_entry(page_addr);
    }

    return FOS_SUCCESS;
}

/**
 * Initialize kernel page directory.
 *
 * _init_free_list must be called before this function!
 * Obviously this should be run before paging is enabled.
 */
static fernos_error_t _init_kernel_pd(void) {
    phys_addr_t kpd = _pop_free_page();
    if (kpd == NULL_PHYS_ADDR) {
        return FOS_NO_MEM;
    }

    // First, 0 out the whole area.
    pt_entry_t *pd = (pt_entry_t *)kpd;
    for (uint32_t pdi = 0; pdi < 1024; pdi++) {
        pd[pdi] = not_present_pt_entry();
    }

    PROP_ERR(_place_range(pd, PROLOGUE_START, PROLOGUE_END + 1, true, false));    

    PROP_ERR(_place_range(pd, (phys_addr_t)_ro_shared_start, (phys_addr_t)_ro_shared_end, true, false));
    PROP_ERR(_place_range(pd, (phys_addr_t)_ro_kernel_start, (phys_addr_t)_ro_kernel_end, true, false));

    PROP_ERR(_place_range(pd, (phys_addr_t)_bss_shared_start, (phys_addr_t)_bss_shared_end, false, false));
    PROP_ERR(_place_range(pd, (phys_addr_t)_data_shared_start, (phys_addr_t)_data_shared_end, false, false));

    PROP_ERR(_place_range(pd, (phys_addr_t)_bss_kernel_start, (phys_addr_t)_bss_kernel_end, false, false));
    PROP_ERR(_place_range(pd, (phys_addr_t)_data_kernel_start, (phys_addr_t)_data_kernel_end, false, false));

    kernel_pd = kpd;
    
    return FOS_SUCCESS;
}

/**
 * Copy contents from one page directory mem space to another.
 *
 * NOTE: This assumes the range of addresses given already exists in both the given
 * page directories!
 */
static fernos_error_t _copy_range(pt_entry_t *dest_pd, const pt_entry_t *src_pd, 
        phys_addr_t s, phys_addr_t e) {
    CHECK_ALIGN(dest_pd, M_4K);
    CHECK_ALIGN(src_pd, M_4K);
    CHECK_ALIGN(s, M_4K);
    CHECK_ALIGN(e, M_4K);

    for (phys_addr_t i = s; i < e; i += M_4K) {
        uint32_t pi = i / M_4K;

        uint32_t pdi = pi / 1024;
        uint32_t pti = pi % 1024;

        pt_entry_t *dest_pt = (pt_entry_t *)pte_get_base(dest_pd[pdi]);
        void *dest_page = (void *)pte_get_base(dest_pt[pti]);

        pt_entry_t *src_pt = (pt_entry_t *)pte_get_base(src_pd[pdi]);
        void *src_page = (void *)pte_get_base(src_pt[pti]);
        
        mem_cpy(dest_page, src_page, M_4K);
    }

    return FOS_SUCCESS;
}

/**
 * _init_kernel_pd must be called before calling this function!
 */
static fernos_error_t _init_first_user_pd(void) {
    phys_addr_t upd = _pop_free_page();
    if (upd == NULL_PHYS_ADDR) {
        return FOS_NO_MEM;
    }

    pt_entry_t *kpd = (pt_entry_t *)kernel_pd;

    // First, 0 out the whole area.
    pt_entry_t *pd = (pt_entry_t *)upd;
    for (uint32_t pdi = 0; pdi < 1024; pdi++) {
        pd[pdi] = not_present_pt_entry();
    }

    PROP_ERR(_place_range(pd, (phys_addr_t)_ro_shared_start, (phys_addr_t)_ro_shared_end, true, false));
    PROP_ERR(_place_range(pd, (phys_addr_t)_ro_user_start, (phys_addr_t)_ro_user_end, true, false));

    // Here, both the kernel and user pds will need their own indepent copies of the shared data
    // and shared bss. So, when making the user space, we allocate new pages in this area.
    // Then, we copy over what is contained in the kernel pages!
    PROP_ERR(_place_range(pd, (phys_addr_t)_bss_shared_start, (phys_addr_t)_bss_shared_end, false, true));
    PROP_ERR(_copy_range(pd, kpd, (phys_addr_t)_bss_shared_start, (phys_addr_t)_bss_shared_end));

    PROP_ERR(_place_range(pd, (phys_addr_t)_data_shared_start, (phys_addr_t)_data_shared_end, false, true));
    PROP_ERR(_copy_range(pd, kpd, (phys_addr_t)_data_shared_start, (phys_addr_t)_data_shared_end));

    PROP_ERR(_place_range(pd, (phys_addr_t)_bss_user_start, (phys_addr_t)_bss_user_end, false, false));
    PROP_ERR(_place_range(pd, (phys_addr_t)_data_user_start, (phys_addr_t)_data_user_end, false, false));

    first_user_pd = upd;

    return FOS_SUCCESS;
}

fernos_error_t init_paging(void) {
    PROP_ERR(_init_free_list());
    PROP_ERR(_init_kernel_pd());
    //PROP_ERR(_init_first_user_pd());

    set_page_directory(kernel_pd);
    //enable_paging();

    pt_entry_t *pd = (pt_entry_t *)kernel_pd;
    pt_entry_t *pt = (pt_entry_t *)pte_get_base(pd[1]);
    for (uint32_t i = 0; i < 8; i++) {
        term_put_fmt_s("BASE: %X\n", pte_get_base(pt[i]));
    }

    return FOS_SUCCESS;
}

uint32_t get_num_free_pages(void) {
    return free_list_len;
}


