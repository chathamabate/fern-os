
#include "k_startup/kernel.h"
#include "k_bios_term/term.h"
#include "k_startup/page.h"
#include "k_startup/gdt.h"
#include "k_startup/idt.h"
#include "k_startup/state.h"
#include "k_startup/process.h"
#include "k_startup/test/page.h"
#include "k_startup/thread.h"
#include "k_startup/tss.h"
#include "k_startup/action.h"
#include "k_sys/page.h"
#include "u_startup/main.h"
#include "s_mem/simple_heap.h"
#include "s_bridge/ctx.h"
#include "s_util/constraints.h"
#include "k_startup/test/page.h"
#include "k_startup/test/page_helpers.h"

#include "s_data/test/list.h"
#include "s_data/test/wait_queue.h"

#include <stdint.h>

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

kernel_state_t *kernel = NULL;

static fernos_error_t init_kernel_state(void) {
    kernel = new_da_kernel_state();
    if (!kernel) {
        return FOS_NO_MEM;
    }

    proc_id_t pid = idtb_pop_id(kernel->proc_table);
    if (pid == idtb_null_id(kernel->proc_table)) {
        return FOS_NO_MEM;
    }

    uint32_t user_pd = pop_initial_user_info();
    if (user_pd == NULL_PHYS_ADDR) {
        return FOS_UNKNWON_ERROR;
    }

    process_t *proc = new_da_process(pid, user_pd, NULL);
    if (!proc) {
        return FOS_NO_MEM;
    }

    kernel->root_proc = proc;
    idtb_set(kernel->proc_table, pid, proc);

    // Now create our first thread!

    thread_id_t tid = idtb_pop_id(proc->thread_table);
    if (tid == idtb_null_id(proc->thread_table)) {
        return FOS_NO_MEM;
    }

    thread_t *thr = new_da_thread(tid, proc, user_main, NULL);
    if (!thr) {
        return FOS_NO_MEM;
    }

    proc->main_thread = thr;
    idtb_set(proc->thread_table, tid, thr);

    // Finally, schedule our first thread!
    ks_schedule_thread(kernel, thr);

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
    try_setup_step(init_kernel_state(), "Failed to setup kernel state");

    test_timed_wait_queue();
    lock_up();

    set_syscall_action(fos_syscall_action);
    set_timer_action(fos_timer_action);
    
    return_to_ctx(&(kernel->curr_thread->ctx));
}
