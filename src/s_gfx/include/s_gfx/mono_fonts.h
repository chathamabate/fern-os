
#pragma once

#include <stdint.h>
#include "s_gfx/gfx.h"
#include "s_data/term_buffer.h"

/**
 * An ascii monofont describes how to render the first 128
 * ascii characters. All characters are given the same rendering
 * height and width.
 */
typedef struct _ascii_mono_font_t {
    /**
     * Width of all characters in pixels. (Must be non-zero)
     */
    uint8_t char_width;

    /**
     * Height of all characters in pixels. (Must be non-zero)
     */
    uint8_t char_height;

    /**
     * The dimmensions of this array are slightly confusing.
     * 
     * Each character is given one bitmap.
     * A bitmap follows the definition found in `s_gfx/gfx.h`.
     * For example, an 6 X 8 character (6 columns and 8 rows)
     * will have a bit map where each row is 1 byte and there are 8 rows.
     * 
     * {
     *      0x00,
     *      0x00, 
     *      0x00,
     *      0x00,
     *      0x00,
     *      0x00,
     *      0x00,
     *      0x00
     * }
     *
     * The bitmaps array will store 128 of these bitmaps consecutively!
     */
    const uint8_t *bitmaps;
} ascii_mono_font_t;

extern const ascii_mono_font_t * const ASCII_MONO_8X8;
extern const ascii_mono_font_t * const ASCII_MONO_8X16;

/**
 * Draw a string to a buffer using an ascii monospace font.
 * If `str` contains a character with value >= 128, said character will be 
 * drawn as the NULL character.
 *
 * `(x, y)` will be the top left corner of the first letter in `str`.
 *
 * `clip_area` can be NULL.
 */
void gfx_draw_ascii_mono_text(gfx_buffer_t *buf, const gfx_box_t *clip_area,
        const char *str, 
        const ascii_mono_font_t *amf, 
        int32_t x, int32_t y, 
        uint8_t w_scale, uint8_t h_scale,
        gfx_color_t fg_color, gfx_color_t bg_color);

/**
 * A palette is just 16 colors which are meant be used for the following colors in order:
 *
 * 0 BLACK           
 * 1 BLUE            
 * 2 GREEN           
 * 3 CYAN            
 * 4 RED             
 * 5 MAGENTA         
 * 6 BROWN           
 * 7 LIGHT_GREY      
 * 8 BRIGHT_BLACK           
 * 9 BRIGHT_BLUE            
 * 10 BRIGHT_GREEN           
 * 11 BRIGHT_CYAN            
 * 12 BRIGHT_RED             
 * 13 BRIGHT_MAGENTA         
 * 14 BRIGHT_BROWN           
 * 15 WHITE                  
 */
typedef struct _gfx_ansi_palette_t {
    gfx_color_t colors[16];
} gfx_ansi_palette_t;

/**
 * Render a terminal buffer's contents to the given buffer.
 *
 * `curr_tb` is optional.
 *
 * If `curr_tb` is given, only cells in `next_tb` which differ from those in 
 * `curr_tb` are rendered. (If the dimmensions of `curr_tb` don't match the
 * dimmensions of `next_tb` this function does nothing)
 *
 * If `curr_tb` is NULL, all of `next_tb` is rendered.
 */
void gfx_draw_term_buffer(gfx_buffer_t *buf, const gfx_box_t *clip_area,
        const term_buffer_t *curr_tb, const term_buffer_t *next_tb, 
        const ascii_mono_font_t *amf, const gfx_ansi_palette_t *palette,
        int32_t x, int32_t y,
        uint8_t w_scale, uint8_t h_scale);
