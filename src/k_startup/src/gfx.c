
#include "k_startup/gfx.h"

#include "os_defs.h"
#include "s_util/constraints.h"
#include "k_sys/debug.h"
#include "s_util/str.h"
#include "s_gfx/mono_fonts.h"

void gfx_to_screen(gfx_screen_t *screen, gfx_buffer_t *frame) {
    // We'll put a memory barrier both before and after cause why not.
    __sync_synchronize();

    int32_t rendered_width = MIN(screen->width, frame->width);
    int32_t rendered_height = MIN(screen->height, frame->height);

    for (int32_t r = 0; r < rendered_height; r++) {
        volatile gfx_color_t *screen_row = (volatile gfx_color_t *)((volatile uint8_t *)(screen->buffer) + (screen->pitch * r));
        gfx_color_t *frame_row = frame->buffer + (frame->width * r);

        // It's possible a memcpy would be safe here, but I am not taking any chances with MMIO.
        for (int32_t c = 0; c < rendered_width; c++) {
            screen_row[c] = frame_row[c]; 
        }
    }

    __sync_synchronize();
}

static gfx_screen_t screen = {
    .buffer = NULL // Not initialized at first.
};

gfx_screen_t * const SCREEN = &screen;

/**
 * Remember that `init_screen` will fail if the memory mapped buffer doesnt have
 * pixels in these dimmensions.
 */
static gfx_color_t back_buffer_arr[FERNOS_GFX_HEIGHT][FERNOS_GFX_WIDTH];

static gfx_buffer_t back_buffer = {
    .al = NULL, // Statically allocated.
    
    .width = FERNOS_GFX_WIDTH,
    .height = FERNOS_GFX_HEIGHT,
    .buffer = (gfx_color_t *)back_buffer_arr
};

gfx_buffer_t * const BACK_BUFFER = &back_buffer;

#define DIRECT_TERM_ROWS (40U)
#define DIRECT_TERM_COLS (80U)
#define DIRECT_TERM_W_SCALE (1U)
#define DIRECT_TERM_H_SCALE (1U)
#define DIRECT_TERM_PAD (1U)
#define DIRECT_TERM_FONT (ASCII_MONO_8X16)

static term_cell_t direct_term_arr[DIRECT_TERM_ROWS][DIRECT_TERM_COLS];

/**
 * Lucky for us, terminal buffers can now be statically allocated at compile time!
 */
static term_buffer_t direct_term = {
    .al = NULL, // Static buffer.
    
    .rows = DIRECT_TERM_ROWS,
    .cols = DIRECT_TERM_COLS,

    .cursor_row = 0,
    .cursor_col = 0,

    .default_cell = {
        .c = ' ',
        .style = term_style(TC_WHITE, TC_BLACK)
    },
    .curr_style = term_style(TC_WHITE, TC_BLACK),

    .buf = (term_cell_t *)direct_term_arr
};

term_buffer_t * const DIRECT_TERM = &direct_term;

fernos_error_t init_screen(const m2_info_start_t *m2_info) {
    if (!m2_info) {
        return FOS_E_BAD_ARGS;
    }

    if (screen.buffer) {
        // The screen buffer has already been initialized!
        return FOS_E_STATE_MISMATCH;
    }

    // Otherwise, let's search for the

    const m2_info_tag_base_t *tags = m2_info_tag_area(m2_info);

    const m2_info_tag_framebuffer_t *fb_tag = 
        (const m2_info_tag_framebuffer_t *)m2_find_info_tag(tags, M2_ITT_FRAMEBUFFER);
    if (!fb_tag) { // Couldn't find a framebuffer definition!
        return FOS_E_STATE_MISMATCH;
    }

    // Ok, there is a framebuffer tag!    

    uint64_t end = fb_tag->addr + (fb_tag->pitch * fb_tag->height);

    if (end < fb_tag->addr || end > 0xFFFFFFFF) {
        return FOS_E_STATE_MISMATCH; // If there was wrap OR the buffer exceeds the 32-bit memory area,
                                     // don't even try to write.
    }

    // Confirm the screen is setup as expected! 
    // (Remember, the final page of the epilogue will be unmappable in kernel space)
    if (fb_tag->addr < FOS_AREA_END || end > ALIGN(0xFFFFFFFF, M_4K) ||
            fb_tag->width != FERNOS_GFX_WIDTH ||
            fb_tag->height != FERNOS_GFX_HEIGHT || fb_tag->bpp != FERNOS_GFX_BPP) {

        // An error at this point means that a frame buffer was setup, but not in the expected
        // configuration. So, let's just write 0xFF's to the entire buffer area!

        __sync_synchronize();
        for (volatile uint8_t *ptr = (volatile uint8_t *)(uint32_t)(fb_tag->addr); 
                ptr < (uint8_t *)(uint32_t)end; ptr++) {
            *ptr = 0xFF;
        }
        __sync_synchronize();

        return FOS_E_STATE_MISMATCH;
    }

    // Now actually initialize the screen buffer struct in this file! 

    screen = (gfx_screen_t) {
        .pitch = fb_tag->pitch,
        .width = fb_tag->width,
        .height = fb_tag->height,
        .buffer = (gfx_color_t *)(uint32_t)(fb_tag->addr)
    };

    tb_default_clear(DIRECT_TERM);

    return FOS_E_SUCCESS;
}

