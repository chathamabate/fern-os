
#include "k_bios_term/term.h"
#include "k_bios_term/term_sys_helpers.h"
#include "k_sys/page.h"
#include "s_util/ansi.h"
#include "s_util/str.h"
#include <stdint.h>

#include "k_sys/gdt.h"

static void term_put_64bit(uint64_t v) {
    term_put_fmt_s(ANSI_CYAN_FG "[4] " ANSI_RESET "%08X" "\n", (uint32_t)(v >> 32));
    term_put_fmt_s(ANSI_CYAN_FG "[0] " ANSI_RESET "%08X" "\n", (uint32_t)v);
}

#define LABEL_SPACE 8
static void term_put_pair(const char *label, const char *val) {
    char buf[LABEL_SPACE + 1];

    str_la(buf, LABEL_SPACE, ' ', label);
    term_put_fmt_s(ANSI_LIGHT_GREY_FG "%s" ANSI_RESET ": %s\n", buf, val);
}

static void term_put_cond(const char *label, uint8_t cond) {
    term_put_pair(label, cond 
        ? ANSI_GREEN_FG "ON" ANSI_RESET 
        : ANSI_RED_FG "OFF" ANSI_RESET
    );
}

const char *privilege_as_literal(uint8_t privilege) {
    if (privilege == 0) {
        return ANSI_RED_FG "ROOT" ANSI_RESET;
    } 
    if (privilege == 3) {
        return  ANSI_GREEN_FG "USER" ANSI_RESET;
    } 

    return ANSI_BROWN_FG "UNKNOWN" ANSI_RESET;
}

void term_put_seg_selector(seg_selector_t ssr) {
    char buf[128];

    uint16_t ind = ssr_get_ind(ssr);
    uint8_t rpl = ssr_get_rpl(ssr);
    uint8_t ti = ssr_get_ti(ssr);

    str_fmt(buf, "%u", ind);
    term_put_pair("Index", buf);

    const char *priv_name = privilege_as_literal(rpl);
    term_put_pair("RPL", priv_name);

    term_put_pair("Table", ti 
            ? ANSI_GREEN_FG "LOCAL" ANSI_RESET 
            : ANSI_RED_FG "GLOBAL" ANSI_RESET);
}

void term_put_seg_desc(seg_desc_t sd) {
    term_put_64bit(sd);

    char buf[128];

    uint8_t present = sd_get_present(sd);

    term_put_cond("Prsnt", present);

    if (!present) {
        return;
    }

    uint32_t base = sd_get_base(sd);
    str_fmt(buf, "0x%8X", base);
    term_put_pair("Base", buf); 

    uint32_t limit = sd_get_limit(sd);
    str_fmt(buf, "0x%8X", limit);
    term_put_pair("Limit", buf);

    uint8_t privilege = sd_get_privilege(sd);
    const char *priv_name = privilege_as_literal(privilege);

    term_put_pair("Priv", priv_name);

    term_put_cond("4K Gran", sd_get_gran(sd));

    // Now for type specific info.

    uint8_t type = sd_get_type(sd);

    const char *type_name = NULL;

    if ((type & 0x18) == 0x10) {
        type_name = ANSI_CYAN_FG "DATA" ANSI_RESET;
        term_put_pair("Type", type_name);

        data_seg_desc_t dsd = (data_seg_desc_t)sd;

        term_put_cond("Write", dsd_get_writable(dsd));
        term_put_cond("Ex Down", dsd_get_ex_down(dsd));
        term_put_cond("Big", dsd_get_big(dsd));

    } else if ((type & 0x18) == 0x18) {
        type_name = ANSI_RED_FG "EXEC" ANSI_RESET;
        term_put_pair("Type", type_name);

        exec_seg_desc_t esd = (exec_seg_desc_t)sd;

        term_put_cond("Read", esd_get_readable(esd));
        term_put_cond("Cnfrm", esd_get_conforming(esd));
        term_put_cond("Dflt", esd_get_def(esd));

    } else {
        type_name = ANSI_MAGENTA_FG "SYS" ANSI_RESET;
        term_put_pair("Type", type_name);

        // I guess leave this empty?
    }
}

void term_put_gate_desc(gate_desc_t gd) {
    term_put_64bit(gd);

    uint8_t present = gd_get_present(gd);
    term_put_cond("Prsnt", present);

    if (!present) {
        return;
    }

    char buf[128];

    seg_selector_t ssr = gd_get_selector(gd);
    str_fmt(buf, "0x%4X", ssr);
    term_put_pair("Sel", buf);

    uint8_t dpl = gd_get_privilege(gd);
    term_put_pair("DPL", privilege_as_literal(dpl));

    uint32_t offset;

    uint8_t type = gd_get_type(gd);
    switch (type) {
    case GD_TASK_GATE_TYPE:
        term_put_pair("Type", ANSI_CYAN_FG "TASK" ANSI_RESET);
        break;
    case GD_INTR_GATE_TYPE:
        term_put_pair("Type", ANSI_RED_FG "INTR" ANSI_RESET);

        offset = igd_get_base(gd);
        str_fmt(buf, "0x%8X", offset);
        term_put_pair("Offset", buf);
        break;
    case GD_TRAP_GATE_TYPE:
        term_put_pair("Type", ANSI_GREEN_FG "TRAP" ANSI_RESET);

        offset = trgd_get_base(gd);
        str_fmt(buf, "0x%8X", offset);
        term_put_pair("Offset", buf);
        break;
    default:
        term_put_pair("Type", ANSI_BROWN_FG "UNKNOWN" ANSI_RESET);
        break;
    }
}

void term_put_dtr(dtr_val_t dtv) {
    char buf[128];

    str_fmt(buf, "0x%8X", dtv_get_base(dtv));
    term_put_pair("Base", buf);

    str_fmt(buf, "0x%8X", dtv_get_size(dtv));
    term_put_pair("Size", buf);

    str_fmt(buf, "%u", dtv_get_num_entries(dtv));
    term_put_pair("Entries", buf);
}

void term_put_pte(pt_entry_t pte) {
    char buf[128];

    uint8_t p = pte_get_present(pte);
    term_put_cond("Prsnt", p);

    if (!p) {
        return;
    }

    uint8_t wr = pte_get_writable(pte);
    term_put_cond("Write", wr);

    uint8_t priv = pte_get_user(pte);
    if (priv) {
        term_put_pair("Priv", ANSI_GREEN_FG "USER" ANSI_RESET);
    } else {
        term_put_pair("Priv", ANSI_GREEN_FG "ROOT" ANSI_RESET);
    }

    uint8_t avail = pte_get_avail(pte);
    str_fmt(buf, "0x%2X", avail);
    term_put_pair("Avail", buf);

    phys_addr_t base = pte_get_base(pte);
    str_fmt(buf, "0x%8X", base);
    term_put_pair("Base", buf);
}
