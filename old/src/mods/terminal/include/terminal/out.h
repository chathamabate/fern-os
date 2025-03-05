
#ifndef TERMINAL_OUT_H
#define TERMINAL_OUT_H

// NOTE: Much of this is taken from OSDev wiki.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Hardware text mode color constants. */
typedef enum _vga_color_t {
	VGA_COLOR_BLACK = 0,
	VGA_COLOR_BLUE = 1,
	VGA_COLOR_GREEN = 2,
	VGA_COLOR_CYAN = 3,
	VGA_COLOR_RED = 4,
	VGA_COLOR_MAGENTA = 5,
	VGA_COLOR_BROWN = 6,
	VGA_COLOR_LIGHT_GREY = 7,
	VGA_COLOR_DARK_GREY = 8,
	VGA_COLOR_LIGHT_BLUE = 9,
	VGA_COLOR_LIGHT_GREEN = 10,
	VGA_COLOR_LIGHT_CYAN = 11,
	VGA_COLOR_LIGHT_RED = 12,
	VGA_COLOR_LIGHT_MAGENTA = 13,
	VGA_COLOR_LIGHT_BROWN = 14,
	VGA_COLOR_WHITE = 15,
} vga_color_t;

//static const size_t VGA_WIDTH = 80;
//static const size_t VGA_HEIGHT = 25;

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
static volatile uint16_t * const TERMINAL_BUFFER = (uint16_t *)0xB8000;

static inline uint8_t vga_entry_color(vga_color_t fg, vga_color_t bg) {
	return fg | bg << 4;
}

static inline uint8_t vga_color_flip(uint8_t color) {
    return (color << 4) | (color >> 4);
}
 
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
	return (uint16_t) uc | (uint16_t) color << 8;
}

static inline uint8_t vga_extract_color(uint16_t entry) {
    return (uint8_t)(entry >> 8);
}

static inline vga_color_t vga_extract_bg_color(uint8_t color) {
    return (vga_color_t)(color >> 4);
}

static inline vga_color_t vga_extract_fg_color(uint8_t color) {
    return (vga_color_t)(color & 0x0F);
}

static inline unsigned char vga_extract_char(uint16_t entry) {
    return (unsigned char)(entry & 0xFF);
}

static inline volatile uint16_t *vga_get_entry_p(size_t row, size_t col) {
    return &(TERMINAL_BUFFER[(row * VGA_WIDTH) + col]);
}

static inline void vga_set_entry(size_t row, size_t col, uint16_t entry) {
    *vga_get_entry_p(row, col) = entry;
}

static inline uint16_t vga_get_entry(size_t row, size_t col) {
    return *vga_get_entry_p(row, col);
}

static inline void vga_set_entry_color_p(volatile uint16_t *entry, uint8_t color) {
    *entry = (*entry & 0x00FF) | ((uint16_t)color << 8);
}

static inline void vga_set_entry_color(size_t row, size_t col, uint8_t color) {
    vga_set_entry_color_p(vga_get_entry_p(row, col), color);
}

static inline void vga_set_entry_char_p(volatile uint16_t *entry, unsigned char c) {
    *entry = (*entry & 0xFF00) | (uint16_t)c;
}

static inline void vga_set_entry_char(size_t row, size_t col, unsigned char c) {
    vga_set_entry_char_p(vga_get_entry_p(row, col), c);
}

// Must be called before all below calls.
void term_init(void);

void term_show_cursor(bool p);
void term_clear(void);

size_t term_get_cursor_row(void);
size_t term_get_cursor_col(void);
void term_set_cursor(size_t row, size_t col);

uint8_t term_get_output_style(void);
void term_set_output_style(uint8_t color);


// Scroll up a single line.
// Cursor position remains unchanged.
void term_scroll_down(void);

// Move the cursor down a line, if we are already
// at the bottom of the screen, scroll up!
void term_cursor_next_line(void);

// This call outputs a single character to the terminal at the cursors position.
// The cursor then is advanced.
// If ther cursor is at the end of a line, the screen will be scrolled one line up.
//
// Special Characters Supported:
// 
// \n   - Skip to start of next line.
// \r   - Go to beginning of this line.
//
void term_put_c(char c);

// Print a string onto the terminal.
//
// This supports control characters supported by putc.
// This supports ANSII colors found in util/ansii.h
void term_put_s(const char *s);

// Prints a string to the terminal using str_fmt.
// NOTE: Make sure your resulting string does overflow
// the static Buffer size noted here.
#define TERM_FMT_BUF_SIZE 1024
void term_put_fmt_s(const char *fmt, ...);

// Print out the values in the stack before this function is called.
// slots is the number of 32-bit values to trace up the stack.
void term_put_trace(uint32_t slots, uint32_t *esp);

#endif
