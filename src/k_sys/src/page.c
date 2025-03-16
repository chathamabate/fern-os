
#include "k_sys/page.h"
#include "s_util/misc.h"

int init_paged_area(phys_addr_t start, phys_addr_t end, phys_addr_t tail) {
    uint32_t mask_4k = M_4K - 1;

    if ((start & mask_4k) || (end & mask_4k) || (tail & mask_4k)) {
        return 1; // All given addresses must be 4k aligned.
    }

    phys_addr_t curr, next;

    if (!is_paging_enabled()) {
        curr = start;

        while (curr < end) {
            next = curr + M_4K;

            *(uint32_t *)curr = next < end ? next : tail;

            curr = next;
        }

        return 0;
    }

    // We need virtual address to the page table??? 
    // Wait, this is lowkey hard, why even do it like that tbh...
    // This should be partitioned from the begining!

    return 0;
}

