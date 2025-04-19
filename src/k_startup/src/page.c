
#include "k_sys/page.h"

#include "u_startup/main.h"
#include "k_bios_term/term.h"
#include "k_startup/page.h"
#include "k_sys/debug.h"
#include "os_defs.h"
#include "s_util/misc.h"
#include "s_util/err.h"
#include "s_util/str.h"
#include "s_bridge/intr.h"
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
static uint32_t *first_user_esp  = NULL;

/**
 * A set of reserved pages in the kernel to be used for page table editing.
 *
 * Always should be marked Identity, never should be added to the free list.
 *
 * By Aligning by M_4K * NUM_FKPs, this should be guaranteed to be in the same 4 MB region.
 * Thus all ptes should be in the same pt. (This is why NUM_FREE_KPs must be a power of 2)
 */
uint8_t free_kernel_pages[NUM_FREE_KERNEL_PAGES][M_4K] __attribute__((aligned(NUM_FREE_KERNEL_PAGES * M_4K)));

/**
 * After paging is initialized, these structures will be used to modify what
 * the free_kernel_page actually points to.
 * 
 * NOTE: The page table referenced here will not actually be "identity", this
 * 1024 entry space is reserved and mapped to an arbitrary page which contains
 * the correct page table.
 */
static pt_entry_t free_kernel_page_pt[1024] __attribute__((aligned(M_4K)));
static pt_entry_t *free_kernel_page_ptes[NUM_FREE_KERNEL_PAGES];

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

#define _R_IDENTITY  (1 << 0)
#define _R_ALLOCATE  (1 << 1)
#define _R_WRITEABLE (1 << 2)

/**
 * Make a certain range available within a page table.
 *
 * e is exclusive.
 */
