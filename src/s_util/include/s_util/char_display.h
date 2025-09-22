
#pragma once

#include <stdlib.h>

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

/*
 * Bright Alternatives
 */

#define CDC_BRIGHT_BLACK           (8 + 0)
#define CDC_BRIGHT_BLUE            (8 + 1)
#define CDC_BRIGHT_GREEN           (8 + 2)
#define CDC_BRIGHT_CYAN            (8 + 3)
#define CDC_BRIGHT_RED             (8 + 4)
#define CDC_BRIGHT_MAGENTA         (8 + 5)
#define CDC_BRIGHT_BROWN           (8 + 6)
#define CDC_WHITE                  (8 + 7)

static inline char_display_color_t cdc_bright(char_display_color_t cdc) {
    return cdc | (1U << 3);
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

static inline char_display_color_t cds_fg_color(char_display_style_t cds) {
    return (char_display_color_t)cds & 0x0FU;
}

typedef struct _char_display_t char_display_t;
typedef struct _char_display_impl_t char_display_impl_t;

struct _char_display_impl_t {

    void (*delete_char_display)(char_display_t *cd);

    /*
     * These functions actually won't even be exposed to the user, only used internally
     * by the char display base implementation.
     *
     * (I believe this is similar to private virtual functions in cpp)
     */

    /**
     * Place a character on the screen at position (`row`, `col`) with style `style`.
     *
     * It is gauranteed:
     * 0 <= `row` < `rows`
     * 0 <= `col` < `cols`
     * 32 <= `c` < 127 (`c` is an ascii printable character)
     */
    void (*cd_put_c)(char_display_t *cd, size_t row, size_t col, char_display_style_t style, char c);

    /**
     * Get a character at a given position on the display.
     *
     * 0 <= `row` < `rows`
     * 0 <= `col` < `cols`
     *
     * If `style` is given, the character's style should be written to `*style`.
     *
     * NOTE: Your display need not remember styles it does not support. For example, if a white
     * italic character is placed at (0, 0), but your display doesn't support italics, just "white
     * FG" can be written to `*style`.
     */
    char (*cd_get_c)(char_display_t *cd, size_t row, size_t col, char_display_style_t *style);

    /**
     * This should efficiently move each row of characters up `shift` rows on the display.
     * Each new row should be filled with `cols` `fill` characters, each with style `style`.
     *
     * If `shift` >= `rows` this will clear the screen.
     */
    void (*cd_scroll_down)(char_display_t *cd, size_t shift, char fill, char_display_style_t style);
};

/**
 * A Char Display is some sort of abstract grid of characters.
 *
 * The grid has a constant number of rows and columns.
 */
struct _char_display_t {
    const char_display_impl_t * const impl;

    const size_t rows;
    const size_t cols;

    /*
     * The below fields SHOULD NOT be accessed or modified by the virtual function implementations.
     */

    /**
     * When the style is "RESET", this is the style returned to.
     */
    char_display_style_t default_style;

    /**
     * The current style text is displayed as.
     */
    char_display_style_t curr_style;

    /**
     * The current position of the cursor.
     */
    size_t cursor_row;
    size_t cursor_col;
};

/**
 * Initialize the char_display_t base class fields.
 *
 * This is a helper function to be used in the constructors of derrived classes.
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
 * Place character c at the cursor position, and advance the cursor.
 *
 * Control Characters Supported:
 *
 * '\n' - Move cursor to start of next line.
 * '\r' - Move cursor to beginning of current line.
 * '\b` - Move cursor back one space.
 *
 * Otherwise, if `c` is not in the range [32, 127), nothing will happen.
 */
void cd_put_c(char_display_t *cd, char c);

/**
 * Place the contents of a string `s`.
 *
 * Control Sequences Supported: See `s_util/ansi.h`.
 *
 * Control Characters Supported:
 *
 * '\n' - Move cursor to start of next line.
 * '\r' - Move cursor to beginning of current line.
 * '\b` - Move cursor back one space.
 *
 * Otherwise, if `c` is not in the range [32, 127), `c` is ignored.
 */
void cd_put_s(char_display_t *cd, const char *s);
