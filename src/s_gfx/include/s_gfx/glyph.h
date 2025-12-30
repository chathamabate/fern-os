
#pragma once

#include <stdint.h>
#include <stddef.h>

/**
 * A "glyph" is a monochrome symbol which can easily be 
 * scaled and rendered.
 *
 * NOTE: A `glyph_t` is meant to be always part 
 * of a glyph set. 
 *
 * For example, `bit_map` are the bits which make up the glyph,
 * `bit_map`'s dimmensions equal `glyph_height` * `max_glyph_width` / 8.
 * (Where `max_glyph_wdith` is rounded up to the nearest multiple of 8)
 * `glyph_height` and `max_glyph_width` come form the parent `glyph_set_t`.
 */
typedef struct _glyph_t {
    /**
     * The width of this base glyph in pixels.
     */
    uint8_t glyph_width;

    const uint8_t *bit_map;
} glyph_t;

/**
 * A glyph set is a table of "glyph"'s.
 *
 * All glyph's in a glyph set must have the same height,
 * but can have differnet widths.
 */
typedef struct _glyph_set_t {
    /**
     * The height of each glyph in this set in PIXELS!
     */
    uint8_t glyph_height;

    /**
     * The maximum width of a glyph in this set in PIXELS!
     */
    uint8_t max_glyph_width;

    /**
     * Number of glyphs in this set.
     */
    size_t num_glyphs;

    /**
     * If a glyph in the glyph table has a width of 0 or a NULL
     * bit map pointer, it is regarded as "unknown".
     *
     * In which case, this glyph should be used instead!
     */
    glyph_t unknown_glyph;

    /**
     * An array of glyphs with length `num_glyphs`.
     */
    const glyph_t *table;
} glyph_set_t;

/**
 * A static glyph set of the first 128 ascii characters in a monospace style.
 */
extern const glyph_set_t * const ASCII_GLYPHS_MONOSPACE;


