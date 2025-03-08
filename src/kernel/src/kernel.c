

#include "fstndutil/str.h"
#include "msys/gdt.h"
#include "msys/debug.h"
#include "msys/intr.h"
#include "term/term.h"
#include "fstndutil/ansii.h"
#include "term/term_sys_helpers.h"
#include "fstndutil/test/str.h"

int kernel_main(void) {

    /*
    exec_seg_desc_t esd = exec_seg_desc();
    sd_set_base(&esd, 0x10);
    sd_set_limit(&esd, 0x1F);
    sd_set_privilege(&esd, 3);
    sd_set_gran(&esd, 0);
    esd_set_readable(&esd, 0);
    esd_set_def(&esd, 1);
    esd_set_conforming(&esd, 1);

    term_put_seg_desc(esd);

    data_seg_desc_t dsd = data_seg_desc();
    sd_set_base(&dsd, 0x10000000);
    sd_set_limit(&dsd, 0xFFFFF);
    sd_set_privilege(&dsd, 0);
    sd_set_gran(&esd, 1);
    dsd_set_big(&dsd, 1);
    dsd_set_ex_down(&dsd, 0);
    dsd_set_writable(&dsd, 1);

    term_put_seg_desc(dsd);
    */

    /*
    uint32_t ef = read_eflags();
    term_put_fmt_s("0x%X\n", ef);

    enable_intrs();
    */

    gdtr_val_t v = read_gdtr();
    seg_desc_t *d = gdtv_get_base(v);
    term_put_seg_desc(d[1]);

    //term_put_gdtv(v);



    //load_gdtr(0xFFFFFFFFAAAAAAAAULL);

    /*
    gdtr_val_t gdtv = read_gdtr();
    gdtv_set_size(&gdtv, 0x100);
    gdtv_set_base(&gdtv, 0x0);

    term_put_gdtv(gdtv);
    */

    return 0;
}

