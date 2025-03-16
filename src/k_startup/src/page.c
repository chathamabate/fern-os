
#include "k_sys/page.h"
#include "os_defs.h"

#include "k_startup/page.h"

pt_entry_t identity_pts[NUM_IDENTITY_PTS][1024] __attribute__ ((aligned(M_4K)));

static void init_identity_pts(void) {
    for (uint32_t pti = 0; pti < NUM_IDENTITY_PTS; pti++) {
        pt_entry_t *pt = identity_pts[pti];

        for (uint32_t i = 0; i < 1024; i++) {
            pt_entry_t *pte = &(pt[i]);
            
            pte_set_base(pte, ((pti * 1024) + i) * M_4K);
            pte_set_user(pte, 0);
            pte_set_present(pte, 1);
            pte_set_writable(pte, 1);
        }
    }
}

int _init_paging(void) {

    init_identity_pts();
}

