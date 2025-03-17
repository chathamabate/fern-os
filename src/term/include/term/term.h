
#pragma once

// NOTE: Much of this is taken from OSDev wiki.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "k_sys/debug.h"
#include "s_util/err.h"

// Must be called before all below calls.
fernos_error_t init_term(void);

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

