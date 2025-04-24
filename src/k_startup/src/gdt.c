
#include "k_startup/gdt.h"
#include "k_sys/gdt.h"
#include "k_sys/tss.h"
#include <stdint.h>

static seg_desc_t *gdt = NULL;

fernos_error_t init_gdt(void) {
    gdt = (seg_desc_t *)_gdt_start;

    for (uint32_t i = 0; i < NUM_GDT_ENTRIES; i++) {
        gdt[i] = not_present_seg_desc();
    }

    exec_seg_desc_t *kernel_code = &(gdt[1]);  // 0x08
    *kernel_code = exec_seg_desc();

    sd_set_base(kernel_code, 0x0);
    sd_set_limit(kernel_code, 0xFFFFF);
    sd_set_gran(kernel_code, 0x1);
    sd_set_privilege(kernel_code, ROOT_PRVLG);

    esd_set_def(kernel_code, 0x1);
    esd_set_readable(kernel_code, 0x1);
    esd_set_conforming(kernel_code, 0x0);

    data_seg_desc_t *kernel_data = &(gdt[2]);  // 0x10
    *kernel_data = data_seg_desc();

    sd_set_base(kernel_data, 0x0);
    sd_set_limit(kernel_data, 0xFFFFF);
    sd_set_gran(kernel_data, 0x1);
    sd_set_privilege(kernel_data, ROOT_PRVLG);

    dsd_set_ex_down(kernel_data, 0x0);
    dsd_set_writable(kernel_data, 0x1);
    dsd_set_big(kernel_data, 0x1);

    exec_seg_desc_t *user_code = &(gdt[3]);  // 0x18
    *user_code = exec_seg_desc();

    sd_set_base(user_code, 0x0);
    sd_set_limit(user_code, 0xFFFFF);
    sd_set_gran(user_code, 0x1);
    sd_set_privilege(user_code, USER_PRVLG);

    esd_set_def(user_code, 0x1);
    esd_set_readable(user_code, 0x1);
    esd_set_conforming(user_code, 0x0);

    data_seg_desc_t *user_data = &(gdt[4]);  // 0x20
    *user_data = data_seg_desc();

    sd_set_base(user_data, 0x0);
    sd_set_limit(user_data, 0xFFFFF);
    sd_set_gran(user_data, 0x1);
    sd_set_privilege(user_data, USER_PRVLG);

    dsd_set_ex_down(user_data, 0x0);
    dsd_set_writable(user_data, 0x1);
    dsd_set_big(user_data, 0x1);

    task_seg_desc_t *task_desc = &(gdt[5]); // 0x28
    *task_desc = task_seg_desc();

    sd_set_base(task_desc, (uint32_t)_tss_start);
    sd_set_limit(task_desc, sizeof(task_state_segment_t) - 1);
    sd_set_gran(task_desc, 0x0);
    sd_set_privilege(task_desc, ROOT_PRVLG);

    tsd_set_busy(task_desc, 0);

    dtr_val_t dtv = dtr_val();

    dtv_set_base(&dtv, gdt);
    dtv_set_num_entries(&dtv, NUM_GDT_ENTRIES);

    load_gdtr(dtv);

    return FOS_SUCCESS;
}
