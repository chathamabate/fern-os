
#include "k_bios_term/term.h"

#include "s_mem/allocator.h"
#include "s_mem/simple_heap.h"
#include "s_mem/test/allocator.h"
#include "s_mem/test/simple_heap.h"
#include "k_startup/page.h"

const mem_manage_pair_t test_mmp;

static allocator_t *gen_shal(void) {
    simple_heap_attrs_t attrs = {
        .start = (void *)(2 * M_4M),
        .end = (const void *)(3 * M_4M),

        .mmp = test_mmp,

        .small_fl_cutoff = 0x100,

        .small_fl_search_amt = 0x10,
        .large_fl_search_amt = 0x10,
    };

    return new_simple_heap_allocator(attrs);

}

void test_shal(mem_manage_pair_t mmp) {
    *(mem_manage_pair_t *)&test_mmp = mmp;
    test_allocator("SHAL", gen_shal);
}
