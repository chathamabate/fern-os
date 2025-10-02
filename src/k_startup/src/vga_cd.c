
#include "k_startup/vga_cd.h"
#include "s_util/char_display.h"
#include "s_util/ansi.h"
#include "k_sys/io.h"
#include "s_util/str.h"
#include <stdarg.h>

/*
 * Basic helpers for using the BIOS VGA Terminal.
 */

#define VGA_ROWS 25
#define VGA_COLS 80
static volatile uint16_t (* const VGA_TERMINAL_BUFFER)[VGA_COLS] = (volatile uint16_t (*)[VGA_COLS])0xB8000;

void out_bios_vga(char_display_style_t style, const char *str) {
    uint32_t i = 0;
    char c;

    while ((c = str[i]) != '\0') {
       VGA_TERMINAL_BUFFER[0][i] = (uint8_t)style | c;
       i++; 
    }
}

static void delete_vga_char_display(char_display_t *cd) {
    (void)cd;
}

static void vcd_get_c(char_display_t *cd, size_t row, size_t col, char_display_style_t *style, char *c) {
    (void)cd;

    uint16_t entry =  VGA_TERMINAL_BUFFER[row][col];

    if (style) {
        *style = (char_display_style_t)((entry & 0xFF00) >> 8);
    }

    if (c) {
        *c = (char)(entry & 0xFF);
    }
}

static void vcd_set_c(char_display_t *cd, size_t row, size_t col, char_display_style_t style, char c) {
    (void)cd;
    
    VGA_TERMINAL_BUFFER[row][col] = ((style & 0xFF) << 8) | c;
}

static void vcd_scroll_up(char_display_t *cd, size_t shift) {
    if (shift == 0) {
        return;
    }

    if (shift > cd->rows) {
        shift = cd->rows;
    }

    for (size_t row = 0; row < cd->rows - shift; row++) {
        mem_cpy((void *)(VGA_TERMINAL_BUFFER[row]), 
                (const void *)(VGA_TERMINAL_BUFFER[row + shift]), sizeof(VGA_TERMINAL_BUFFER[row]));
    }

    for (size_t row = cd->rows - shift; row < cd->rows; row++) {
        for (size_t col = 0; col < cd->cols; col++) {
            VGA_TERMINAL_BUFFER[row][col] = (cd->default_style << 8) | ' ';
        }
    }

}

static void vcd_scroll_down(char_display_t *cd, size_t shift) {
    if (shift == 0) {
        return;
    }

    if (shift > cd->rows) {
        shift = cd->rows;
    }

    for (size_t row = cd->rows - 1; row != shift - 1; row--) {
        mem_cpy((void *)VGA_TERMINAL_BUFFER[row], 
                (const void *)VGA_TERMINAL_BUFFER[row - shift], 
                sizeof(VGA_TERMINAL_BUFFER[row]));
    }

    for (size_t row = 0; row < shift; row++) {
        for (size_t col = 0; col < cd->cols; col++) {
            VGA_TERMINAL_BUFFER[row][col] = (cd->default_style << 8) | ' ';
        }
    }
}

static void vcd_set_row(char_display_t *cd, size_t row, char_display_style_t style, char c) {
    if (row < cd->rows) {
        for (size_t col = 0; col < cd->cols; col++) {
            VGA_TERMINAL_BUFFER[row][col] = (style << 8) | c;
        }
    }
}

static void vcd_set_grid(char_display_t *cd, char_display_style_t style, char c) {
    for (size_t row = 0; row < cd->rows; row++) {
        for (size_t col = 0; col < cd->cols; col++) {
            VGA_TERMINAL_BUFFER[row][col] = (style << 8) | c;
        }
    }
}

static const char_display_impl_t _VGA_CD_IMPL = {
    .delete_char_display = delete_vga_char_display,
    .cd_get_c = vcd_get_c,
    .cd_set_c = vcd_set_c,
    .cd_scroll_up = vcd_scroll_up,
    .cd_scroll_down = vcd_scroll_down,
    .cd_set_row = vcd_set_row,
    .cd_set_grid = vcd_set_grid
};


static char_display_t _VGA_CD;
char_display_t * const VGA_CD = &_VGA_CD;

static void disable_bios_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

fernos_error_t init_vga_char_display(void) {
    disable_bios_cursor();

    init_char_display_base(VGA_CD, &_VGA_CD_IMPL, VGA_ROWS, VGA_COLS, 
            char_display_style(CDC_WHITE, CDC_BLACK));

    // Now before we return, let's clear the display and display its cursor.

    vcd_set_grid(VGA_CD, VGA_CD->default_style, ' ');
    vcd_set_c(VGA_CD, 0, 0, cds_flip(VGA_CD->default_style), ' ');

    return FOS_E_SUCCESS;
}

/*
 * Legacy compatibilty functions!
 */

static char VGA_TERM_FMT_BUF[TERM_FMT_BUF_SIZE];
void term_put_fmt_s(const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    str_vfmt(VGA_TERM_FMT_BUF, fmt, va);
    va_end(va);

    term_put_s(VGA_TERM_FMT_BUF);
}

#define _ESP_ID_FMT ANSI_GREEN_FG "%%esp" ANSI_RESET
#define _ESP_INDEX_FMT ANSI_CYAN_FG "%2X" ANSI_RESET "(" _ESP_ID_FMT ")"

#define _EVEN_ROW_FMT _ESP_INDEX_FMT " = " ANSI_LIGHT_GREY_FG "%08X" ANSI_RESET "\n"
#define _ODD_ROW_FMT _ESP_INDEX_FMT " = " ANSI_BRIGHT_LIGHT_GREY_FG "%08X" ANSI_RESET "\n"

#define _ESP_VAL_ROW_FMT "   " _ESP_ID_FMT "  = " ANSI_BRIGHT_LIGHT_GREY_FG "%08X" ANSI_RESET "\n"

void term_put_trace(uint32_t slots, uint32_t *esp) {
    char buf[100];

    for (size_t i = 0; i < slots; i++) {
        size_t j = slots - 1 - i; 

        if (j % 2 == 0) {
            str_fmt(buf, _EVEN_ROW_FMT, j * sizeof(uint32_t), esp[j]);
        } else {
            str_fmt(buf, _ODD_ROW_FMT, j * sizeof(uint32_t), esp[j]);
        }

        term_put_s(buf);
    }

    str_fmt(buf, _ESP_VAL_ROW_FMT, esp);
    term_put_s(buf);
}

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
