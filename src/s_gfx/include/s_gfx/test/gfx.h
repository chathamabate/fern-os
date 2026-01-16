
#pragma once

#include "s_gfx/gfx.h"

/**
 * This is a test suite similar to the other test suites in this project.
 * It tests out the more easily verifiable logic of `gfx.c`.
 */
bool test_gfx(void);

/*
 * Here are a set of animations which test out graphics functionalities!
 * We except each of these functions be called in a timed loop.
 *
 * Each test DOES NOT clear the given buffer before rendering!
 */

/**
 * This is a static drawing, not an animation.
 * It draws a grid of rectangles with 1 pixel borders.
 */
void gfx_test_rect_grid(gfx_buffer_t *buffer, const gfx_box_t *clip);

/**
 * In this test, a rectangle bounces around the buffer's edges.
 * Starting from the top left corner.
 *
 * Stateful function.
 */
void gfx_test_bouncing_rect(gfx_buffer_t *buffer, const gfx_box_t *clip);

/**
 * Just like `gfx_test_bouncing_rect`, this function test bouncing a round a box who's bounds
 * are larger than the buffer itself! (Should should automatic clipping off the buffer's edges.)
 *
 * Stateful function.
 */
void gfx_test_outside_bouncing_rect(gfx_buffer_t *buffer, const gfx_box_t *clip);

/**
 * Just a rectangle at the middle of the screen which slowly gets larger than smaller.
 *
 * Stateful function.
 */
void gfx_test_growing_rect(gfx_buffer_t *buffer, const gfx_box_t *clip);

/**
 * This will render a seriese of bitmaps scaled and unscaled in rows on the screen.
 * This is stateless and not an animation.
 */
void gfx_test_bitmaps(gfx_buffer_t *buffer, const gfx_box_t *clip);

/**
 * This is just like `gfx_test_outside_bouncing_rect`, but with a bouncing bitmap instead!
 *
 * Stateful function.
 */
void gfx_test_outside_bouncing_bitmap(gfx_buffer_t *buffer, const gfx_box_t *clip);

/**
 * This just generates a buffer with some gradient, then pastes it into the given buffer
 * at (x, y)
 */
void gfx_test_gradient_buffer_paste(gfx_buffer_t *buffer, const gfx_box_t *clip, int32_t x, int32_t y);

