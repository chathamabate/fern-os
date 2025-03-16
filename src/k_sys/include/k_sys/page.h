

#pragma once

#include "k_sys/debug.h"
#include "k_sys/page.h"
#include "s_util/misc.h"
#include <stdint.h>

typedef uint32_t phys_addr_t;
#define NULL_PHYS_ADDR (0)

typedef uint32_t pt_entry_t;

#define PTE_PRSNT_OFF (0)      
#define PTE_PRSNT_WID (1)  
#define PTE_PRSNT_WID_MASK TO_MASK64(PTE_PRSNT_WID)
#define PTE_PRSNT_MASK (PTE_PRSNT_WID_MASK << PTE_PRSNT_OFF)       

#define PTE_WRITABLE_OFF (1)      
#define PTE_WRITABLE_WID (1)  
#define PTE_WRITABLE_WID_MASK TO_MASK64(PTE_WRITABLE_WID)
#define PTE_WRITABLE_MASK (PTE_WRITABLE_WID_MASK << PTE_WRITABLE_OFF)       

#define PTE_USER_OFF (2)      
#define PTE_USER_WID (1)  
#define PTE_USER_WID_MASK TO_MASK64(PTE_USER_WID)
#define PTE_USER_MASK (PTE_USER_WID_MASK << PTE_USER_OFF)       

#define PTE_AVAIL_OFF (9)      
#define PTE_AVAIL_WID (3)  
#define PTE_AVAIL_WID_MASK TO_MASK64(PTE_AVAIL_WID)
#define PTE_AVAIL_MASK (PTE_AVAIL_WID_MASK << PTE_AVAIL_OFF)       

#define PTE_BASE_OFF (12)      
#define PTE_BASE_WID (20)  
#define PTE_BASE_WID_MASK TO_MASK64(PTE_BASE_WID)
#define PTE_BASE_MASK (PTE_BASE_WID_MASK << PTE_BASE_OFF)       

static inline pt_entry_t not_present_pt_entr(void) {
    return 0;
}

static inline void pte_set_present(pt_entry_t *pte, uint8_t p) {
    pt_entry_t te = *pte;

    te &= ~(PTE_PRSNT_MASK);
    te |= (p & PTE_PRSNT_WID_MASK) << PTE_PRSNT_OFF;

    *pte = te;
}

static inline uint8_t pte_get_present(pt_entry_t pte) {
    return (pte & PTE_PRSNT_MASK) >> PTE_PRSNT_OFF;
}

static inline void pte_set_writable(pt_entry_t *pte, uint8_t wr) {
    pt_entry_t te = *pte;

    te &= ~(PTE_WRITABLE_MASK);
    te |= (wr & PTE_WRITABLE_WID_MASK) << PTE_WRITABLE_OFF;

    *pte = te;
}

static inline uint8_t pte_get_writable(pt_entry_t pte) {
    return (pte & PTE_WRITABLE_MASK) >> PTE_WRITABLE_OFF;
}

static inline void pte_set_user(pt_entry_t *pte, uint8_t usr) {
    pt_entry_t te = *pte;

    te &= ~(PTE_USER_MASK);
    te |= (usr & PTE_USER_WID_MASK) << PTE_USER_OFF;

    *pte = te;
}

static inline uint8_t pte_get_user(pt_entry_t pte) {
    return (pte & PTE_USER_MASK) >> PTE_USER_OFF;
}

static inline void pte_set_avail(pt_entry_t *pte, uint8_t av) {
    pt_entry_t te = *pte;

    te &= ~(PTE_AVAIL_MASK);
    te |= (av & PTE_AVAIL_WID_MASK) << PTE_AVAIL_OFF;

    *pte = te;
}

static inline uint8_t pte_get_avail(pt_entry_t pte) {
    return (pte & PTE_AVAIL_MASK) >> PTE_AVAIL_OFF;
}

static inline void pte_set_base(pt_entry_t *pte, phys_addr_t base) {
    pt_entry_t te = *pte;

    te &= ~(PTE_BASE_MASK);
    te |= base & ~TO_MASK64(PTE_BASE_OFF); 

    *pte = te;
}

static inline phys_addr_t pte_get_base(pt_entry_t pte) {
    return (pte & (uint32_t)PTE_BASE_MASK);
}

void enable_paging(void);

static inline uint32_t is_paging_enabled(void) {
    return read_cr0() & (1 << 31);
}

phys_addr_t get_page_directory(void);
void set_page_directory(phys_addr_t pd);

static inline void flush_page_cache(void) {
    set_page_directory(get_page_directory());
}

// Initialize a large area of memory as being a "paged" area.
// All this does is create a linked list embedded within the area such that
// it is easy to later pop and push free pages from the area.
//
// All given physical addresses MUST be 4K aligned, or an error is returned.
//
// It may be of interest to attatch this linked list to another linkedlist of 
// free pages. To do so, specify the "tail" as the physical address of the first
// page in the next linked list. Otherwise, set tail as NULL_PHYS_ADDR.
//
// NOTE: This function will work both when paging is enabled, and when paging
// is disabled!
//
// returns 0 on success, 1 on error.
int init_paged_area(phys_addr_t start, phys_addr_t end, phys_addr_t tail);


