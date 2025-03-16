

#include "s_util/str.h"
#include "k_sys/dt.h"
#include "k_sys/gdt.h"
#include "k_sys/debug.h"
#include "k_sys/idt.h"
#include "k_sys/intr.h"
#include "term/term.h"
#include "s_util/ansii.h"
#include "term/term_sys_helpers.h"
#include "s_util/test/str.h"
#include "k_sys/page.h"
#include "k_startup/page.h"

#include "os_defs.h"

void gpf_hndlr(void) {
    term_put_s("Oh no, a general protection fault\n");

    lock_up();
}

void pf_hndlr(void) {
    term_put_s("OH NO A PF\n");

    lock_up();
}

int kernel_main(void) {

    term_put_fmt_s("%X %X\n",
            identity_pts, identity_pts + 1);

    return 0;
}

