
#pragma once

#include "s_gfx/gfx.h"

/*
 * Here are a set of animations which test out graphics functionalities!
 * We except each of these functions be called in a timed loop.
 *
 * Each test DOES NOT clear the given buffer before rendering!
 */

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
 */
void gfx_test_outside_bouncing_rect(gfx_buffer_t *buffer, const gfx_box_t *clip);
