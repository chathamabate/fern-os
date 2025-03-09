

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

void hndlr(void) {
    disable_intrs();

    term_put_s("HERE\n");

    lock_up();
}

int kernel_main(void) {
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

