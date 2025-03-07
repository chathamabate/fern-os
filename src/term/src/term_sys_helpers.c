
#include "term/term_sys_helpers.h"
#include "term/term.h"

#include "msys/gdt.h"

void term_put_seg_desc(seg_desc_t sd) {
    uint8_t present = sd_get_present(sd);

    if (!present) {
        term_put_s("Not Present\n");
        return;
    }

    uint32_t base = sd_get_base(sd);
    term_put_fmt_s("Base : 0x%X\n", base);

    uint32_t limit = sd_get_limit(sd);
    term_put_fmt_s("Limit: 0x%X\n", limit);
}
