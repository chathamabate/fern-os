
#include "k_startup/page.h"
#include "k_startup/test/page.h"
#include "k_sys/idt.h"
#include "s_util/err.h"
#include "s_util/test/str.h"
#include "k_sys/debug.h"
#include "k_startup/gdt.h"
#include "k_startup/idt.h"
#include "k_bios_term/term.h"
#include "s_mem/simple_heap.h"

#include "s_data/test/id_table.h"
#include "s_data/test/list.h"
#include "s_mem/test/simple_heap.h"

void timer_handler(void);

uint32_t *alt_task_esp;
int task0_main(void);
int task1_main(void);

static uint32_t *initialize_task_stack(uint32_t *stack_end, void *entry_point) {
    uint32_t *base = stack_end - 11;

    base[10] = read_eflags() | (1 << 9);
    base[9] = 0x8; // Kernel code segment.
    base[8] = (uint32_t)entry_point;
    base[4] = (uint32_t)(base + 8);

    return base;
}

void kernel_init(void) {
    uint8_t init_err_style = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

    if (init_gdt() != FOS_SUCCESS) {
        out_bios_vga(init_err_style, "Failed to set up GDT");
        lock_up();
    }

    if (init_idt() != FOS_SUCCESS) {
        out_bios_vga(init_err_style, "Failed to set up IDT");
        lock_up();
    }

    if (init_term() != FOS_SUCCESS) {
        out_bios_vga(init_err_style, "Failed to set up Terminal");
        lock_up();
    }

    if (init_paging() != FOS_SUCCESS) {
        out_bios_vga(init_err_style, "Failed to set up Paging");
        lock_up();
    }

    // Ok, now setup kernel heap.

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
        out_bios_vga(init_err_style, "Failed to set up Kernel Heap");
        lock_up();
    }

    set_default_allocator(k_al);

    // Ok now for expiremental multi tasking.

    /*
    intr_gate_desc_t timer_gd = intr_gate_desc();

    gd_set_selector(&timer_gd, 0x8);
    gd_set_privilege(&timer_gd, 0);
    igd_set_base(&timer_gd, timer_handler);

    set_gd(32, timer_gd);

    // Timer interrupt pushes 3 double words (32-bits)
    // pushal pushes 8 double words (32-bits)
    //
    // In total 11, dwords.
    // It'd be cool if we had like a stack copy function?
    // Do we really need that tho??
    // Context can just be left on the stack right??
    // Maybe we can have a secondary task??

    uint32_t alt_stack_size = M_4K * 4;

    uint8_t *alt_stack_start = da_malloc(alt_stack_size); 
    if (!alt_stack_start) {
        out_bios_vga(init_err_style, "Couldn't create alt stack");
    }

    uint8_t *alt_stack_end = alt_stack_start + alt_stack_size;

    // Oop, we gotta create the inital stack frame...

    alt_task_esp = initialize_task_stack((uint32_t *)alt_stack_end, (void *)task1_main);
    */
}

// I think if there is a page fault exception... that's bad.
// Not my problem right now tho I guess...
// So we need a task set???
// Task sets like od something???
// IG...?

int counter = 0;

int task0_main(void) {
    /*
    test_shal(
        (mem_manage_pair_t) {
            .request_mem = alloc_pages,
            .return_mem = free_pages
        }
    );
    */

    test_linked_list();

    return 0;
}

int task1_main(void) {
    while (1) {
        term_put_fmt_s("Counter: %d\n", counter);
    }

    return 0;
}



