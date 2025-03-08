
#include "kernel/init.h"
#include "msys/intr.h"
#include "msys/gdt.h"
#include "term/term.h"
#include "term/term_sys_helpers.h"

#define NUM_GDT_ENTRIES 0x10
static seg_desc_t gdt[NUM_GDT_ENTRIES] __attribute__((aligned(0x8)));

static void init_gdt(void) {
    for (size_t i = 0; i < NUM_GDT_ENTRIES; i++) {
        gdt[i] = not_present_gate_desc();
    }

    // I don't totally understand the conforming bit at this time tbh.

    exec_seg_desc_t *kernel_code = &(gdt[1]);  // 0x08
    *kernel_code = exec_seg_desc();

    sd_set_base(kernel_code, 0x0);
    sd_set_limit(kernel_code, 0xFFFFF);
    sd_set_gran(kernel_code, 0x1);
    sd_set_privilege(kernel_code, 0x0);

    esd_set_def(kernel_code, 0x1);
    esd_set_readable(kernel_code, 0x1);
    esd_set_conforming(kernel_code, 0x0);

    data_seg_desc_t *kernel_data = &(gdt[2]);  // 0x10
    *kernel_data = data_seg_desc();

    sd_set_base(kernel_data, 0x0);
    sd_set_limit(kernel_data, 0xFFFFF);
    sd_set_gran(kernel_data, 0x1);
    sd_set_privilege(kernel_data, 0x0);

    dsd_set_ex_down(kernel_data, 0x0);
    dsd_set_writable(kernel_data, 0x1);
    dsd_set_big(kernel_data, 0x1);

    exec_seg_desc_t *user_code = &(gdt[3]);  // 0x18
    *user_code = exec_seg_desc();

    sd_set_base(user_code, 0x0);
    sd_set_limit(user_code, 0xFFFFF);
    sd_set_gran(user_code, 0x1);
    sd_set_privilege(user_code, 0x3);

    esd_set_def(user_code, 0x1);
    esd_set_readable(user_code, 0x1);
    esd_set_conforming(user_code, 0x0);

    data_seg_desc_t *user_data = &(gdt[4]);  // 0x20
    *user_data = data_seg_desc();

    sd_set_base(user_data, 0x0);
    sd_set_limit(user_data, 0xFFFFF);
    sd_set_gran(user_data, 0x1);
    sd_set_privilege(user_data, 0x3);

    dsd_set_ex_down(user_data, 0x0);
    dsd_set_writable(user_data, 0x1);
    dsd_set_big(user_data, 0x1);

    // Actually set it up now plz.

    gdtr_val_t gdtv = gdtr_val();

    gdtv_set_base(&gdtv, gdt);
    gdtv_set_num_entries(&gdtv, NUM_GDT_ENTRIES);

    load_gdtr(gdtv);
}

void init_all(void) {
    disable_intrs();

    init_gdt();

    term_init();

    enable_intrs();
}

