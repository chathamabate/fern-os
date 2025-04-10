
#include "k_bios_term/term.h"

#include "s_mem/allocator.h"
#include "s_mem/simple_heap.h"
#include "s_mem/test/simple_heap.h"
#include "k_startup/page.h"

void test_shal(void) {
    simple_heap_attrs_t attrs = {
        .start = (void *)(2 * M_4M),
        .end = (const void *)(3 * M_4M),

        .request_mem = alloc_pages,
        .return_mem = free_pages,

        .small_fl_cutoff = 0x100,

        .small_fl_search_amt = 0x10,
        .large_fl_search_amt = 0x10,
    };

    allocator_t *al = new_simple_heap_allocator(attrs);

    if (!al) {
        term_put_s("Couldn't create allocator\n");
    }

    al_dump(al, term_put_fmt_s);

    delete_allocator(al);
}
