
#pragma once

#include "s_gfx/gfx.h"
#include "s_gfx/mono_fonts.h"


/*
 * similar to `test/gfx.c` These functions are stateful and expected to be called multiple repeatedly
 * to form an animation!
 */

/**
 * This function displays a row of text which will scroll horizontally AND bounce vertically.
 *
 * Stateful Function.
 */
void gfx_test_moving_mono_text(gfx_buffer_t *buffer, const gfx_box_t *clip_area, 
        const ascii_mono_font_t *font);
