
#pragma once

#include <stdlib.h>
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

#define CDC_BRIGHT (1U << 4)

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

/**
 * Flip the background color and foreground color components of a style.
 * Keeps Other styles untouched.
 */
static inline char_display_style_t cds_flip(char_display_style_t cds) {
    return char_display_style(cds_bg_color(cds), cds_fg_color(cds)) | (cds & 0xFF00); 
}

typedef struct _char_display_pair_t {
    char_display_style_t style;
    char c;
} char_display_pair_t;

typedef struct _char_display_t char_display_t;
typedef struct _char_display_impl_t char_display_impl_t;

struct _char_display_impl_t {
    void (*delete_char_display)(char_display_t *cd);

    void (*cd_set_c)(char_display_t *cd, size_t row, size_t col, char_display_style_t style, char c);
    char (*cd_get_c)(char_display_t *cd, size_t row, size_t col, char_display_style_t *style);

    void (*cd_put_row)(char_display_t *cd, size_t row, const char_display_pair_t *row_data);
    void (*cd_set_row)(char_display_t *cd, size_t row, char_display_style_t style, char c);

    void (*cd_get_row)(char_display_t *cd, size_t row, char_display_pair_t *row_data);

    void (*cd_put_grid)(char_display_t *cd, const char_display_pair_t *grid_data);
    void (*cd_set_grid)(char_display_t *cd, char_display_style_t style, char c);

    void (*cd_get_grid)(char_display_t *cd, char_display_pair_t *grid_data);
};

/**
 * A Char Display is some sort of abstract grid of characters.
 *
 * The grid has a constant number of rows and columns.
 *
 * A character display has no notion of history. 
 *
 * The char display should be able to handle all ASCII printable characters 
 * (i.e. in the range [32, 127))
 *
 * Undefined behavior if any other characters are passed into the functions below.
 */
struct _char_display_t {
    const char_display_impl_t * const impl;

    const size_t rows;
    const size_t cols;
};

/**
 * Initialize the char_display_t base class fields.
 *
 * This is a helper function to be used in the constructors of derrived classes.
 */
void init_char_display_base(char_display_t *base, const char_display_impl_t *impl, size_t rows, size_t cols);

/**
 * Delete the character display.
 */
static inline void delete_char_display(char_display_t *cd) {
    if (cd) {
        cd->impl->delete_char_display(cd);
    }
}

/**
 * Place a character on the screen at position (`row`, `col`) with style `style`.
 * 
 * This should only do anything if:
 * 0 <= `row` < `rows`
 * 0 <= `col` < `cols`
 */
static inline void cd_set_c(char_display_t *cd, size_t row, size_t col, char_display_style_t style, char c) {
    cd->impl->cd_set_c(cd, row, col, style, c); 
}

/**
 * Get a character at a given position on the display.
 *
 * This should only do anything if:
 * 0 <= `row` < `rows`
 * 0 <= `col` < `cols`
 *
 * If `style` is given, the character's style should be written to `*style`.
 *
 * NOTE: Your display need not remember styles it does not support. For example, if a white
 * italic character is placed at (0, 0), but your display doesn't support italics, just "white
 * FG" can be written to `*style`.
 */
static inline char cd_get_c(char_display_t *cd, size_t row, size_t col, char_display_style_t *style) {
    return cd->impl->cd_get_c(cd, row, col, style);
}

/**
 * Write a full row to the display efficiently.
 *
 * This should only do anything if:
 * 0 <= `row` < `rows`
 * `row_data` is non-NULL.
 *
 * `row_data` should be an array of pairs with size at least `cols`.
 */
static inline void cd_put_row(char_display_t *cd, size_t row, const char_display_pair_t *row_data) {
    cd->impl->cd_put_row(cd, row, row_data);
}

/**
 * Set an entire row to a single character.
 *
 * This should only do anything if:
 * 0 <= `row` < `rows`
 */
static inline void cd_set_row(char_display_t *cd, size_t row, char_display_style_t style, char c) {
    cd->impl->cd_set_row(cd, row, style, c);
}

/**
 * Read a full row from the display efficiently.
 *
 * This should only do anything if:
 * 0 <= `row` < `rows`
 * `row_data` is non-NULL.
 *
 * `row_data` should be an array of pairs with size at least `cols`.
 */
static inline void cd_get_row(char_display_t *cd, size_t row, char_display_pair_t *row_data) {
    cd->impl->cd_get_row(cd, row, row_data);
}

/**
 * Write over the entire display efficiently.
 *
 * Only does anything if `grid_data` is non-NULL.
 */
static inline void cd_put_grid(char_display_t *cd, const char_display_pair_t *grid_data) { 
    cd->impl->cd_put_grid(cd, grid_data);
}

/**
 * Set the entire grid to a single character with a single style.
 */
static inline void cd_set_grid(char_display_t *cd, char_display_style_t style, char c) {
    cd->impl->cd_set_grid(cd, style, c);
}

/**
 * Read out the entire display efficiently.
 *
 * Only does anything if `grid_data` is non-NULL.
 *
 * `grid_data` should be an array of pairs with size at least `rows * cols`.
 */
static inline void cd_get_grid(char_display_t *cd, char_display_pair_t *grid_data) {
    cd->impl->cd_get_grid(cd, grid_data);
}


