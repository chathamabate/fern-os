
#include "term/term.h"
#include "msys/io.h"
#include "util/ansii.h"
#include "util/str.h"

static void disable_bios_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

typedef struct _term_state_t {
    // When the cursor is shown, it will flip the style of its
    // position.
    bool show_cursor;

    uint8_t default_style;

    // Color of text which will be output to the terminal.
    uint8_t output_style;

    size_t cursor_row;
    size_t cursor_col;
} term_state_t;

static term_state_t term_state;

void term_init(void) {
    disable_bios_cursor();

    term_state.show_cursor = true;
    term_state.default_style = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    term_state.output_style = term_state.default_style;
    term_state.cursor_row = 0;
    term_state.cursor_col = 0;

    term_clear();
}

static void term_flip_cursor_color(void) {
    uint8_t color = vga_extract_color(
            vga_get_entry(term_state.cursor_row, term_state.cursor_col)); 
    vga_set_entry_color(term_state.cursor_row, term_state.cursor_col, vga_color_flip(color));
}

void term_show_cursor(bool p) {
    if (p != term_state.show_cursor) {
        term_flip_cursor_color();
        term_state.show_cursor = p;
    }
}

void term_clear(void) {
    for (size_t r = 0; r < VGA_HEIGHT; r++) {
        for (size_t c = 0; c < VGA_WIDTH; c++) {
            vga_set_entry(r, c, 
                vga_entry(' ', term_state.default_style));
        }
    }
    
    if (term_state.show_cursor) {
        term_flip_cursor_color();
    }
}

size_t term_get_cursor_row(void) {
    return term_state.cursor_row;
}

size_t term_get_cursor_col(void) {
    return term_state.cursor_col;
}

// Quick helper for removing and restoring the visible cursor
// from the screen.
static inline void _term_cursor_guard(void) {
    if (term_state.show_cursor) {
        term_flip_cursor_color(); 
    }
}

void term_set_cursor(size_t row, size_t col) {
    _term_cursor_guard();

    term_state.cursor_row = row;
    term_state.cursor_col = col;

    _term_cursor_guard();
}

uint8_t term_get_output_style(void) {
    return term_state.output_style;
}

void term_set_output_style(uint8_t color) {
    term_state.output_style = color;
}

// Just do the copying. (NO CURSOR TOGGLING)
static void _term_scroll_down(void) {
    for (size_t r = 0; r < VGA_HEIGHT - 1; r++) {
        for (size_t c = 0; c < VGA_WIDTH; c++) {
            vga_set_entry(r, c, vga_get_entry(r + 1, c));
        }
    }

    for (size_t c = 0; c < VGA_WIDTH; c++) {
        vga_set_entry(VGA_HEIGHT - 1, c, vga_entry(' ', term_state.default_style));
    }
}

void term_scroll_down(void) {
    // First let's forget about the cursor all together.
    _term_cursor_guard();
    
    _term_scroll_down();

    // Put cursor back if needed.
    _term_cursor_guard();
}

static void _term_cursor_next_line(void) {
    if (term_state.cursor_row == VGA_HEIGHT - 1) {
        // When on the last line, let's scroll down.
        _term_scroll_down();
    } else {
        // Otherwise, actually move cursor.
        term_state.cursor_row++;
    }

    // Always set cursor column to the beginning of the line.
    term_state.cursor_col = 0;
}

void term_cursor_next_line(void) {
    _term_cursor_guard();
    _term_cursor_next_line();
    _term_cursor_guard();
}

// NO CURSOR TOGGLING
static void _term_advance_cursor(void) {
    term_state.cursor_col++;

    if (term_state.cursor_col == VGA_WIDTH) {
        term_state.cursor_col = 0;
        _term_cursor_next_line();
    }
}

static void _term_put_c(char c) {
    switch (c) {
    case '\n':
        _term_cursor_next_line();
        break;
    case '\r':
        term_state.cursor_col = 0;
        break;
    default:
        vga_set_entry(term_state.cursor_row, term_state.cursor_col, 
                vga_entry(c, term_state.output_style));
        _term_advance_cursor();
        break;
    }
}

void term_put_c(char c) {
    _term_cursor_guard();
    _term_put_c(c);
    _term_cursor_guard();
}

// Returns number of characters consumed.
static size_t term_interp_esc_seq(const char *s) {
    const char *i = s;
    char c;

    if (*i != '[') {
        return 0;   
    }

    uint8_t num = 0;

    while ((c = *(++i)) && ('0' <= c && c <= '9')) {
        num *= 10;
        num += c - '0';
    }

    if (c == 'm') {
        i++; // Consume m.
             
        uint8_t curr_style = term_state.output_style;
        vga_color_t curr_fg_color = vga_extract_fg_color(curr_style);
        vga_color_t curr_bg_color = vga_extract_bg_color(curr_style);
             
        if (num == 0) {
            term_state.output_style = term_state.default_style;
        } else if (30 <= num && num <= 37) {
            num -= 30;
            term_state.output_style =
                vga_entry_color(num, curr_bg_color);
        } else if (40 <= num && num <= 47) {
            num -= 40;
            term_state.output_style =
                vga_entry_color(curr_fg_color, num);
        } else if (90 <= num && num <= 97) {
            num -= 82;
            term_state.output_style =
                vga_entry_color(num, curr_bg_color);
        } else if (100 <= num && num <= 107) {
            num -= 92;
            term_state.output_style =
                vga_entry_color(curr_fg_color, num);
        }
    }
    
    return i - s; 
}

void term_put_s(const char *s) {
    _term_cursor_guard();

    const char *i = s;
    char c;

    while ((c = *(i++))) {
        if (c == 0x1B) {
            i += term_interp_esc_seq(i);
        } else {
            _term_put_c(c); 
        }
    }

    _term_cursor_guard();
}

static char term_fmt_buf[TERM_FMT_BUF_SIZE];
void term_put_fmt_s(const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    str_vfmt(term_fmt_buf, fmt, va);
    va_end(va);

    term_put_s(term_fmt_buf);
}


#define _ESP_ID_FMT ANSII_GREEN_FG "%%esp" ANSII_RESET
#define _ESP_INDEX_FMT ANSII_CYAN_FG "%u" ANSII_RESET "(" _ESP_ID_FMT ")"

#define _EVEN_ROW_FMT _ESP_INDEX_FMT " = " ANSII_LIGHT_GREY_FG "0x%X" ANSII_RESET "\n"
#define _ODD_ROW_FMT _ESP_INDEX_FMT " = " ANSII_BRIGHT_LIGHT_GREY_FG "0x%X" ANSII_RESET "\n"

#define _ESP_VAL_ROW_FMT _ESP_ID_FMT " = " ANSII_BRIGHT_LIGHT_GREY_FG "0x%X" ANSII_RESET "\n"

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

