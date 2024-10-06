
#include "terminal/out.h"

static char to_hex(unsigned int x) {
    if (x > 9) {
        return 'A' + (x - 10);
    }

    return '0' + x;
}

void print_hex(unsigned int x) {
    for (int i = 7; i >= 0; i--) {
        char hex = to_hex((x >> (4 * i)) & 0xF);
        TERMINAL_BUFFER[7 - i] = vga_entry(
                hex, 
                vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK)
        );
    }
}

void terminal_clear(void) {
    for (size_t r = 0; r < VGA_HEIGHT; r++) {
        for (size_t c = 0; c < VGA_WIDTH; c++) {
            TERMINAL_BUFFER[(r * VGA_WIDTH) + c] = 
                vga_entry(' ', vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        }
    }
}

