
#include "k_startup/gdt.h"
#include <stdint.h>

seg_desc_t gdt[NUM_GDT_ENTRIES] __attribute__((aligned(0x8)));

fernos_error_t init_gdt(void) {
    for (uint32_t i = 0; i < NUM_GDT_ENTRIES; i++) {
        gdt[i] = not_present_seg_desc();
    }

    exec_seg_desc_t *k_startup_code = &(gdt[1]);  // 0x08
    *k_startup_code = exec_seg_desc();

    sd_set_base(k_startup_code, 0x0);
    sd_set_limit(k_startup_code, 0x80000);
    sd_set_gran(k_startup_code, 0x1);
    sd_set_privilege(k_startup_code, 0x0);

    esd_set_def(k_startup_code, 0x1);
    esd_set_readable(k_startup_code, 0x1);
    esd_set_conforming(k_startup_code, 0x0);

    data_seg_desc_t *k_startup_data = &(gdt[2]);  // 0x10
    *k_startup_data = data_seg_desc();

    sd_set_base(k_startup_data, 0x0);
    sd_set_limit(k_startup_data, 0x80000);
    sd_set_gran(k_startup_data, 0x1);
    sd_set_privilege(k_startup_data, 0x0);

    dsd_set_ex_down(k_startup_data, 0x0);
    dsd_set_writable(k_startup_data, 0x1);
    dsd_set_big(k_startup_data, 0x1);

    exec_seg_desc_t *user_code = &(gdt[3]);  // 0x18
    *user_code = exec_seg_desc();

    sd_set_base(user_code, 0x0);
    sd_set_limit(user_code, 0x80000);
    sd_set_gran(user_code, 0x1);
    sd_set_privilege(user_code, 0x3);

    esd_set_def(user_code, 0x1);
    esd_set_readable(user_code, 0x1);
    esd_set_conforming(user_code, 0x0);

    data_seg_desc_t *user_data = &(gdt[4]);  // 0x20
    *user_data = data_seg_desc();

    sd_set_base(user_data, 0x0);
    sd_set_limit(user_data, 0x80000);
    sd_set_gran(user_data, 0x1);
    sd_set_privilege(user_data, 0x3);

    dsd_set_ex_down(user_data, 0x0);
    dsd_set_writable(user_data, 0x1);
    dsd_set_big(user_data, 0x1);

    // Useful experiments
    /*
    data_seg_desc_t *high_data = &(gdt[5]);  // 0x28
    *high_data = data_seg_desc();

    sd_set_base(high_data, 0x80000000);
    sd_set_limit(high_data, 0x80000);
    sd_set_gran(high_data, 0x1);
    sd_set_privilege(high_data, 0x03);

    dsd_set_ex_down(high_data, 0x0);
    dsd_set_writable(high_data, 0x1);
    dsd_set_big(high_data, 0x1);
    */

    dtr_val_t dtv = dtr_val();

    dtv_set_base(&dtv, gdt);
    dtv_set_num_entries(&dtv, NUM_GDT_ENTRIES);

    load_gdtr(dtv);

    return FOS_SUCCESS;
}
