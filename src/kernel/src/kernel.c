

#include "fstndutil/str.h"
#include "msys/gdt.h"
#include "term/term.h"
#include "fstndutil/ansii.h"
#include "term/term_sys_helpers.h"
#include "fstndutil/test/str.h"

int kernel_main(void) {
    term_init();

    term_put_fmt_s("Hello %04X\n", 1);

    if (test_str()) {
        term_put_s("SUCCESS\n");
    } else {
        term_put_s("FAIL\n");
    }
    /*

    seg_desc_t sd = not_present_desc();

    sd_set_present(&sd, 1);
    sd_set_base(&sd, 0x10000000);
    sd_set_limit(&sd, 0xFFFFF);

    term_put_seg_desc(sd);
    */

    return 0;
}

