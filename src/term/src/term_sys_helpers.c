
#include "term/term_sys_helpers.h"
#include "term/term.h"
#include "fstndutil/ansii.h"
#include "fstndutil/str.h"
#include <stdint.h>

#include "msys/gdt.h"

#define LABEL_SPACE 8
static void term_put_pair(const char *label, const char *val) {
    char buf[LABEL_SPACE + 1];

    str_la(buf, LABEL_SPACE, ' ', label);
    term_put_fmt_s(ANSII_LIGHT_GREY_FG "%s" ANSII_RESET ": %s\n", buf, val);
}

static void term_put_cond(const char *label, uint8_t cond) {
    term_put_pair(label, cond 
        ? ANSII_GREEN_FG "ON" ANSII_RESET 
        : ANSII_RED_FG "OFF" ANSII_RESET
    );
}

void term_put_seg_desc(seg_desc_t sd) {
    term_put_fmt_s(ANSII_CYAN_FG "[4] " ANSII_GREEN_FG "%X" ANSII_RESET "\n", (uint32_t)(sd >> 32));
    term_put_fmt_s(ANSII_CYAN_FG "[0] " ANSII_GREEN_FG "%X" ANSII_RESET "\n", (uint32_t)sd);

    term_put_cond("Pres", 1);
    term_put_cond("Gran", 0);

    /*

    uint8_t present = sd_get_present(sd);

    if (!present) {
        term_put_s("Not Present\n");
        return;
    }

    // Hmmm, my head kinda hurts tbh, might take a break.

    uint32_t base = sd_get_base(sd);
    term_put_fmt_s("Base : 0x%X\n", base);

    uint32_t limit = sd_get_limit(sd);
    term_put_fmt_s("Limit: 0x%X\n", limit);
    */
}
