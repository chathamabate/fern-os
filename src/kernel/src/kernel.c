

#include "term/term.h"

int kernel_main(void) {
    term_init();
    term_put_s("HELLO\n");

    while (1);
}
