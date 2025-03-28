
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
#define INITIAL_FREE_AREA_START ((phys_addr_t)_static_area_end) 
#define INITIAL_FREE_AREA_END  ((phys_addr_t)_init_kstack_start) // Exclusive end.

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

        // Must we access a different entry in the given page directory?
        if (pdi != curr_pdi) {
            pt_entry_t *pde = &(pd[pdi]);

            // Must we allocate a new page table?
            if (!pte_get_present(*pde)) {
                phys_addr_t new_page = _pop_free_page();
                if (new_page == NULL_PHYS_ADDR) {
                    return FOS_NO_MEM;
                }

                pt_entry_t *new_pt = (pt_entry_t *)new_page;
                clear_page_table(new_pt);

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
    clear_page_table(pd);

    PROP_ERR(_place_range(pd, PROLOGUE_START + M_4K, PROLOGUE_END + 1, true, false));    
    PROP_ERR(_place_range(pd, (phys_addr_t)_ro_shared_start, (phys_addr_t)_ro_shared_end, true, false));
    PROP_ERR(_place_range(pd, (phys_addr_t)_ro_kernel_start, (phys_addr_t)_ro_kernel_end, true, false));

    PROP_ERR(_place_range(pd, (phys_addr_t)_bss_shared_start, (phys_addr_t)_bss_shared_end, false, false));
    PROP_ERR(_place_range(pd, (phys_addr_t)_data_shared_start, (phys_addr_t)_data_shared_end, false, false));

    PROP_ERR(_place_range(pd, (phys_addr_t)_bss_kernel_start, (phys_addr_t)_bss_kernel_end, false, false));
    PROP_ERR(_place_range(pd, (phys_addr_t)_data_kernel_start, (phys_addr_t)_data_kernel_end, false, false));

    PROP_ERR(_place_range(pd, (phys_addr_t)_init_kstack_start, (phys_addr_t)_init_kstack_end, false, false));

    // Now setup up the free kernel page!
    // THIS IS VERY CONFUSING SADLY!
    phys_addr_t fkp = (phys_addr_t)free_kernel_page;

    uint32_t fkp_pi = fkp / M_4K;
    uint32_t fkp_pdi = fkp_pi / 1024;
    uint32_t fkp_pti = fkp_pi % 1024;

    // Let's get the page table which references fkp.
    pt_entry_t *pte = &(pd[fkp_pdi]);
    if (!pte_get_present(*pte)) {
        return FOS_UNKNWON_ERROR;
    }

    // Ok, this page table will contain the entry for the free kernel page!
    pt_entry_t *fkp_pt = (pt_entry_t *)pte_get_base(*pte);

    // Start by setting the free page as not present!
    fkp_pt[fkp_pti] = not_present_pt_entry();

    phys_addr_t fkp_pt_alias = (phys_addr_t)free_kernel_page_pt;

    uint32_t fkp_pt_alias_pi = fkp_pt_alias / M_4K;
    uint32_t fkp_pt_alias_pdi = fkp_pt_alias_pi / 1024;
    uint32_t fkp_pt_alias_pti = fkp_pt_alias_pi % 1024;
    
    // Now alias the fkp page table 
    
    pte = &(pd[fkp_pt_alias_pdi]);
    if (!pte_get_present(*pte)) {
        return FOS_UNKNWON_ERROR;
    }

    pt_entry_t *fkp_pt_alias_pt = (pt_entry_t *)pte_get_base(*pte);
    fkp_pt_alias_pt[fkp_pt_alias_pti] = fos_unique_pt_entry((phys_addr_t)fkp_pt);

    free_kernel_page_pte = &(free_kernel_page_pt[fkp_pti]);

    kernel_pd = kpd;
    
    return FOS_SUCCESS;
}

/**
 * Copy contents from one page directory mem space to another.
 *
 * NOTE: This assumes the range of addresses given already exists in both the given
 * page directories!
 */
/*
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
*/

/**
 * _init_kernel_pd must be called before calling this function!
 */
/*
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
*/

fernos_error_t init_paging(void) {
    PROP_ERR(_init_free_list());
    PROP_ERR(_init_kernel_pd());
    //PROP_ERR(_init_first_user_pd());

    set_page_directory(kernel_pd);
    enable_paging();

    return FOS_SUCCESS;
}

/* NOW PAGING HAS BEEN ENABLED */

uint32_t get_num_free_pages(void) {
    return free_list_len;
}

static phys_addr_t assign_free_page(phys_addr_t p) {
    phys_addr_t ret = NULL_PHYS_ADDR;
    if (pte_get_present(*free_kernel_page_pte)) {
        ret = pte_get_base(*free_kernel_page_pte); 
    }

    *free_kernel_page_pte = fos_unique_pt_entry(p);

    flush_page_cache();

    return ret;
}

void push_free_page(phys_addr_t page_addr) {
    if (!(IS_ALIGNED(page_addr, M_4K))) {
        return;
    }
    
    phys_addr_t old = assign_free_page(page_addr);
    *(phys_addr_t *)free_kernel_page = free_list_head;
    assign_free_page(old);

    free_list_head = page_addr;
    free_list_len++; 
}

phys_addr_t pop_free_page(void) {
    if (free_list_head == NULL_PHYS_ADDR) {
        return NULL_PHYS_ADDR; 
    }

    phys_addr_t old = assign_free_page(free_list_head);
    phys_addr_t next = *(phys_addr_t *)free_kernel_page;
    assign_free_page(old);

    phys_addr_t ret = free_list_head;

    free_list_head = next;
    free_list_len--;

    return ret;
}

static phys_addr_t new_page_table(void) {
    phys_addr_t pt = pop_free_page();

    if (pt != NULL_PHYS_ADDR) {
        phys_addr_t old = assign_free_page(pt);
        clear_page_table((pt_entry_t *)free_kernel_page);
        assign_free_page(old);
    }

    return pt;
}

static fernos_error_t pt_alloc_range(phys_addr_t pt, uint32_t s, uint32_t e, uint32_t *true_e) {
    CHECK_ALIGN(pt, M_4K);

    if (!true_e) {
        return FOS_BAD_ARGS;
    }

    if (s == e) {
        *true_e = e;
        return FOS_SUCCESS;
    }

    if (s > 1023 || e > 1024 || e < s) {
        return FOS_INVALID_RANGE;
    }

    fernos_error_t err = FOS_SUCCESS;
    uint32_t i;

    phys_addr_t old = assign_free_page(pt);

    pt_entry_t *ptes = (pt_entry_t *)free_kernel_page;

    for (i = s; i < e; i++) {
        pt_entry_t *pte = &(ptes[i]);

        if (pte_get_present(*pte)) {
            err = FOS_ALREADY_ALLOCATED;
            break;
        }

        phys_addr_t new_page = pop_free_page();
        if (new_page == NULL_PHYS_ADDR) {
            err = FOS_NO_MEM;
            break;
        }

        *pte = fos_unique_pt_entry(new_page);
    }

    assign_free_page(old);

    *true_e = i;

    return err;
}

static void pt_free_range(phys_addr_t pt, uint32_t s, uint32_t e) {
    if (!IS_ALIGNED(pt, M_4K)) {
        return;
    }

    if (s == e) {
        return;
    }

    if (s > 1023 || e > 1024 || e < s) {
        return;
    }

    phys_addr_t old = assign_free_page(pt);

    pt_entry_t *ptes = (pt_entry_t *)free_kernel_page;

    for (uint32_t i = s; i < e; i++) {
        pt_entry_t *pte = &(ptes[i]);

        if (pte_get_present(*pte) && pte_get_avail(*pte) == UNIQUE_ENTRY) {
            phys_addr_t base = pte_get_base(*pte);
            push_free_page(base);
        }

        *pte = not_present_pt_entry();
    }

    assign_free_page(old);
}

fernos_error_t pd_alloc_pages(phys_addr_t pd, void *s, void *e, void **true_e) {
    CHECK_ALIGN(pd, M_4K);
    CHECK_ALIGN(s, M_4K);
    CHECK_ALIGN(e, M_4K);

    if (!true_e) {
        return FOS_BAD_ARGS;
    }

    if (s == e) {
        *true_e = e;
        return FOS_SUCCESS;
    }

    if ((uint32_t)e < (uint32_t)s) {
        *true_e = NULL;
        return FOS_INVALID_RANGE;
    }

    fernos_error_t err = FOS_SUCCESS;

    uint32_t pi_s = (uint32_t)s / 1024;
    uint32_t pi_e = (uint32_t)e / 1024;
    uint32_t pi = pi_s;

    phys_addr_t old = assign_free_page(pd);

    pt_entry_t *pdes = (pt_entry_t *)free_kernel_page;

    while (err == FOS_SUCCESS && pi < pi_e) {
        uint32_t nb = ALIGN(pi + 1024, 1024);
        if (nb > pi_e) {
            nb = pi_e;
        }
        
        uint32_t pdi = pi / 1024;
        pt_entry_t *pde = &(pdes[pdi]);

        if (!pte_get_present(*pde)) {
            phys_addr_t new_pt = new_page_table();
            if (new_pt == NULL_PHYS_ADDR) {
                err = FOS_NO_MEM;
                break;
            }

            *pde = fos_unique_pt_entry(new_pt);
        }

        phys_addr_t pt = pte_get_base(*pde);

        uint32_t pti   = pi % 1024;

        // If we make it here, we know pi_s and pi_e span a non-empty range.
        // It is impossible for nb to be 0. If nb is on a boundary, set pti_e
        // to 1024!
        uint32_t pti_e = nb % 1024;
        if (pti_e == 0) {
            pti_e = 1024;
        }

        uint32_t true_pti_e;
        err = pt_alloc_range(pt, pti, pti_e, &true_pti_e);

        pi += (true_pti_e - pti);
    }

    assign_free_page(old);

    *true_e = i;

    return err;
}

void pd_free_pages(phys_addr_t pd, void *s, void *e) {
    if (!IS_ALIGNED(pd, M_4K) || !IS_ALIGNED(s, M_4K) || !IS_ALIGNED(e, M_4K)) {
        return;
    }

    if (s == e) {
        return;
    }

    if (e < s) {
        return;
    }

    phys_addr_t old = assign_free_page(pd);


    assign_free_page(old);
}



static uint8_t mm[M_4K] __attribute__ ((aligned(M_4K)));

void dss(void) {
    assign_free_page((phys_addr_t)mm); 

    phys_addr_t p = pop_free_page();

    free_kernel_page[0] = 0xFE;
    term_put_fmt_s("%X\n", mm[0]);
    term_put_fmt_s("%X\n", p);
}

