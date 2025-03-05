

#include "term/term.h"
#include "util/test/str.h"

int kernel_main(void) {
    term_init();
    if (test_str()) {
        term_put_s("SUCCESS\n");
    }

    while (1);
}