void gfx_direct_term_render(void) {
    // Black out the back buffer.
    gfx_clear(BACK_BUFFER, BASIC_ANSI_PALETTE->colors[TC_BLACK]);

    const int32_t width = DIRECT_TERM_W_SCALE * (int32_t)(DIRECT_TERM_FONT->char_width) * DIRECT_TERM_COLS;
    const int32_t height = DIRECT_TERM_H_SCALE * (int32_t)(DIRECT_TERM_FONT->char_height) * DIRECT_TERM_ROWS;

    // Let's center this bad boy.
    const int32_t x = (FERNOS_GFX_WIDTH - width) / 2;
    const int32_t y = (FERNOS_GFX_HEIGHT - height) / 2;

    gfx_draw_term_buffer(BACK_BUFFER, NULL, NULL, 
            DIRECT_TERM, DIRECT_TERM_FONT, BASIC_ANSI_PALETTE, 
            x, y, DIRECT_TERM_W_SCALE, DIRECT_TERM_H_SCALE);

    // Now let's draw a nice lil' rectangle around this guy.
    gfx_draw_rect(BACK_BUFFER, NULL, x - 1 - DIRECT_TERM_PAD, y - 1 - DIRECT_TERM_PAD, 
            width + (2 * (1 + DIRECT_TERM_PAD)), 
            height + (2 * (1 + DIRECT_TERM_PAD)), 
            1, BASIC_ANSI_PALETTE->colors[TC_WHITE]);

    // Finally render out the back buffer!
    gfx_render();
}

void gfx_direct_put_s(const char *s) {
    tb_put_s(DIRECT_TERM, s);
    gfx_direct_term_render();
}

void gfx_direct_fatal(const char *msg) {
    gfx_direct_put_s(msg);
    lock_up();
}

/*
 * OLD FUNCTIONS Adapted to new direct terminal!
 */

#include "s_util/ansi.h"

#define _ESP_ID_FMT ANSI_GREEN_FG "%%esp" ANSI_RESET
#define _ESP_INDEX_FMT ANSI_CYAN_FG "%2X" ANSI_RESET "(" _ESP_ID_FMT ")"

#define _EVEN_ROW_FMT _ESP_INDEX_FMT " = " ANSI_LIGHT_GREY_FG "%08X" ANSI_RESET "\n"
#define _ODD_ROW_FMT _ESP_INDEX_FMT " = " ANSI_BRIGHT_LIGHT_GREY_FG "%08X" ANSI_RESET "\n"

#define _ESP_VAL_ROW_FMT "   " _ESP_ID_FMT "  = " ANSI_BRIGHT_LIGHT_GREY_FG "%08X" ANSI_RESET "\n"

void gfx_direct_trace(uint32_t slots, uint32_t *esp) {
    char buf[100];

    for (size_t i = 0; i < slots; i++) {
        size_t j = slots - 1 - i; 

        if (j % 2 == 0) {
            str_fmt(buf, _EVEN_ROW_FMT, j * sizeof(uint32_t), esp[j]);
        } else {
            str_fmt(buf, _ODD_ROW_FMT, j * sizeof(uint32_t), esp[j]);
        }

        gfx_direct_put_s(buf);
    }

    str_fmt(buf, _ESP_VAL_ROW_FMT, esp);
    gfx_direct_put_s(buf);
}

static void gfx_direct_64bit(uint64_t v) {
    gfx_direct_put_fmt_s(ANSI_CYAN_FG "[4] " ANSI_RESET "%08X" "\n", (uint32_t)(v >> 32));
    gfx_direct_put_fmt_s(ANSI_CYAN_FG "[0] " ANSI_RESET "%08X" "\n", (uint32_t)v);
}

#define LABEL_SPACE 8
static void gfx_direct_pair(const char *label, const char *val) {
    char buf[LABEL_SPACE + 1];

    str_la(buf, LABEL_SPACE, ' ', label);
    gfx_direct_put_fmt_s(ANSI_LIGHT_GREY_FG "%s" ANSI_RESET ": %s\n", buf, val);
}

