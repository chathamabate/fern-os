

#include "fstndutil/str.h"
#include "msys/dt.h"
#include "msys/gdt.h"
#include "msys/debug.h"
#include "msys/idt.h"
#include "msys/intr.h"
#include "term/term.h"
#include "fstndutil/ansii.h"
#include "term/term_sys_helpers.h"
#include "fstndutil/test/str.h"

int kernel_main(void) {
    // Let's just relax a little bit man, it's all going to be OK!
    term_put_s("HELLO\nHELLO\n");
    while (1);
    // OK, looks like this is kinda working??
    /*
    dtr_val_t dt = read_gdtr();
    seg_desc_t *gdt = dtv_get_base(dt);

    term_put_seg_desc(gdt[5]);
    */

    /*
    intr_gate_desc_t gd = intr_gate_desc();

    gd_set_selector(&gd, 0x8);
    gd_set_privilege(&gd, 0x0);
    igd_set_base(&gd, (uint32_t)hndlr);

    set_gd(32, gd);

    term_put_gate_desc(gd);
    */

    return 0;
}

