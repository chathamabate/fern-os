
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
    term_put_fmt_s(ANSII_CYAN_FG "[4] " ANSII_RESET "%08X" "\n", (uint32_t)(sd >> 32));
    term_put_fmt_s(ANSII_CYAN_FG "[0] " ANSII_RESET "%08X" "\n", (uint32_t)sd);

    char buf[128];

    uint8_t present = sd_get_present(sd);

    term_put_cond("Prsnt", present);

    if (!present) {
        return;
    }

    uint32_t base = sd_get_base(sd);
    str_fmt(buf, "%8X", base);
    term_put_pair("Base", buf); 

    uint32_t limit = sd_get_limit(sd);
    str_fmt(buf, "%8X", limit);
    term_put_pair("Limit", buf);

    uint32_t privilege = sd_get_privilege(sd);
    const char *priv_name = NULL;

    if (privilege == 0) {
        priv_name = ANSII_RED_FG "ROOT" ANSII_RESET;
    } else if (privilege == 3) {
        priv_name = ANSII_GREEN_FG "USER" ANSII_RESET;
    } else {
        priv_name = ANSII_BROWN_FG "UNKNOWN" ANSII_RESET;
    }

    term_put_pair("Priv", priv_name);

    term_put_cond("4K Gran", sd_get_gran(sd));

    // Now for type specific info.

    uint8_t type = sd_get_type(sd);

    const char *type_name = NULL;

    if ((type & 0x18) == 0x10) {
        type_name = ANSII_CYAN_FG "DATA" ANSII_RESET;
        term_put_pair("Type", type_name);

        data_seg_desc_t dsd = (data_seg_desc_t)sd;

        term_put_cond("Write", dsd_get_writable(dsd));
        term_put_cond("Ex Down", dsd_get_ex_down(dsd));
        term_put_cond("Big", dsd_get_big(dsd));

    } else if ((type & 0x18) == 0x18) {
        type_name = ANSII_RED_FG "EXEC" ANSII_RESET;
        term_put_pair("Type", type_name);

        exec_seg_desc_t esd = (exec_seg_desc_t)sd;

        term_put_cond("Read", esd_get_readable(esd));
        term_put_cond("Cnfrm", esd_get_conforming(esd));
        term_put_cond("Dflt", esd_get_def(esd));

    } else {
        type_name = ANSII_MAGENTA_FG "SYS" ANSII_RESET;
        term_put_pair("Type", type_name);

        // I guess leave this empty?
    }
}

void term_put_gdtv(gdtr_val_t gdtv) {
    char buf[128];

    str_fmt(buf, "%8X", gdtv_get_base(gdtv));
    term_put_pair("Base", buf);

    str_fmt(buf, "%8X", gdtv_get_size(gdtv));
    term_put_pair("Size", buf);

    str_fmt(buf, "%u " ANSII_LIGHT_GREY_FG "(Decimal)" ANSII_RESET, gdtv_get_num_entries(gdtv));
    term_put_pair("Entries", buf);
}
