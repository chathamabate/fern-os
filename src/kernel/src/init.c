
#include "kernel/init.h"
#include "msys/intr.h"
#include "term/term.h"

static void init_gdt(void) {
    
}

void init_all(void) {
    disable_intrs();

    init_gdt();
    term_init();

    enable_intrs();
}

