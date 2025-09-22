
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
};

/**
 * Delete the character display.
 */
void delete_char_display(char_display_t *cd);

/**
 *
 */
void cd_append_style();

/**
 * Display a character at row `r` and col `c`.
 *
 * 0 <= `r` < `rows`
 * 0 <= `c` < `cols`
 * 32 <= `ch` < 127 (i.e. `ch` is a printable ascii character)
 */
void cd_put_char(char_display_t *cd, size_t r, size_t c, char ch);

/**
 * This should  
 */
void cd_scroll_down(char_display_t *cd);



