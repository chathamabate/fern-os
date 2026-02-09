
#pragma once

#include "s_gfx/window.h"
#include "s_data/term_buffer.h"
#include "s_gfx/mono_fonts.h"

#define WINDOW_DUMMY_FONT (ASCII_MONO_8X16)
#define WINDOW_DUMMY_PALETTE (BASIC_ANSI_PALETTE)

/**
 * A Dummy Window is just an example of a self managed window.
 * It's purpose is for testing.
 *
 * All it does is log events to a terminal buffer! (Which is rendered!)
 */
typedef struct _window_dummy_t {
    window_t super;

    allocator_t * const al;

    /**
     * This terminal buffer reflects the current contents of this window's buffer.
     */
    term_buffer_t * const visible_tb;

    /**
     * When this is true, the contents of this window's buffer are unknown.
     */
    bool dirty_buffer;

    /**
     * The "true" value of the this window's terminal buffer which should be rendered next frame!
     */
    term_buffer_t * const real_tb;
} window_dummy_t;

/**
 * Create a new dummy window!
 */
window_t *new_window_dummy(allocator_t *al);

static inline window_t *new_da_window_dummy(void) {
    return new_window_dummy(get_default_allocator());
}
