

#include "fstndutil/str.h"
#include "msys/dt.h"
#include "msys/gdt.h"
#include "msys/debug.h"
#include "msys/intr.h"
#include "term/term.h"
#include "fstndutil/ansii.h"
#include "term/term_sys_helpers.h"
#include "fstndutil/test/str.h"

int kernel_main(void) {
    dtr_val_t idtv = read_idtr();
    term_put_dtr(idtv);

    term_cursor_next_line();

    uint32_t num_entries = dtv_get_num_entries(idtv);
    gate_desc_t *idt = dtv_get_base(idtv);

    term_put_gate_desc(idt[11]);

    /*
    for (uint32_t i = 0; i < num_entries; i++) {
        gate_desc_t gd = idt[i];

        if (gd_get_present(gd)) {
            term_put_gate_desc(gd);
            break;
        }
    }
    */

    return 0;
}

