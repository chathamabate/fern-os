
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "s_mem/allocator.h"
#include "s_util/misc.h"

/*
 * This is a newer version of the old "char_display_t" design.
 * I have moved this from `s_util` here to `s_data` because it requires an allocator.
 */


/**
 * A Terminal Color uses 4 bits.
 *
 * The first 3 are the base color.
 * The 4th is the bright bit.
 */
typedef uint8_t term_color_t;

/*
 * Base Colors
 */

#define TC_BLACK           (0)
#define TC_BLUE            (1)
#define TC_GREEN           (2)
#define TC_CYAN            (3)
#define TC_RED             (4)
#define TC_MAGENTA         (5)
#define TC_BROWN           (6)
#define TC_LIGHT_GREY      (7)

#define TC_BRIGHT (1U << 3)

/*
 * Bright Alternatives
 */

#define TC_BRIGHT_BLACK           (TC_BRIGHT | 0)
#define TC_BRIGHT_BLUE            (TC_BRIGHT | 1)
#define TC_BRIGHT_GREEN           (TC_BRIGHT | 2)
#define TC_BRIGHT_CYAN            (TC_BRIGHT | 3)
#define TC_BRIGHT_RED             (TC_BRIGHT | 4)
#define TC_BRIGHT_MAGENTA         (TC_BRIGHT | 5)
#define TC_BRIGHT_BROWN           (TC_BRIGHT | 6)
#define TC_WHITE                  (TC_BRIGHT | 7)

#define tc_bright(cdc) ((term_color_t)(cdc) | (1U << TC_BRIGHT))

/**
 * A Terminal style has the following form:
 *
 * [0:2]  Foreground base color.
 * [3]    Foreground color bright bit.
 * [4:6]  Background base color. 
 * [7]    Background color bright bit.
 * [8]    Bold
 * [9]    Italic
 * [10:15] Not yet designated.
 */
typedef uint16_t term_style_t;

#define TS_BOLD   (1U << 8)
#define TS_ITALIC (1U << 9)

#define term_style(fg, bg) ((term_color_t)(fg) | ((term_color_t)(bg) << 4))
#define ts_bg_color(ts) ((term_color_t)((term_style_t)(ts) >> 4) & 0x000FU) 
#define ts_with_bg_color(ts, bg) (((term_style_t)(ts) & 0xFF0F) | ((term_color_t)(bg) << 4))
#define ts_fg_color(ts) (term_color_t)((term_style_t)(ts) & 0x000FU)
#define ts_with_fg_color(ts, fg) (((term_style_t)(ts) & 0xFFF0) | (term_color_t)(fg))

/**
 * Flip the background color and foreground color components of a style.
 * Keeps Other styles untouched.
 *
 * Keeping this a static inline because `ts` is used multiple times in the formula.
 * As a macro, this could be dangerous.
 */
static inline term_style_t ts_flip(term_style_t ts) {
    return term_style(ts_bg_color(ts), ts_fg_color(ts)) | (ts & 0xFF00); 
}

/**
 * Add an ANSI style to a char display style.
 *
 * SEE "s_util/ansi.h".
 */
term_style_t ts_with_ansi_style_code(term_style_t ts, uint8_t ansi_style_code);

typedef struct _term_cell_t {
    term_style_t style;

    /**
     * This can be any character value!
     */
    char c;
} term_cell_t;

static inline bool tc_equals(term_cell_t tc0, term_cell_t tc1) {
    return tc0.style == tc1.style && tc0.c == tc1.c;
}

/**
 * This equals functions treats all NON-ASCII-PRINTABLE characters as ' ''s.
 */
static inline bool tc_ascii_printable_equals(term_cell_t tc0, term_cell_t tc1) {
    if (!is_ascii_printable(tc0.c)) {
        tc0.c = ' ';
    }

    if (!is_ascii_printable(tc1.c)) {
        tc1.c = ' ';
    }

    return tc0.style == tc1.style && tc0.c == tc1.c;
}

