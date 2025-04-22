
#include "k_startup/fwd_defs.h"
#include "k_startup/page.h"
#include "k_startup/test/page.h"
#include "s_util/constraints.h"
#include "k_sys/idt.h"
#include "k_sys/page.h"
#include "s_data/id_table.h"
#include "s_mem/allocator.h"
#include "s_util/err.h"
#include "s_util/test/str.h"
#include "k_sys/debug.h"
#include "k_startup/gdt.h"
#include "k_startup/idt.h"
#include "k_bios_term/term.h"
#include "s_mem/simple_heap.h"
#include "s_bridge/intr.h"

#include "s_data/test/id_table.h"
#include "s_data/test/list.h"
#include "s_mem/test/simple_heap.h"

#include "k_startup/state.h"

static kernel_state_t *kernel = NULL;

static uint8_t init_err_style;

static inline void setup_fatal(const char *msg) {
    out_bios_vga(init_err_style, msg);
    lock_up();
}

static inline void try_setup_step(fernos_error_t err, const char *msg) {
    if (err != FOS_SUCCESS) {
        setup_fatal(msg);
    }
}

static fernos_error_t init_kernel_heap(void) {
    simple_heap_attrs_t shal_attrs = {
        .start = (void *)(_static_area_end + M_4K),
        .end =   (const void *)(_static_area_end + M_4K + M_4M),
        .mmp = (mem_manage_pair_t) {
            .request_mem = alloc_pages,
            .return_mem = free_pages
        },

        .small_fl_cutoff = 0x100,
        .small_fl_search_amt = 0x10,
        .large_fl_search_amt = 0x10
    };
    allocator_t *k_al = new_simple_heap_allocator(shal_attrs);

    if (!k_al) {
        return FOS_NO_MEM;
    }

    set_default_allocator(k_al);

    return FOS_SUCCESS;
}

// Could we maybe have an init_kernel_state too??
// IDK Just a thought!

/**
 * Creates a new kernel state with basically no fleshed out details.
 */
static kernel_state_t *new_blank_kernel_state(allocator_t *al) {
    kernel_state_t *ks = al_malloc(al, sizeof(kernel_state_t));
    if (!ks) {
        return NULL;
    }

    id_table_t *pt = new_id_table(al, FOS_MAX_PROCS);
    if (!pt) {
        al_free(al, ks);
        return NULL;
    }

    ks->al = al;
    ks->curr_thread = NULL;
    ks->proc_table = pt;
    ks->root_proc = NULL;

    return ks;
}

void start_kernel(void) {
    init_err_style = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

    try_setup_step(init_gdt(), "Failed to initialize GDT");
    try_setup_step(init_idt(), "Failed to initialize IDT");
    try_setup_step(init_term(), "Failed to initialize Terminal");
    try_setup_step(init_paging(), "Failed to setup paging");
    try_setup_step(init_kernel_heap(), "Failed to setup kernel heap");



    // Initialize kernel state.

    kernel = new_blank_kernel_state(get_default_allocator());
    if (!kernel) {
        out_bios_vga(init_err_style, "Failed to allocate kernel structure");
        lock_up();
    }

    // Ok, now we need to setup the first process.
    
    phys_addr_t first_user_pd;
    const uint32_t *first_user_esp;

    if (pop_initial_user_info(&first_user_pd, &first_user_esp) != FOS_SUCCESS) {
        out_bios_vga(init_err_style, "Failed to pop user proc info");
        lock_up();
    }


    fernos_error_t err = idtb_request_id(kernel->proc_table, 0);
    if (err != FOS_SUCCESS) {
    }



    test_id_table();
    lock_up();

    // NOW in here we set up the kernel then enter user space???
    // Lot's of questions tbh...


    // Enter Userspace!
    context_return(first_user_pd, first_user_esp);
}
