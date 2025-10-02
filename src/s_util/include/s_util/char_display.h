
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * A Character Display Color uses 4 bits.
 *
 * The first 3 are the base color.
 * The 4th is the bright bit.
 */
typedef uint8_t char_display_color_t;

/*
 * Base Colors
 */

#define CDC_BLACK           (0)
#define CDC_BLUE            (1)
#define CDC_GREEN           (2)
#define CDC_CYAN            (3)
#define CDC_RED             (4)
#define CDC_MAGENTA         (5)
#define CDC_BROWN           (6)
#define CDC_LIGHT_GREY      (7)

#define CDC_BRIGHT (1U << 3)

/*
 * Bright Alternatives
 */

#define CDC_BRIGHT_BLACK           (CDC_BRIGHT | 0)
#define CDC_BRIGHT_BLUE            (CDC_BRIGHT | 1)
#define CDC_BRIGHT_GREEN           (CDC_BRIGHT | 2)
#define CDC_BRIGHT_CYAN            (CDC_BRIGHT | 3)
#define CDC_BRIGHT_RED             (CDC_BRIGHT | 4)
#define CDC_BRIGHT_MAGENTA         (CDC_BRIGHT | 5)
#define CDC_BRIGHT_BROWN           (CDC_BRIGHT | 6)
#define CDC_WHITE                  (CDC_BRIGHT | 7)

static inline char_display_color_t cdc_bright(char_display_color_t cdc) {
    return cdc | (1U << CDC_BRIGHT);
}

/**
 * A Character Display style has the following form:
 *
 * [0:2]  Foreground base color.
 * [3]    Foreground color bright bit.
 * [4:6]  Background base color. 
 * [7]    Background color bright bit.
 * [8]    Bold
 * [9]    Italic
 * [10:15] Not yet designated.
 *
 * NOTE: Character styles are OPTIONAL. If your display is entirely monochrome
 * that is totally OK!
 */
typedef uint16_t char_display_style_t;

#define CDS_BOLD   (1U << 8)
#define CDS_ITALIC (1U << 9)

static inline char_display_style_t char_display_style(char_display_color_t fg, char_display_color_t bg) {
    return fg | (bg << 4);
}

static inline char_display_color_t cds_bg_color(char_display_style_t cds) {
    return (char_display_color_t)(cds >> 4) & 0x0FU;
}

static inline char_display_style_t cds_with_bg_color(char_display_style_t cds, char_display_color_t bg) {
    return (cds & 0xFF0F) | (bg << 4);
}

static inline char_display_color_t cds_fg_color(char_display_style_t cds) {
    return (char_display_color_t)cds & 0x0FU;
}

static inline char_display_style_t cds_with_fg_color(char_display_style_t cds, char_display_color_t fg) {
    return (cds & 0xFFF0) | fg;
}

/**
 * Flip the background color and foreground color components of a style.
 * Keeps Other styles untouched.
 */
static inline char_display_style_t cds_flip(char_display_style_t cds) {
    return char_display_style(cds_bg_color(cds), cds_fg_color(cds)) | (cds & 0xFF00); 
}

/**
 * Add an ANSI style to a char display style.
 *
 * SEE "s_util/ansi.h".
 */
char_display_style_t cds_with_ansi_style_code(char_display_style_t cds, uint8_t ansi_style_code);

typedef struct _char_display_pair_t {
    char_display_style_t style;
    char c;
} char_display_pair_t;

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

