

#include "term/term.h"

int kernel_main(void) {
    term_init();

    term_put_s("Hello World\n");

    return 0;
}

