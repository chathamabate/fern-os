
#include "terminal/out.h"
#include "msys/io.h"

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

void term_set_cursor(size_t row, size_t col) {
    term_state.cursor_row = row;
    term_state.cursor_col = col;
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
    if (term_state.show_cursor) {
        term_flip_cursor_color(); 
    }
    
    _term_scroll_down();

    // Put cursor back if needed.
    if (term_state.show_cursor) {
        term_flip_cursor_color(); 
    }
}

void term_cursor_next_line(void) {
    if (term_state.show_cursor) {
        term_flip_cursor_color();
    }

    if (term_state.cursor_row == VGA_HEIGHT - 1) {
        // When on the last line, let's scroll down.
        _term_scroll_down();
    } else {
        // Otherwise, actually move cursor.
        term_state.cursor_row++;
    }

    // Always set cursor column to the beginning of the line.
    term_state.cursor_col = 0;

    if (term_state.show_cursor) {
        term_flip_cursor_color();
    }
}

void term_outc(char c) {
    // Let's overwrite the cursor.
    vga_set_entry(
            term_state.cursor_row, term_state.cursor_col,
            vga_entry(c, term_state.output_style)
    );

    term_state.cursor_col++;

    if (term_state.cursor_col == VGA_WIDTH) {
        term_state.cursor_col = 0;
        term_state.cursor_row++;

        if (term_state.cursor_row == VGA_HEIGHT) {
            _term_scroll_down();
            term_state.cursor_row = VGA_HEIGHT - 1;
        }
    }
    
    if (term_state.show_cursor) {
        term_flip_cursor_color();
    }
}

void term_puts(const char *s) {
    const char *i = s;
    char c;

    while ((c = *(i++))) {
       term_outc(c); 
    }
}

