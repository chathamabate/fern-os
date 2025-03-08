

#include "fstndutil/str.h"
#include "msys/gdt.h"
#include "msys/debug.h"
#include "msys/intr.h"
#include "term/term.h"
#include "fstndutil/ansii.h"
#include "term/term_sys_helpers.h"
#include "fstndutil/test/str.h"

int kernel_main(void) {

    gdtr_val_t v = read_gdtr();
    seg_desc_t *d = gdtv_get_base(v);
    term_put_seg_desc(d[4]);

    return 0;
}

