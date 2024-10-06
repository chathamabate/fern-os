
#include "terminal/out.h"
#include "msys/io.h"


void disable_bios_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

static bool show_cursor = false;
static size_t cursor_row = 0;
static size_t cursor_col = 0;

static void flip_color(size_t row, size_t col) {
    uint16_t entry = get_vga_entry(row, col);

    uint8_t color = vga_extract_color(entry);
    uint8_t new_color = vga_entry_color(
            vga_extract_bg_color(color),
            vga_extract_fg_color(color)
    );

    set_vga_entry_color(row, col, new_color);

}

void term_set_cursor(bool p) {
    if (p != show_cursor) {
        flip_color(cursor_row, cursor_col); 
        show_cursor = p;
    }
}

void term_clear(void) {
    for (size_t r = 0; r < VGA_HEIGHT; r++) {
        for (size_t c = 0; c < VGA_WIDTH; c++) {
            set_vga_entry(r, c, 
                vga_entry(' ', vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK)));
        }
    }

    cursor_row = 0;
    cursor_col = 0;

    if (show_cursor) {
        flip_color(cursor_row, cursor_col); 
    }
}

