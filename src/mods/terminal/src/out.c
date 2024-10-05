
#include "terminal/out.h"

void terminal_clear(void) {
    for (size_t r = 0; r < VGA_HEIGHT; r++) {
        for (size_t c = 0; c < VGA_WIDTH; c++) {
            TERMINAL_BUFFER[(r * VGA_WIDTH) + c] = 
                vga_entry(' ', vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        }
    }
}