typedef struct _term_buffer_t {
    /**
     * This can be NULL to signify a static terminal buffer.
     */
    allocator_t * const al;

    /**
     * The value of a default cell. This is used when free spaces is created within the buffer.
     * For example, during a resize or scroll.
     */
    const term_cell_t default_cell;

    /**
     * These must be greater than 0.
     */
    uint16_t rows, cols;

    /**
     * Position of the cursor.
     *
     * NOTE: To allow for cursor rendering flexibility, `buf` will
     * not hold some special value at this position.
     *
     * These value are just where the next character will be printed.
     * THAT'S IT! 
     */
    uint16_t cursor_row, cursor_col;

    /**
     * The style of the next character to be printed.
     * This can be changed with ANSI control sequences.
     */
    term_style_t curr_style;

    /**
     * Buf must have size AT LEAST `rows * cols * sizeof(term_cell_t)`.
     */
    term_cell_t *buf;
} term_buffer_t;

#define TB_CELL(tb, r, c) ((tb)->buf[((r) * (tb)->cols) + (c)])

/**
 * Allocate a new terminal buffer!
 *
 * Cursor starts at (0, 0).
 *
 * Terminal buffer is cleared with default style before being returned.
 *
 * Returns NULL on error.
 */
term_buffer_t *new_term_buffer(allocator_t *al, term_cell_t default_cell, uint16_t rows, uint16_t cols);

static inline term_buffer_t *new_da_term_buffer(term_cell_t default_cell, uint16_t rows, uint16_t cols) {
    return new_term_buffer(get_default_allocator(), default_cell, rows, cols);
}

/**
 * Delete a terminal buffer.
 */
void delete_term_buffer(term_buffer_t *tb);

/**
 * Set all cells within the buffer to a single value.
 *
 * Cursor is set to (0, 0) 
 * Current style remains unchanged.
 */
void tb_clear(term_buffer_t *tb, term_cell_t cell_val);

static inline void tb_default_clear(term_buffer_t *tb) {
    tb_clear(tb, tb->default_cell);
}

/**
 * Resize the terminal buffer to the given size.
 *
 * After the resize, `tb_clear` is called with the default cell value.
 *
 * Returns FOS_E_STATE_MISMATCH if `tb` is a static buffer.
 * Returns FOS_E_NO_MEM if the resize allocation fails.
 *
 * In case of error, the buffer's state is left unchanged.
 */
fernos_error_t tb_resize(term_buffer_t *tb, uint16_t rows, uint16_t cols);

/**
 * Shift terminal contents `shift` rows up.
 *
 * Leaving new rows at the bottom of the terminal.
 * (New rows populated with default cell value)
 *
 * Cursor position remains unchanged.
 */
void tb_scroll_up(term_buffer_t *tb, uint16_t shift);

/**
 * Shift terminal contents `shift` rows down.
 *
 * Leaving new rows at the top of the terminal.
 * (New rows populated with default cell value)
 *
 * Cursor position remains unchanged.
 */
void tb_scroll_down(term_buffer_t *tb, uint16_t shift);

/**
 * Write a character to the terminal buffer.
 *
 * Control Characters Supported:
 * '\n' - Move cursor to the beginning of the next line. (Scrolling if needed)
 * '\r` - Move cursor to the beginning of the current line.
 * '\b' - Move the cursor one position backwards. 
 *
 * Undefined behavior if `c` is not a ASCII printable character or one of the listed control 
 * sequences.
 */
void tb_put_c(term_buffer_t *tb, char c);

/**
 * Write a string to the terminal buffer.
 *
 * Control Characters Supported:
 * '\n' - Move cursor to the beginning of the next line. (Scrolling if needed)
 * '\r` - Move cursor to the beginning of the current line.
 * '\b' - Move the cursor one position backwards. 
 *
 * ANSI Control Sequences Supported: See "s_util/ansi.h"
 *
 * Undefined behavior if a non-supported control character or sequence is encountered in `s`.
 */
void tb_put_s(term_buffer_t *tb, const char *s);

#define CHAR_DISPLAY_FMT_BUF_SIZE 1024
void tb_put_fmt_s(term_buffer_t *tb, const char *fmt, ...);
