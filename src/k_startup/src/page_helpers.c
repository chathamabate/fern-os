
#include "k_startup/page_helpers.h"

#include "s_util/str.h"
#include "s_util/constraints.h"

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

fernos_error_t mem_set_to_user(phys_addr_t user_pd, void *user_dest, uint8_t val, uint32_t bytes, uint32_t *set) {
    if (user_pd == NULL_PHYS_ADDR) {
        return FOS_E_BAD_ARGS;
    }

    fernos_error_t err = FOS_E_SUCCESS;

    uint8_t *iter = user_dest;
    uint32_t bytes_left = bytes;

    while (bytes_left > 0) {
        phys_addr_t underlying = get_underlying_page(user_pd, iter);
        if (underlying == NULL_PHYS_ADDR) {
            err = FOS_E_NO_MEM;
            break;
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

    if (set) {
        *set = bytes - bytes_left;
    }

    return err;
}

fernos_error_t new_user_app_pd(const user_app_t *ua, const void *abs_ab, size_t abs_ab_len,
        phys_addr_t *out) {
    fernos_error_t err;

    if (!ua || !out) {
        return FOS_E_BAD_ARGS;
    }

    // First confirm args block is seemingly valid.

    if (abs_ab_len > 0 && !abs_ab) {
        return FOS_E_BAD_ARGS;
    }

    if (abs_ab_len > FOS_APP_ARGS_AREA_SIZE) {
        return FOS_E_BAD_ARGS;
    }
    
    // The entry point of the given app MUST be inside one of the given ranges!
    bool entry_valid = false;

    // Ok, let's first confirm that `ua` is loadable!
    for (size_t i = 0; i < FOS_MAX_APP_AREAS; i++) {
        if (ua->areas[i].occupied) {
            if (ua->areas[i].area_size == 0) {
                return FOS_E_INVALID_RANGE; // We'll say all loadable regions must have
                                            // non-zero size!
            }

            const void *start = ua->areas[i].load_position;
            const void *end = (uint8_t *)start + ua->areas[i].area_size;

            if (!IS_ALIGNED(start, M_4K)) {
                return FOS_E_ALIGN_ERROR;
            }

            if (end < start) { // Wrap case! 
                return FOS_E_INVALID_RANGE;
            }

            // NOTE: The end of the loadable area CAN be the end of the app area!
            if (start < (void *)FOS_APP_AREA_START || (void *)FOS_APP_AREA_END < end) {
                return FOS_E_INVALID_RANGE;
            }

            if (start <= ua->entry && ua->entry < end) {
                entry_valid = true;
            }

            // Also, just as a sanity check, let's confirm that the given size is <= the area size.
            if (ua->areas[i].given_size > ua->areas[i].area_size) {
                return FOS_E_INVALID_RANGE;
            }
        }
    }
    
    if (!entry_valid) {
        return FOS_E_INVALID_RANGE;
    }

    // NOTE: We don't explicitly check if ranges overlap above. This will be handled automatically
    // when we write to the new page directory below.

    // Looks like our args block and user app are both somewhat loadable!!

    // First off, we need to setup the page directory for our new application.
    // If we fail at any point, we can just easily delete the page directory and return.
    
    phys_addr_t new_pd = pop_initial_user_pd_copy();
    if (new_pd == NULL_PHYS_ADDR) {
        return FOS_E_NO_MEM;
    }

    // First, let's deal with the args block. 
    
    err = FOS_E_SUCCESS;

    if (abs_ab) { // Remember, abs_ab is allowed to be NULL!
                      // If no args block is given, we don't need to do much!

        // First let's reserve the area necessary in the page directory for the args block!
        size_t abs_ab_alloc_size = abs_ab_len;
        if (!IS_ALIGNED(abs_ab_alloc_size, M_4K)) {
            abs_ab_alloc_size = ALIGN(abs_ab_alloc_size + M_4K, M_4K);
        }

        const void *true_e;
        err = pd_alloc_pages(new_pd, true, (void *)FOS_APP_ARGS_AREA_START, 
                (void *)(FOS_APP_ARGS_AREA_START + abs_ab_alloc_size), &true_e);


        // Ok, now we copy the args block into the app args area!  (This is why the args block must 
        // be absolute when given)

        if (err == FOS_E_SUCCESS) {
            err = mem_cpy_to_user(new_pd, (void *)FOS_APP_ARGS_AREA_START, abs_ab, 
                    abs_ab_len, NULL);
        }
    }

    // Now we can move onto loading app sections!

    for (size_t i = 0; err == FOS_E_SUCCESS && i < FOS_MAX_APP_AREAS; i++) {
        const user_app_area_entry_t *uaa = ua->areas + i;

        if (!(uaa->occupied)) {
            continue;;
        }

        // We know from above that at this point, all occupied areas must have a non zero
        // area size! (This doesn't really change that much, but whatever!)
        //
        // We also know that each load position in 4K Aligned!
        // We'll round up sizes to 4K which shouldn't cause any problems since load positions
        // are 4K aligned!

        size_t area_size = uaa->area_size;
        if (!IS_ALIGNED(area_size, M_4K)) {
            area_size = ALIGN(area_size, M_4K) + M_4K;
        }
        
        const void *true_e;
        err = pd_alloc_pages(new_pd, true, uaa->load_position, 
                (uint8_t *)(uaa->load_position) + area_size, &true_e);

        if (err == FOS_E_SUCCESS && uaa->given_size > 0) {
            err = mem_cpy_to_user(new_pd, uaa->load_position, uaa->given, uaa->given_size, NULL);
        }

        const size_t bytes_to_set = uaa->area_size - uaa->given_size;

        if (err == FOS_E_SUCCESS && bytes_to_set > 0) {
            err = mem_set_to_user(new_pd, 
                    (uint8_t *)(uaa->load_position) + uaa->given_size, 0, bytes_to_set, NULL);
        }
    }

    if (err != FOS_E_SUCCESS) {
        delete_page_directory(new_pd);

        return err;
    }

    *out = new_pd;
    
    return FOS_E_SUCCESS;
}

user_app_t *ua_copy_from_user(allocator_t *al, phys_addr_t pd, const user_app_t *u_ua) {
    fernos_error_t err;

    if (!al || pd == NULL_PHYS_ADDR || !u_ua) {
        return NULL;
    }

    user_app_t *ua = al_malloc(al, sizeof(user_app_t)); 
    if (!ua) {
        return NULL;
    }

    err = mem_cpy_from_user(ua, pd, u_ua, sizeof(user_app_t), NULL);
    if (err != FOS_E_SUCCESS) {
        al_free(al, ua);
        return NULL;
    }

    ua->al = al; // Essential we overwrite the userspace allocator!

    // Ok, now to do a deep copy of all the given regions!

    err = FOS_E_SUCCESS;

    size_t i; 

    for (i = 0; i < FOS_MAX_APP_AREAS && err == FOS_E_SUCCESS; ) {
        if (ua->areas[i].occupied && ua->areas[i].given_size > 0) { // A copy is required!
            void *given_copy = al_malloc(al, ua->areas[i].given_size);

            if (given_copy) { 
                const size_t u_given_size = ua->areas[i].given_size;
                const void *u_given = ua->areas[i].given;

                ua->areas[i].given = given_copy;

                // We incrememnt `i` once `ua->areas` holds a buffer allocated here in this space.
                // This will make deletion easier in an error case.
                i++;

                err = mem_cpy_from_user(given_copy, pd, u_given, u_given_size, NULL); 
            } else { // Failed to malloc
                err = FOS_E_NO_MEM;
            }
        } else {
            i++;
        }
    }

    if (err != FOS_E_SUCCESS) { // error case, we only delete given buffers we know
                                // are here in this space.
        for (size_t j = 0; j < i; j++) {
            if (ua->areas[j].occupied && ua->areas[j].given_size > 0) {
                al_free(al, (void *)(ua->areas[j].given));
            }
        }

        al_free(al, ua);

        return NULL;
    }
    
    return ua;
}