static void gfx_direct_cond(const char *label, uint8_t cond) {
    gfx_direct_pair(label, cond 
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

void gfx_direct_seg_selector(seg_selector_t ssr) {
    char buf[128];

    uint16_t ind = ssr_get_ind(ssr);
    uint8_t rpl = ssr_get_rpl(ssr);
    uint8_t ti = ssr_get_ti(ssr);

    str_fmt(buf, "%u", ind);
    gfx_direct_pair("Index", buf);

    const char *priv_name = privilege_as_literal(rpl);
    gfx_direct_pair("RPL", priv_name);

    gfx_direct_pair("Table", ti 
            ? ANSI_GREEN_FG "LOCAL" ANSI_RESET 
            : ANSI_RED_FG "GLOBAL" ANSI_RESET);
}

void gfx_direct_seg_desc(seg_desc_t sd) {
    gfx_direct_64bit(sd);

    char buf[128];

    uint8_t present = sd_get_present(sd);

    gfx_direct_cond("Prsnt", present);

    if (!present) {
        return;
    }

    uint32_t base = sd_get_base(sd);
    str_fmt(buf, "0x%8X", base);
    gfx_direct_pair("Base", buf); 

    uint32_t limit = sd_get_limit(sd);
    str_fmt(buf, "0x%8X", limit);
    gfx_direct_pair("Limit", buf);

    uint8_t privilege = sd_get_privilege(sd);
    const char *priv_name = privilege_as_literal(privilege);

    gfx_direct_pair("Priv", priv_name);

    gfx_direct_cond("4K Gran", sd_get_gran(sd));

    // Now for type specific info.

    uint8_t type = sd_get_type(sd);

    const char *type_name = NULL;

    if ((type & 0x18) == 0x10) {
        type_name = ANSI_CYAN_FG "DATA" ANSI_RESET;
        gfx_direct_pair("Type", type_name);

        data_seg_desc_t dsd = (data_seg_desc_t)sd;

        gfx_direct_cond("Write", dsd_get_writable(dsd));
        gfx_direct_cond("Ex Down", dsd_get_ex_down(dsd));
        gfx_direct_cond("Big", dsd_get_big(dsd));

    } else if ((type & 0x18) == 0x18) {
        type_name = ANSI_RED_FG "EXEC" ANSI_RESET;
        gfx_direct_pair("Type", type_name);

        exec_seg_desc_t esd = (exec_seg_desc_t)sd;

        gfx_direct_cond("Read", esd_get_readable(esd));
        gfx_direct_cond("Cnfrm", esd_get_conforming(esd));
        gfx_direct_cond("Dflt", esd_get_def(esd));

    } else {
        type_name = ANSI_MAGENTA_FG "SYS" ANSI_RESET;
        gfx_direct_pair("Type", type_name);

        // I guess leave this empty?
    }
}

void gfx_direct_gate_desc(gate_desc_t gd) {
    gfx_direct_64bit(gd);

    uint8_t present = gd_get_present(gd);
    gfx_direct_cond("Prsnt", present);

    if (!present) {
        return;
    }

    char buf[128];

    seg_selector_t ssr = gd_get_selector(gd);
    str_fmt(buf, "0x%4X", ssr);
    gfx_direct_pair("Sel", buf);

    uint8_t dpl = gd_get_privilege(gd);
    gfx_direct_pair("DPL", privilege_as_literal(dpl));

    uint32_t offset;

    uint8_t type = gd_get_type(gd);
    switch (type) {
    case GD_TASK_GATE_TYPE:
        gfx_direct_pair("Type", ANSI_CYAN_FG "TASK" ANSI_RESET);
        break;
    case GD_INTR_GATE_TYPE:
        gfx_direct_pair("Type", ANSI_RED_FG "INTR" ANSI_RESET);

        offset = igd_get_base(gd);
        str_fmt(buf, "0x%8X", offset);
        gfx_direct_pair("Offset", buf);
        break;
    case GD_TRAP_GATE_TYPE:
        gfx_direct_pair("Type", ANSI_GREEN_FG "TRAP" ANSI_RESET);

        offset = trgd_get_base(gd);
        str_fmt(buf, "0x%8X", offset);
        gfx_direct_pair("Offset", buf);
        break;
    default:
        gfx_direct_pair("Type", ANSI_BROWN_FG "UNKNOWN" ANSI_RESET);
        break;
    }
}

void gfx_direct_dtr(dtr_val_t dtv) {
    char buf[128];

    str_fmt(buf, "0x%8X", dtv_get_base(dtv));
    gfx_direct_pair("Base", buf);

    str_fmt(buf, "0x%8X", dtv_get_size(dtv));
    gfx_direct_pair("Size", buf);

    str_fmt(buf, "%u", dtv_get_num_entries(dtv));
    gfx_direct_pair("Entries", buf);
}

void gfx_direct_pte(pt_entry_t pte) {
    char buf[128];

    uint8_t p = pte_get_present(pte);
    gfx_direct_cond("Prsnt", p);

    if (!p) {
        return;
    }

    uint8_t wr = pte_get_writable(pte);
    gfx_direct_cond("Write", wr);

    uint8_t priv = pte_get_user(pte);
    if (priv) {
        gfx_direct_pair("Priv", ANSI_GREEN_FG "USER" ANSI_RESET);
    } else {
        gfx_direct_pair("Priv", ANSI_GREEN_FG "ROOT" ANSI_RESET);
    }

    uint8_t avail = pte_get_avail(pte);
    str_fmt(buf, "0x%2X", avail);
    gfx_direct_pair("Avail", buf);

    phys_addr_t base = pte_get_base(pte);
    str_fmt(buf, "0x%8X", base);
    gfx_direct_pair("Base", buf);
}
