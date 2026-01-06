
#pragma once

#include "s_gfx/gfx.h"

/*
 * Here are a set of animations which test out graphics functionalities!
 * We except each of these functions be called in a timed loop, given 
 * a frame number each time.
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
