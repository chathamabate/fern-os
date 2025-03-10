

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
#include "msys/page.h"


int kernel_main(void) {
    /*
    intr_gate_desc_t gd = intr_gate_desc();

    gd_set_selector(&gd, 0x8);
    gd_set_privilege(&gd, 0x0);
    igd_set_base(&gd, timer_handler);

    set_gd(32, gd);
    */

    pt_entry_t pte = not_present_pt_entr();

    pte_set_present(&pte, 1);
    pte_set_base(&pte, (void *)0x80000000);

    term_put_pte(pte);


    return 0;
}

