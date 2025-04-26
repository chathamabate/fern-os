
#include "k_startup/kernel.h"
#include "k_bios_term/term.h"
#include "k_startup/page.h"
#include "k_startup/gdt.h"
#include "k_startup/idt.h"
#include "k_startup/state.h"
#include "k_startup/process.h"
#include "k_startup/thread.h"
#include "k_startup/tss.h"
#include "k_startup/action.h"
#include "k_sys/page.h"
#include "u_startup/main.h"
#include "s_mem/simple_heap.h"
#include "s_bridge/ctx.h"
#include "s_util/constraints.h"
#include <stdint.h>

static uint8_t init_err_style;

static kernel_state_t *kernel = NULL;

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
        .start = (void *)FOS_FREE_AREA_START,
        .end =   (void *)FOS_FREE_AREA_END,
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

void start_kernel(void) {
    init_err_style = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

    try_setup_step(validate_constraints(), "Failed to validate memory areas");

    try_setup_step(init_gdt(), "Failed to initialize GDT");
    try_setup_step(init_idt(), "Failed to initialize IDT");
    try_setup_step(init_global_tss(), "Failed to initialize TSS");

    set_gpf_action(fos_gpf_action);

    try_setup_step(init_term(), "Failed to initialize Terminal");
    try_setup_step(init_paging(), "Failed to setup paging");
    try_setup_step(init_kernel_heap(), "Failed to setup kernel heap");
    
    uint32_t user_pd = pop_initial_user_info();
    if (user_pd == NULL_PHYS_ADDR) {
        setup_fatal("Failed to get first user PD");
    }

    // Let's assume we are using thread 0.

    uint8_t *ustack_end = (uint8_t *)FOS_THREAD_STACK_END(0);
    uint8_t *ustack_start = ustack_end - M_4K;

    const void *true_e;
    try_setup_step(pd_alloc_pages(user_pd, true, ustack_start, ustack_end, &true_e), 
            "Failed to allocated first stack");

    set_timer_action(fos_timer_action);
    set_syscall_action(fos_syscall_action);

    user_ctx_t ctx = { 
        .ds = USER_DATA_SELECTOR,
        .cr3 = user_pd,

        .eax = (uint32_t)user_main,
        .ecx = (uint32_t)4,

        .eip = (uint32_t)thread_entry_routine,
        .cs = USER_CODE_SELECTOR,
        .eflags = read_eflags() | (1 << 9),

        .esp = (uint32_t)ustack_end,
        .ss = USER_DATA_SELECTOR
    };

    return_to_ctx(&ctx);
}
