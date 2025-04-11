
#include "k_bios_term/term.h"

#include "s_mem/allocator.h"
#include "s_mem/simple_heap.h"
#include "s_mem/test/allocator.h"
#include "s_mem/test/simple_heap.h"
#include "k_startup/page.h"

static const mem_manage_pair_t test_mmp;

static allocator_t *gen_basic_shal(void) {
    simple_heap_attrs_t attrs = {
        .start = (void *)(2 * M_4M),
        .end = (const void *)((3 * M_4M)),

        .mmp = test_mmp,

        .small_fl_cutoff = 0x100,
        .small_fl_search_amt = 0x10,
        .large_fl_search_amt = 0x10,
    };

    return new_simple_heap_allocator(attrs);
}

static allocator_t *gen_big_cutoff_shal(void) {
    simple_heap_attrs_t attrs = {
        .start = (void *)(2 * M_4M),
        .end = (const void *)((3 * M_4M)),

        .mmp = test_mmp,

        .small_fl_cutoff = 0x1000,
        .small_fl_search_amt = 0x10,
        .large_fl_search_amt = 0x10,
    };

    return new_simple_heap_allocator(attrs);
}

static allocator_t *gen_no_search_limit_shal(void) {
    simple_heap_attrs_t attrs = {
        .start = (void *)(2 * M_4M),
        .end = (const void *)((3 * M_4M)),

        .mmp = test_mmp,

        .small_fl_cutoff = 0x10,
        .small_fl_search_amt = 0,
        .large_fl_search_amt = 0,
    };

    return new_simple_heap_allocator(attrs);
}

void test_shal(mem_manage_pair_t mmp) {
    *(mem_manage_pair_t *)&test_mmp = mmp;

    //(void)gen_basic_shal;
    test_allocator("Basic SHAL", gen_basic_shal);

    (void)gen_big_cutoff_shal;
    //test_allocator("Big Cutoff SHAL", gen_big_cutoff_shal);
    
    (void)gen_no_search_limit_shal;
    //test_allocator("No Search Limit SHAL", gen_no_search_limit_shal);
}
