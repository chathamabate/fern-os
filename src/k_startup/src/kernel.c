
#include "k_startup/fwd_defs.h"
#include "k_startup/page.h"
#include "k_startup/process.h"
#include "k_startup/test/page.h"
#include "k_startup/thread.h"
#include "k_startup/tss.h"
#include "k_sys/idt.h"
#include "k_sys/intr.h"
#include "k_sys/page.h"
#include "k_sys/tss.h"
#include "s_mem/allocator.h"
#include "s_util/err.h"
#include "s_util/test/str.h"
#include "k_sys/debug.h"
#include "k_startup/gdt.h"
#include "k_startup/idt.h"
#include "k_bios_term/term.h"
#include "s_mem/simple_heap.h"
#include "s_bridge/ctx.h"

#include "s_data/test/id_table.h"
#include "s_data/test/list.h"
#include "s_mem/test/simple_heap.h"
#include "k_startup/state.h"
#include "u_startup/main.h"

/*
void fos_syscall_action(phys_addr_t pd, const uint32_t *esp, uint32_t id, uint32_t arg) {
    (void)id;
    (void)arg;
    context_return_value(pd, esp, 0);
}

void fos_timer_action(phys_addr_t pd, const uint32_t *esp) {
    context_return(pd, esp);
}
*/

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

/**
 * On error this call does no cleanup. Errors at this point are seen as fatal.
 */
static fernos_error_t init_kernel_state(void) {
    // Initialize kernel state.

    kernel = new_da_blank_kernel_state();
    if (!kernel) {
        return FOS_NO_MEM;
    }

    // Ok, now we need to setup the first process.
    
    phys_addr_t first_user_pd;
    const uint32_t *first_user_esp;

    PROP_ERR(pop_initial_user_info(&first_user_pd, &first_user_esp));

    // Reserve 0 for the root process. 
    PROP_ERR(idtb_request_id(kernel->proc_table, 0)); 

    process_t *root = new_da_process(0, first_user_pd, NULL);
    if (!root) {
        return FOS_NO_MEM;
    }

    idtb_set(kernel->proc_table, 0, root);

    // NOTE: The main thread of a process need not alway have id 0.
    // The first thread of the first process will though.
    PROP_ERR(idtb_request_id(root->thread_table, 0));

    thread_t *root_thr = new_da_thread(0, root, first_user_esp);
    idtb_set(root->thread_table, 0, root_thr);

    root->main_thread = root_thr;

    // Schedule the root thread.
    root_thr->next_thread = root_thr;
    root_thr->prev_thread = root_thr;

    kernel->curr_thread = root_thr;
    root_thr->state = THREAD_STATE_SCHEDULED;

    return FOS_SUCCESS;
}

void thingy(void) {
    term_put_s("HELLO\n");
    while (1);
}

void start_kernel(void) {
    init_err_style = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

    try_setup_step(init_gdt(), "Failed to initialize GDT");
    try_setup_step(init_idt(), "Failed to initialize IDT");
    try_setup_step(init_global_tss(), "Failed to initialize TSS");
    try_setup_step(init_term(), "Failed to initialize Terminal");
    try_setup_step(init_paging(), "Failed to setup paging");
    try_setup_step(init_kernel_heap(), "Failed to setup kernel heap");
    //try_setup_step(init_kernel_state(), "Failed to setup kernel state");
    
    uint32_t user_pd;
    const uint32_t *user_esp;

    try_setup_step(pop_initial_user_info(&user_pd, &user_esp), "Failed to pop user info");

    user_ctx_t ctx = { 
        .cr3 = user_pd,

        .eip = (uint32_t)user_main,
        .cs = USER_CODE_SELECTOR,
        .eflags = read_eflags(),

        .esp = (uint32_t)user_esp,
        .ss = USER_DATA_SELECTOR
    };

    enter_user_ctx(&ctx);
}

// We need the page fault handler...  That's the primary road block right now.