static fernos_error_t _place_range(pt_entry_t *pd, uint8_t *s, const uint8_t *e, uint8_t flags) {
    if ((flags & _R_IDENTITY) && (flags & _R_ALLOCATE)) {
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

    const bool writeable = (flags & _R_WRITEABLE) == _R_WRITEABLE;

    uint32_t curr_pdi = 1024;
    pt_entry_t *pt = NULL;

    for (uint8_t *i = s; i < e; i+= M_4K) {
        uint32_t pi = (uint32_t)i / M_4K;

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

                // Page tables are always writeable.
                *pde = fos_unique_pt_entry((phys_addr_t)new_pt, true);
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

        phys_addr_t page_addr = (phys_addr_t)i;
        if (flags & _R_ALLOCATE) {
            // If allocate is marked, we map the virtual address i to a different page.
            page_addr = _pop_free_page();
            if (page_addr == NULL_PHYS_ADDR) {
                return FOS_NO_MEM;
            }
        }

        pt[pti] = (flags & _R_IDENTITY) 
            ? fos_identity_pt_entry(page_addr, writeable) 
            : fos_unique_pt_entry(page_addr, writeable);
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

    PROP_ERR(_place_range(pd, (uint8_t *)(PROLOGUE_START + M_4K), (const uint8_t *)(PROLOGUE_END + 1), _R_IDENTITY | _R_WRITEABLE));    
    PROP_ERR(_place_range(pd, _ro_shared_start, _ro_shared_end, _R_IDENTITY));
    PROP_ERR(_place_range(pd, _ro_kernel_start, _ro_kernel_end, _R_IDENTITY));

    PROP_ERR(_place_range(pd, _bss_shared_start, _bss_shared_end, _R_WRITEABLE));
    PROP_ERR(_place_range(pd, _data_shared_start, _data_shared_end, _R_WRITEABLE));

    PROP_ERR(_place_range(pd, _bss_kernel_start, _bss_kernel_end, _R_WRITEABLE));
    PROP_ERR(_place_range(pd, _data_kernel_start, _data_kernel_end, _R_WRITEABLE));

    PROP_ERR(_place_range(pd, _init_kstack_start, _init_kstack_end, _R_WRITEABLE));

    // Now setup up the free kernel pages!
    // THIS IS VERY CONFUSING SADLY!
    phys_addr_t fkp = (phys_addr_t)free_kernel_pages;

    uint32_t fkp_pi = fkp / M_4K;
    uint32_t fkp_pdi = fkp_pi / 1024;
    uint32_t fkp_pti = fkp_pi % 1024;

    // Let's get the page table which references fkp.
    pt_entry_t *pte = &(pd[fkp_pdi]);
    if (!pte_get_present(*pte)) {
        return FOS_UNKNWON_ERROR;
    }

    // Ok, this page table will contain the entry for the free kernel pages!
    pt_entry_t *fkp_pt = (pt_entry_t *)pte_get_base(*pte);

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
    fkp_pt_alias_pt[fkp_pt_alias_pti] = fos_unique_pt_entry((phys_addr_t)fkp_pt, true);

    for (uint32_t pti = 0; pti < NUM_FREE_KERNEL_PAGES; pti++) {
        // Set all kernel pages at not present to start.
        fkp_pt[fkp_pti + pti] = not_present_pt_entry();

        // Now store a reference to each pte for easy access later.
        free_kernel_page_ptes[pti] =  &(free_kernel_page_pt[fkp_pti + pti]);
    }

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
        uint8_t *s, const uint8_t *e) {
    CHECK_ALIGN(dest_pd, M_4K);
    CHECK_ALIGN(src_pd, M_4K);
    CHECK_ALIGN(s, M_4K);
    CHECK_ALIGN(e, M_4K);

    for (uint8_t *i = s; i < e; i += M_4K) {
        uint32_t pi = (uint32_t)i / M_4K;

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
    clear_page_table(pd);

    PROP_ERR(_place_range(pd, _ro_shared_start, _ro_shared_end, _R_IDENTITY));
    PROP_ERR(_place_range(pd, _ro_user_start, _ro_user_end, _R_IDENTITY));

    // Here, both the kernel and user pds will need their own indepent copies of the shared data
    // and shared bss. So, when making the user space, we allocate new pages in this area.
    // Then, we copy over what is contained in the kernel pages!
    PROP_ERR(_place_range(pd, _bss_shared_start, _bss_shared_end, _R_WRITEABLE | _R_ALLOCATE));
    PROP_ERR(_copy_range(pd, kpd, _bss_shared_start, _bss_shared_end));

    PROP_ERR(_place_range(pd, _data_shared_start, _data_shared_end, _R_WRITEABLE | _R_ALLOCATE));
    PROP_ERR(_copy_range(pd, kpd, _data_shared_start, _data_shared_end));

    PROP_ERR(_place_range(pd, _bss_user_start, _bss_user_end, _R_WRITEABLE));
    PROP_ERR(_place_range(pd, _data_user_start, _data_user_end, _R_WRITEABLE));

    // NOTE: that stack start is just initial, It will be able to grow.
    const uint8_t *stack_end = _init_kstack_end;
    uint8_t *stack_start = (uint8_t *)(stack_end - M_4K);

    PROP_ERR(_place_range(pd, stack_start, stack_end, _R_WRITEABLE | _R_ALLOCATE));

    uint32_t stack_pi = (uint32_t)stack_start / M_4K; 
    uint32_t stack_pdi = stack_pi / 1024;
    uint32_t stack_pti = stack_pi % 1024;

    // Let's get the underlying physical page of where the stack was placed.
    pt_entry_t stack_pde = pd[stack_pdi];
    pt_entry_t stack_pte = ((pt_entry_t *)pte_get_base(stack_pde))[stack_pti];
    uint32_t *phys_stack_end = (uint32_t *)((uint8_t *)pte_get_base(stack_pte) + M_4K);

    // Now, we need to set up the stack such that it can be switched into.
    // We need to be able to call `popad` followed by `iret`.
    //
    // This will in total be 11 dwords.

    uint32_t *init_stack_frame = phys_stack_end - 11;

    // Zero out the inital stack frame.
    for (uint32_t i = 0; i < 11; i++) {
        init_stack_frame[i] = 0;
    }

    init_stack_frame[10] = read_eflags() | (1 << 9); // enable interrupts.
    init_stack_frame[9] = 0x8;
    init_stack_frame[8] = (uint32_t)user_main; 
    init_stack_frame[3] = (uint32_t)(init_stack_frame + 8);

    first_user_pd = upd;
    first_user_esp = (uint32_t *)stack_end - 11;

    return FOS_SUCCESS;
}

fernos_error_t init_paging(void) {
    // The functions in this C file require at least 2 kernel free pages.
    if (NUM_FREE_KERNEL_PAGES < 2) {
        return FOS_NO_MEM;
    }

    PROP_ERR(_init_free_list());
    PROP_ERR(_init_kernel_pd());
    PROP_ERR(_init_first_user_pd()); 

    set_page_directory(kernel_pd);
    enable_paging();

    set_intr_ctx(kernel_pd, (const uint32_t *)_init_kstack_end);

    return FOS_SUCCESS;
}

/* NOW PAGING HAS BEEN ENABLED */

phys_addr_t get_kernel_pd(void) {
    return kernel_pd;    
}

fernos_error_t pop_initial_user_info(phys_addr_t *upd, const uint32_t **uesp) {
    if (first_user_pd == NULL_PHYS_ADDR) {
        return FOS_UNKNWON_ERROR;
    }

    if (!upd || !uesp) {
        return FOS_BAD_ARGS;
    }

    *upd = first_user_pd;
    *uesp = first_user_esp;

    first_user_pd = NULL_PHYS_ADDR;
    first_user_esp = NULL;
    
    return FOS_SUCCESS;
}

phys_addr_t assign_free_page(uint32_t slot, phys_addr_t p) {
    phys_addr_t ret = NULL_PHYS_ADDR;

    pt_entry_t *free_kernel_page_pte = free_kernel_page_ptes[slot];

    if (pte_get_present(*free_kernel_page_pte)) {
        ret = pte_get_base(*free_kernel_page_pte); 
    }

    *free_kernel_page_pte = fos_unique_pt_entry(p, true);

    flush_page_cache();

    return ret;
}

uint32_t get_num_free_pages(void) {
    return free_list_len;
}

void push_free_page(phys_addr_t page_addr) {
    if (!(IS_ALIGNED(page_addr, M_4K))) {
        return;
    }
    
    phys_addr_t old = assign_free_page(0, page_addr);
    *(phys_addr_t *)(free_kernel_pages[0]) = free_list_head;
    assign_free_page(0, old);

    free_list_head = page_addr;
    free_list_len++; 
}

phys_addr_t pop_free_page(void) {
    if (free_list_head == NULL_PHYS_ADDR) {
        return NULL_PHYS_ADDR; 
    }

    phys_addr_t old = assign_free_page(0, free_list_head);
    phys_addr_t next = *(phys_addr_t *)(free_kernel_pages[0]);
    assign_free_page(0, old);

    phys_addr_t ret = free_list_head;

    free_list_head = next;
    free_list_len--;

    return ret;
}

static phys_addr_t new_page_table(void) {
    phys_addr_t pt = pop_free_page();

    if (pt != NULL_PHYS_ADDR) {
        phys_addr_t old = assign_free_page(0, pt);
        clear_page_table((pt_entry_t *)(free_kernel_pages[0]));
        assign_free_page(0, old);
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

    phys_addr_t old = assign_free_page(0, pt);

    pt_entry_t *ptes = (pt_entry_t *)(free_kernel_pages[0]);

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

        *pte = fos_unique_pt_entry(new_page, true);
    }

    assign_free_page(0, old);

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

    phys_addr_t old = assign_free_page(0, pt);

    pt_entry_t *ptes = (pt_entry_t *)(free_kernel_pages[0]);

    for (uint32_t i = s; i < e; i++) {
        pt_entry_t *pte = &(ptes[i]);

        if (pte_get_present(*pte) && pte_get_avail(*pte) == UNIQUE_ENTRY) {
            phys_addr_t base = pte_get_base(*pte);
            push_free_page(base);
        }

        *pte = not_present_pt_entry();
    }

    assign_free_page(0, old);
}

phys_addr_t new_page_directory(void) {
    return new_page_table();
}

fernos_error_t pd_alloc_pages(phys_addr_t pd, void *s, const void *e, const void **true_e) {
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

    uint32_t pi_s = (uint32_t)s / M_4K;
    uint32_t pi_e = (uint32_t)e / M_4K;
    uint32_t pi = pi_s;

    phys_addr_t old = assign_free_page(0, pd);

    pt_entry_t *pdes = (pt_entry_t *)(free_kernel_pages[0]);

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

            *pde = fos_unique_pt_entry(new_pt, true);
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

    assign_free_page(0, old);

    *true_e = (void *)(pi * M_4K);

    return err;
}

void pd_free_pages(phys_addr_t pd, void *s, const void *e) {
    if (!IS_ALIGNED(pd, M_4K) || !IS_ALIGNED(s, M_4K) || !IS_ALIGNED(e, M_4K)) {
        return;
    }

    if (s == e) {
        return;
    }

    if (e < s) {
        return;
    }

    uint32_t pi_s = (uint32_t)s / M_4K;
    uint32_t pi_e = (uint32_t)e / M_4K;
    uint32_t pi = pi_s;

    phys_addr_t old = assign_free_page(0, pd);
    pt_entry_t *pdes = (pt_entry_t *)(free_kernel_pages[0]);

    while (pi < pi_e) {
        uint32_t nb = ALIGN(pi + 1024, 1024);
        if (nb > pi_e) {
            nb = pi_e;
        }

        uint32_t pdi = pi / 1024; 
        pt_entry_t *pde = &(pdes[pdi]);

        // Ok are we working with a present page table?
        if (pte_get_present(*pde)) {
            phys_addr_t pt = pte_get_base(*pde);

            // Remeber pi < nb always here!

            uint32_t fs = pi % 1024;
            uint32_t fe = nb % 1024;
            if (fe == 0) {
                fe = 1024;
            }

            pt_free_range(pt, fs, fe);

            /*
             * NOTE: When we all entries in the page table, we know we can push the page table back
             * onto the free list.
             *
             * However, understand this is not a perfect solution. If we allocate just a few pages
             * in an empty page table, then later free those pages. Since we aren't freeing the
             * entire range of the page table, this method will not realize the page table is 
             * entirely empty.
             *
             * Right now, we can expect that sometimes this function is smart enough to free empty
             * page tables, other times it is not.
             *
             * TODO: Consider improving.
             */

            // Did we free a whole page table?
            if (fs == 0 && fe == 1024) {
                push_free_page(pt);
                *pde = not_present_pt_entry();
            }
        }

        pi = nb;
    }

    assign_free_page(0, old);
}

void delete_page_directory(phys_addr_t pd) {
    if (pd == NULL_PHYS_ADDR) {
        return;
    }

    phys_addr_t old = assign_free_page(0, pd);
    pt_entry_t *pdes = (pt_entry_t *)(free_kernel_pages[0]);

    for (uint32_t pdi = 0; pdi < 1024; pdi++) {
        pt_entry_t pde = pdes[pdi]; 

        // Is there a page table in this slot?
        if (pte_get_present(pde)) {
            phys_addr_t pt = pte_get_base(pde);

            // Remember, page tables are NEVER shared.
            pt_free_range(pt, 0, 1024); 
            push_free_page(pt);
        }
    }

    assign_free_page(0, old);

    push_free_page(pd);
}

void page_copy(phys_addr_t dest, phys_addr_t src) {
    phys_addr_t old0 = assign_free_page(0, dest);
    phys_addr_t old1 = assign_free_page(1, src);

    void *vdest = free_kernel_pages[0];
    void *vsrc = free_kernel_pages[1];

    mem_cpy(vdest, vsrc, M_4K);

    assign_free_page(0, old0);
    assign_free_page(1, old1);
}

