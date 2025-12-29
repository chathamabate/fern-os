
#include "k_startup/kernel.h"
#include "k_startup/vga_cd.h"
#include "k_startup/page.h"
#include "k_startup/gdt.h"
#include "k_startup/idt.h"
#include "k_startup/state.h"
#include "k_startup/process.h"
#include "k_startup/tss.h"
#include "k_startup/action.h"
#include "k_sys/page.h"
#include "u_startup/main.h"
#include "s_mem/simple_heap.h"
#include "s_bridge/ctx.h"
#include "s_util/constraints.h"
#include "k_startup/ata_block_device.h"
#include "s_data/id_table.h"
#include "k_sys/kb.h"

#include "s_block_device/cached_block_device.h"
#include "s_block_device/block_device.h"
#include "s_block_device/fat32.h"
#include "s_block_device/fat32.h"
#include "s_block_device/fat32_file_sys.h"
#include "s_block_device/file_sys.h"
#include "k_startup/plugin_fut.h"
#include "k_startup/plugin_fs.h"
#include "k_startup/plugin_kb.h"
#include "k_startup/plugin_pipe.h"
#include "k_startup/plugin_vga_cd.h"

#include "s_util/char_display.h"
#include "k_startup/vga_cd.h"
#include "k_sys/m2.h"
#include "k_startup/gfx.h"

#include <stdint.h>

static inline void setup_fatal(const char *msg) {
    out_bios_vga((uint8_t)char_display_style(CDC_BRIGHT_RED, CDC_BLACK), msg);
    lock_up();
}

static inline void try_setup_step(fernos_error_t err, const char *msg) {
    if (err != FOS_E_SUCCESS) {
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
        return FOS_E_NO_MEM;
    }

    set_default_allocator(k_al);

    return FOS_E_SUCCESS;
}

static void now(fernos_datetime_t *dt) {
    *dt = (fernos_datetime_t) { 0 };
}

kernel_state_t *kernel = NULL;

static void init_kernel_state(void) {
    kernel = new_da_kernel_state();
    if (!kernel) {
        setup_fatal("Failed to allocate kernel state");
    }
    // Let's setup our first user process.

    proc_id_t pid = idtb_pop_id(kernel->proc_table);
    if (pid == idtb_null_id(kernel->proc_table)) {
        setup_fatal("Failed to pop root pid");
    }

    phys_addr_t user_pd = pop_initial_user_pd_copy();
    if (user_pd == NULL_PHYS_ADDR) {
        setup_fatal("Failed to get user PD");
    }

    process_t *proc = new_da_process(pid, user_pd, NULL);
    if (!proc) {
        setup_fatal("Failed to allocate first process");
    }

    kernel->root_proc = proc;
    idtb_set(kernel->proc_table, pid, proc);

    thread_t *thr = proc_new_thread(kernel->root_proc, 
            (uintptr_t)user_main, 0, 0, 0);
    if (!thr) {
        setup_fatal("Failed to allocate first thread");
    }

    // Finally, schedule our first thread!
    thread_schedule(thr, &(kernel->schedule));
}

static void init_kernel_plugins(void) {
    fernos_error_t err;

    plugin_t *plg_fut = new_plugin_fut(kernel);
    if (!plg_fut) {
        setup_fatal("Failed to create Futex plugin");
    }
    try_setup_step(ks_set_plugin(kernel, PLG_FUTEX_ID, plg_fut), "Failed to set Futex plugin");

    block_device_t *bd = new_da_cached_block_device(
        get_ata_block_device(),
        512, 0, true
    );

    if (!bd) {
        setup_fatal("Failed to created block device");
    }

    file_sys_t *fs;
    err = parse_new_da_fat32_file_sys(bd, 0, 0, true, now, &fs);
    if (err != FOS_E_SUCCESS) {
        err = init_fat32(bd, 0, bd_num_sectors(bd), 8);
        if (err != FOS_E_SUCCESS) {
            setup_fatal("Failed to init FAT32");
        }

        // NOTE: This is a temporary fix and VERY VERY dangerous. 
        // It basically resets the entire disk if it fails to parse it the first time.
        err = parse_new_da_fat32_file_sys(bd, 0, 0, true, now, &fs);
        if (err != FOS_E_SUCCESS) {
            setup_fatal("Failed to create FAT32 FS");
        }
    }

    plugin_t *plg_fs = new_plugin_fs(kernel, fs);
    if (!plg_fs) {
        setup_fatal("Failed to create File System plugin");
    }
    try_setup_step(ks_set_plugin(kernel, PLG_FILE_SYS_ID, plg_fs), "Failed to set FS Plugin in the kernel");

    plugin_t *plg_kb = new_plugin_kb(kernel);
    if (!plg_kb) {
        setup_fatal("Failed to create Keyboard plugin");
    }
    try_setup_step(ks_set_plugin(kernel, PLG_KEYBOARD_ID, plg_kb), "Failed to set up KB Plugin in the kernel");

    plugin_t *plg_vga_cd = new_plugin_vga_cd(kernel);
    if (!plg_vga_cd) {
        setup_fatal("Failed to create VGA Character Display plugin");
    }
    try_setup_step(ks_set_plugin(kernel, PLG_VGA_CD_ID, plg_vga_cd), "Failed to set VGA CD Plugin in the kernel");

    plugin_t *plg_pipe = new_plugin_pipe(kernel);
    if (!plg_pipe) {
        setup_fatal("Failed to create Pipe plugin");
    }
    try_setup_step(ks_set_plugin(kernel, PLG_PIPE_ID, plg_pipe), "Failed to set Pipe Plugin in the kernel");
}

void start_kernel(uint32_t m2_magic, const m2_info_start_t *m2_info) {
    if (m2_magic != M2_EAX_MAGIC) {
        lock_up();
    }

    if (init_screen(m2_info) != FOS_E_SUCCESS) {
        lock_up();
    }
    
    // We must setup the screen before doing anything else!
        
    try_setup_step(validate_constraints(), "Failed to validate memory areas");

    try_setup_step(init_gdt(), "Failed to initialize GDT");
    try_setup_step(init_idt(), "Failed to initialize IDT");
    try_setup_step(init_global_tss(), "Failed to initialize TSS");

    // Put these in place so we can catch errors in the following setup steps.
    set_gpf_action(fos_lock_up_action);
    set_pf_action(fos_lock_up_action);

    try_setup_step(init_vga_char_display(), "Failed to init VGA terminal");
    try_setup_step(init_paging(), "Failed to setup paging");

    try_setup_step(init_kernel_heap(), "Failed to setup kernel heap");
    try_setup_step(init_kb(), "Failed to init keyboard");

    init_kernel_state();
    init_kernel_plugins();

    // Now put in the real actions.
    set_gpf_action(fos_gpf_action);
    set_pf_action(fos_pf_action);

    set_syscall_action(fos_syscall_action);
    set_timer_action(fos_timer_action);
    set_irq1_action(fos_irq1_action);

    gfx_clear(SCREEN, gfx_color(255, 0, 0));
    lock_up();

    thread_t *first_thread = (thread_t *)(kernel->schedule.head);
    return_to_ctx(&(first_thread->ctx));
}
