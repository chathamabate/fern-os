
#include "k_startup/page_helpers.h"

#include "s_util/str.h"

void page_copy(phys_addr_t dest, phys_addr_t src) {
    phys_addr_t old0 = assign_free_page(0, dest);
    phys_addr_t old1 = assign_free_page(1, src);

    void *vdest = free_kernel_pages[0];
    void *vsrc = free_kernel_pages[1];

    mem_cpy(vdest, vsrc, M_4K);

    assign_free_page(0, old0);
    assign_free_page(1, old1);
}

phys_addr_t get_underlying_page(phys_addr_t pd, const void *ptr) {
    phys_addr_t old0 = assign_free_page(0, pd);

    uint32_t pi = (uint32_t)ptr / M_4K;
    uint32_t pdi = pi / 1024;
    uint32_t pti = pi % 1024;

    pt_entry_t *pdir = (pt_entry_t *)(free_kernel_pages[0]);
    pt_entry_t *pde = pdir + pdi;

    phys_addr_t ret = NULL_PHYS_ADDR;
    
    if (pte_get_present(*pde)) {
        phys_addr_t phys_pt = pte_get_base(*pde);
        assign_free_page(0, phys_pt);

        pt_entry_t *ptab = (pt_entry_t *)(free_kernel_pages[0]);
        pt_entry_t *pte = ptab + pti;

        if (pte_get_present(*pte)) {
            ret = pte_get_base(*pte);
        }
    }

    assign_free_page(0, old0);

    return ret;
}

/**
 * Returns NULL_PHYS_ADDR on error.
 */
phys_addr_t copy_page_table(phys_addr_t pt) {
    if (pt == NULL_PHYS_ADDR) {
        return NULL_PHYS_ADDR; // Can't copy the NULL page.
    }

    phys_addr_t pt_copy = new_page_table();

    if (pt_copy == NULL_PHYS_ADDR) {
        return NULL_PHYS_ADDR;
    }

    fernos_error_t status = FOS_E_SUCCESS;
    
    phys_addr_t old0 = assign_free_page(0, pt);
    phys_addr_t old1 = assign_free_page(1, pt_copy);

    pt_entry_t *og_pt = (pt_entry_t *)(free_kernel_pages[0]);
    pt_entry_t *new_pt = (pt_entry_t *)(free_kernel_pages[1]);

    for (uint32_t i = 0; status == FOS_E_SUCCESS && i < 1024; i++) {
        pt_entry_t pte = og_pt[i];

        // Skip all empty entries.
        if (!pte_get_present(pte)) {
            continue;  
        }

        uint8_t avail = pte_get_avail(pte);

        if (avail == UNIQUE_ENTRY) {
            // deep copy required!
            phys_addr_t og_base  = pte_get_base(pte);
            phys_addr_t new_base =  pop_free_page();

            if (new_base == NULL_PHYS_ADDR) {
                status = FOS_E_NO_MEM; // We failed :(
            } else {
                page_copy(new_base, og_base); 

                new_pt[i] = fos_unique_pt_entry(new_base, 
                        pte_get_user(pte) == 1,
                        pte_get_writable(pte) == 1);
            }
        } else {
            // shallow copy just fine.
            new_pt[i] = pte;
        }
    }

    assign_free_page(1, old1);
    assign_free_page(0, old0);

    if (status != FOS_E_SUCCESS) {
        delete_page_table(pt_copy);
        pt_copy = NULL_PHYS_ADDR;
    }

    return pt_copy;
}

