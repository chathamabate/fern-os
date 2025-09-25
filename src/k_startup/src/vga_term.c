
#include "k_startup/vga_term.h"
#include "s_util/char_display.h"
#include "k_sys/io.h"

/*
 * Basic helpers for using the BIOS VGA Terminal.
 */

#define VGA_ROWS 25
#define VGA_COLS 80
static volatile uint16_t (* const VGA_TERMINAL_BUFFER)[VGA_COLS] = (uint16_t (*)[VGA_COLS])0xB8000;

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
    (void)cd;

}

static void vcd_scroll_down(char_display_t *cd, size_t shift) {

}

static void vcd_set_row(char_display_t *cd, size_t row, char_display_style_t style, char c) {

}

static void vcd_set_grid(char_display_t *cd, char_display_style_t style, char c) {

}


char_display_t _VGA_CD;
char_display_t * const VGA_CD = &_VGA_CD;

static void disable_bios_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

fernos_error_t init_vga_char_display(void) {
    disable_bios_cursor();
    return FOS_E_SUCCESS;
}

