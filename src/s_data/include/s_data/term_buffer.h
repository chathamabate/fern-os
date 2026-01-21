
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "s_mem/allocator.h"

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

static inline term_color_t tc_bright(term_color_t cdc) {
    return cdc | (1U << TC_BRIGHT);
}

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

static inline term_style_t term_style(term_color_t fg, term_color_t bg) {
    return fg | (bg << 4);
}

static inline term_color_t ts_bg_color(term_style_t ts) {
    return (term_color_t)(ts >> 4) & 0x0FU;
}

static inline term_style_t ts_with_bg_color(term_style_t ts, term_color_t bg) {
    return (ts & 0xFF0F) | (bg << 4);
}

static inline term_color_t ts_fg_color(term_style_t ts) {
    return (term_color_t)ts & 0x0FU;
}

static inline term_style_t ts_with_fg_color(term_style_t ts, term_color_t fg) {
    return (ts & 0xFFF0) | fg;
}

/**
 * Flip the background color and foreground color components of a style.
 * Keeps Other styles untouched.
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
    char c;
} term_cell_t;

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
     * Buf must have size AT LEAST `rows * cols * sizeof(term_cell_t)`.
     */
    term_cell_t *buf;
} term_buffer_t;

/**
 * Allocate a new terminal buffer!
 *
 * (The state of the cells in the created buffer is undefined)
 * cursor starts at (0, 0)
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
 *
 * Cursor position remains unchanged.
 */
void tb_scroll_up(term_buffer_t *tb, uint16_t shift);

/**
 * Shift terminal contents `shift` rows down.
 *
 * Leaving new rows at the top of the terminal.
 *
 * Cursor position remains unchanged.
 */
void tb_scroll_down(term_buffer_t *tb, uint16_t shift);




typedef struct _char_display_t char_display_t;
typedef struct _char_display_impl_t char_display_impl_t;

struct _char_display_impl_t {
    void (*delete_char_display)(char_display_t *cd);

    /*
     * The below functions will be "private virtual". The user will only interact with
     * a character display using the put_s and put_c functions.
     *
     * THEY SHOULD NEVER modify the state within the char_display base class.
     * (They should have no knowledge of a cursor)
     */

    /**
     * Get the value of a single cell in the grid.
     *
     * If `style` is non-NULL, the style of the given cell is written to `*style`.
     * If `c` is non-NULL, the character displayed in the given cell is written to `*c`.
     */
    void (*cd_get_c)(char_display_t *cd, size_t row, size_t col, char_display_style_t *style, char *c);

    /**
     * Place a character on the screen at position (`row`, `col`) with style `style`.
     * 
     * This should only do anything if:
     * 0 <= `row` < `rows`
     * 0 <= `col` < `cols`
     */
    void (*cd_set_c)(char_display_t *cd, size_t row, size_t col, char_display_style_t style, char c);

    /**
     * Shift all rows up leaving new rows at the bottom of the screen.
     *
     * The new rows should be filled with ' ''s using the default style.
     *
     * If `shift` >= `rows`, this clears the screen.
     */
    void (*cd_scroll_up)(char_display_t *cd, size_t shift);

    /**
     * Shift all rows down leaving new rows at the top of the screen.
     *
     * The new rows should be filled with ' ''s using the default style.
     *
     * If `shift` >= `rows`, this clears the screen.
     */
    void (*cd_scroll_down)(char_display_t *cd, size_t shift);

    /**
     * Set an entire row to a single character.
     *
     * This should only do anything if:
     * 0 <= `row` < `rows`
     */
    void (*cd_set_row)(char_display_t *cd, size_t row, char_display_style_t style, char c);

    /**
     * Set the entire grid to a single character with a single style.
     */
    void (*cd_set_grid)(char_display_t *cd, char_display_style_t style, char c);
};

/**
 * A Char Display is some sort of abstract grid of characters.
 *
 * The grid has a constant number of rows and columns.
 *
 * A character display has no notion of history past the current values in the grid.
 *
 * The char display should be able to handle all ASCII printable characters 
 * (i.e. in the range [32, 127)) 
 */
struct _char_display_t {
    const char_display_impl_t * const impl;

    const size_t rows;
    const size_t cols;

    /**
     * 0-indexed row of the cursor.
     */
    size_t cursor_row;

    /**
     * 0-indexed column of the cursor.
     */
    size_t cursor_col;

    /**
     * When an ANSI Reset code is processed, this is the style returned to.
     */
    const char_display_style_t default_style;

    /**
     * The current style used for printing text.
     */
    char_display_style_t curr_style;
};

/**
 * Initialize the char_display_t base class fields.
 *
 * This is a helper function to be used in the constructors of derrived classes.
 *
 * The starting cursor position will be the top left corner (0, 0)
 * NOTE: THIS DOES NOT call any of the given virtual functions AT ALL.
 *
 * Make sure your derrived constructor clears the display and renders the cursor!
 */
void init_char_display_base(char_display_t *base, const char_display_impl_t *impl, size_t rows, size_t cols,
        char_display_style_t default_style);

/**
 * Delete the character display.
 */
static inline void delete_char_display(char_display_t *cd) {
    if (cd) {
        cd->impl->delete_char_display(cd);
    }
}

/**
 * Write a character to the terminal.
 *
 * Control Characters Supported:
 * '\n' - Move cursor to the beginning of the next line. (Scrolling if needed)
 * '\r` - Move cursor to the beginning of the current line.
 * '\b' - Move the cursor one position backwards. 
 *
 * Undefined behavior if `c` is not a ASCII printable character or one of the listed control 
 * sequences.
 */
void cd_put_c(char_display_t *cd, char c);

/**
 * Write a string to the terminal.
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
void cd_put_s(char_display_t *cd, const char *s);

#define CHAR_DISPLAY_FMT_BUF_SIZE 1024
void cd_put_fmt_s(char_display_t *cd, const char *fmt, ...);

