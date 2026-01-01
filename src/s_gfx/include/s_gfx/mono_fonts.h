
#pragma once

#include <stdint.h>
#include "s_gfx/gfx.h"

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
 */
void gfx_draw_ascii_mono_text(gfx_buffer_t *buf, const char *str, 
        const ascii_mono_font_t *amf, 
        int32_t x, int32_t y, 
        uint8_t w_scale, uint8_t h_scale,
        gfx_color_t fg_color, gfx_color_t bg_color);
