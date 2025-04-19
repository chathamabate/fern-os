
#include "k_startup/page.h"
#include "k_startup/test/page.h"
#include "k_sys/idt.h"
#include "k_sys/page.h"
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

void fos_syscall_action(phys_addr_t pd, const uint32_t *esp, uint32_t id, uint32_t arg) {
    term_put_fmt_s("Syscall (%u, %u)\n", id, arg);
    context_return_value(pd, esp, 0);
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

    set_syscall_action(fos_syscall_action);

    phys_addr_t first_user_pd;
    const uint32_t *first_user_esp;

    if (pop_initial_user_info(&first_user_pd, &first_user_esp) != FOS_SUCCESS) {
        out_bios_vga(init_err_style, "Failed to pop user proc info");
        lock_up();
    }

    // Well the page table could be incorrect...
    // Also the context return could be wrong??


    term_put_fmt_s("Shared: %X, %X\n", _ro_shared_start, _ro_shared_end);
    term_put_fmt_s("User:   %X, %X\n", _ro_user_start, _ro_user_end);
    term_put_fmt_s("Kernel: %X, %X\n", _ro_kernel_start, _ro_kernel_end);
    term_put_fmt_s("Context Ret: %X\n", context_return);
    term_put_fmt_s("User PD: %X\n", first_user_pd);

    context_return(first_user_pd, first_user_esp);
}