phys_addr_t copy_page_directory(phys_addr_t pd) {
    if (pd == NULL_PHYS_ADDR) {
        return NULL_PHYS_ADDR;
    }

    phys_addr_t pd_copy = new_page_directory();

    if (pd_copy == NULL_PHYS_ADDR) {
        return NULL_PHYS_ADDR;
    }

    fernos_error_t status = FOS_E_SUCCESS;

    phys_addr_t old0 = assign_free_page(0, pd);
    phys_addr_t old1 = assign_free_page(1, pd_copy);

    pt_entry_t *og_pd = (pt_entry_t *)(free_kernel_pages[0]);
    pt_entry_t *new_pd = (pt_entry_t *)(free_kernel_pages[1]);

    for (uint32_t i = 0; status == FOS_E_SUCCESS && i < 1024; i++) {
        pt_entry_t pte = og_pd[i];

        // Skip empty entries.
        if (!pte_get_present(pte)) {
            continue;
        }

        phys_addr_t og_pt = pte_get_base(pte);

        phys_addr_t pt_copy = copy_page_table(og_pt);
        if (pt_copy == NULL_PHYS_ADDR) {
            status = FOS_E_NO_MEM; // We failed :(
        } else {
            // Page tables are always unique and writeable. 
            new_pd[i] = fos_unique_pt_entry(pt_copy, true, true);
        }
    }

    assign_free_page(1, old1);
    assign_free_page(0, old0);

    if (status != FOS_E_SUCCESS) {
        delete_page_directory(pd_copy);
        pd_copy = NULL_PHYS_ADDR;
    }

    return pd_copy;

}

/**
 * Copy bytes to or from another memory space.
 *
 * When direction is true, copy to the memory space.
 * When false, copy from the memory space.
 */
static fernos_error_t mem_cpy_user(void *kbuf, phys_addr_t user_pd, void *ubuf, 
        uint32_t bytes, uint32_t *copied, bool direction) {
    
    if (!kbuf || user_pd == NULL_PHYS_ADDR || !ubuf) {
        if (copied) {
            *copied = 0;
        }
        return FOS_E_BAD_ARGS;
    }

    const uint8_t *limit = (uint8_t *)ubuf + bytes;

    uint32_t bytes_copied = 0;

    while (bytes_copied < bytes) {
        const uint8_t *tbuf = (uint8_t *)ubuf + bytes_copied;
        phys_addr_t upage = get_underlying_page(user_pd, tbuf);
        if (upage == NULL_PHYS_ADDR) {
            break;
        }

        const uint8_t *nb = (uint8_t *)ALIGN(tbuf + M_4K, M_4K);
        if (nb > limit) {
            nb = limit;
        }

        uint32_t bytes_to_copy = (uint32_t)nb - (uint32_t)tbuf;

        phys_addr_t old0 = assign_free_page(0, upage);

        uint32_t offset = (uint32_t)tbuf % M_4K;

        if (direction) {
            mem_cpy(free_kernel_pages[0] + offset, (uint8_t *)kbuf + bytes_copied, bytes_to_copy); 
        } else {
            mem_cpy((uint8_t *)kbuf + bytes_copied, free_kernel_pages[0] + offset, bytes_to_copy);
        }

        bytes_copied += bytes_to_copy;

        assign_free_page(0, old0);
    }

    if (copied) {
        *copied = bytes_copied;
    }

    return bytes_copied == bytes ? FOS_E_SUCCESS : FOS_E_NO_MEM;

}

fernos_error_t mem_cpy_from_user(void *dest, phys_addr_t user_pd, const void *user_src, 
        uint32_t bytes, uint32_t *copied) {
    return mem_cpy_user(dest, user_pd, (void *)user_src, bytes, copied, false);
}

fernos_error_t mem_cpy_to_user(phys_addr_t user_pd, void *user_dest, const void *src, 
        uint32_t bytes, uint32_t *copied) {
    return mem_cpy_user((void *)src, user_pd, user_dest, bytes, copied, true);
}

fernos_error_t mem_set_to_user(phys_addr_t user_pd, void *user_dest, uint8_t val, uint32_t bytes) {
    if (user_pd == NULL_PHYS_ADDR) {
        return FOS_E_BAD_ARGS;
    }

    uint8_t *iter = user_dest;
    uint32_t bytes_left = bytes;

    while (bytes_left > 0) {
        phys_addr_t underlying = get_underlying_page(user_pd, iter);
        if (underlying == NULL_PHYS_ADDR) {
            return FOS_E_NO_MEM; // Unmapped!
        }

        phys_addr_t old = assign_free_page(0, underlying);
        
        const uint32_t offset = (uint32_t)iter % M_4K;
        uint32_t amt_to_set = M_4K - offset;
        if (amt_to_set > bytes_left) {
            amt_to_set = bytes_left;
        }

        mem_set(free_kernel_pages[0] + offset, val, amt_to_set);

        assign_free_page(0, old);

        iter += amt_to_set;
        bytes_left -= amt_to_set;
    }

    return FOS_E_SUCCESS;
}
