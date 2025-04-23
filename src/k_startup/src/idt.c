
#include "k_startup/idt.h"
#include "k_sys/dt.h"
#include "k_sys/gdt.h"
#include "k_sys/idt.h"
#include "k_sys/intr.h"
#include "s_util/err.h"
#include "k_sys/debug.h"
#include "k_bios_term/term.h"
#include "s_bridge/intr.h"
#include "k_startup/gdt.h"

gate_desc_t *idt = NULL;

fernos_error_t init_idt(void) {
    idt = (gate_desc_t *)_idt_start;

    intr_gate_desc_t gd = intr_gate_desc();

    gd_set_selector(&gd, KERNEL_CODE_SELECTOR);
    gd_set_privilege(&gd, ROOT_PRVLG);
    igd_set_base(&gd, lock_up_handler);

    // First, all nops.
    for (uint32_t i = 0; i < NUM_IDT_ENTRIES; i++) {
        idt[i] = gd;    
    }

    // 0 - 6

    for (uint32_t i = 32; i < 39; i++) {
        intr_gate_desc_t master_irq_gd = intr_gate_desc();

        gd_set_selector(&master_irq_gd, KERNEL_CODE_SELECTOR);
        gd_set_privilege(&master_irq_gd, ROOT_PRVLG);
        igd_set_base(&master_irq_gd, nop_master_irq_handler);

        idt[i] = master_irq_gd;
    }

    // 7

    intr_gate_desc_t master_irq7_gd = intr_gate_desc();

    gd_set_selector(&master_irq7_gd, KERNEL_CODE_SELECTOR);
    gd_set_privilege(&master_irq7_gd, ROOT_PRVLG);
    igd_set_base(&master_irq7_gd, nop_master_irq7_handler);

    idt[39] = master_irq7_gd;

    // 8 - 14

    for (uint32_t i = 40; i < 47; i++) {
        intr_gate_desc_t slave_irq_gd = intr_gate_desc();

        gd_set_selector(&slave_irq_gd, KERNEL_CODE_SELECTOR);
        gd_set_privilege(&slave_irq_gd, ROOT_PRVLG);
        igd_set_base(&slave_irq_gd, nop_slave_irq_handler);

        idt[i] = slave_irq_gd;
    }

    // 15 
    
    intr_gate_desc_t slave_irq15_gd = intr_gate_desc();

    gd_set_selector(&slave_irq15_gd, KERNEL_CODE_SELECTOR);
    gd_set_privilege(&slave_irq15_gd, ROOT_PRVLG);
    igd_set_base(&slave_irq15_gd, nop_slave_irq15_handler);

    idt[47] = slave_irq15_gd;

    // For timer.
    
    intr_gate_desc_t timer_gd = intr_gate_desc();
    gd_set_selector(&timer_gd, KERNEL_CODE_SELECTOR);
    gd_set_privilege(&timer_gd, ROOT_PRVLG);
    igd_set_base(&timer_gd, timer_handler);

    idt[32] = timer_gd;

    // System call handler.

    intr_gate_desc_t syscall_gd = intr_gate_desc();
    gd_set_selector(&syscall_gd, KERNEL_CODE_SELECTOR);
    gd_set_privilege(&syscall_gd, USER_PRVLG); // Users can invoke syscalls.
    igd_set_base(&syscall_gd, syscall_enter_handler);

    idt[48] = syscall_gd;

    /*
    intr_gate_desc_t test_gd = intr_gate_desc();
    gd_set_selector(&test_gd, KERNEL_CODE_SELECTOR);
    gd_set_privilege(&test_gd, 0);
    igd_set_base(&test_gd, random_handler);

    idt[49] = test_gd;
    */

    dtr_val_t dtv = dtr_val();
    dtv_set_base(&dtv, idt);
    dtv_set_num_entries(&dtv, NUM_IDT_ENTRIES);

    load_idtr(dtv);

    pic_remap(32, 40);
    
    return FOS_SUCCESS;
}
