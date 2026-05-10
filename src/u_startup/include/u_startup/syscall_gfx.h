#pragma once

#include "s_bridge/shared_defs.h"
#include "s_gfx/mono_fonts.h"
#include "s_gfx/window.h"

/**
 * Create a self managed dummy window.
 *
 * (In here for debug purposes)
 */
fernos_error_t sc_gfx_new_dummy(void);

/**
 * Create a new Terminal window and handle!
 * If `attrs` is NULL, a defualt will be used.
 *
 * FOS_E_SUCCESS means the window was successfully created and it's handle was 
 * written to `*h`.
 * FOS_E_BAD_ARGS if `h` is NULL.
 * (Other errors may be returned)
 *
 * VERY IMPORTANT: the handle returned will conform to the terminal handle interface.
 * Use the handle endpoints define in `syscall_term.h`!
 * Not the ones below!
 */
fernos_error_t sc_gfx_new_terminal(handle_t *h, const gfx_term_buffer_attrs_t *attrs);

/**
 * Create a new graphics window!
 *
 * The windows will have two buffers. Their addresses will be written to `*shm_buf`.
 * The buffer which starts as visible will be written to `(*shm_buf)[0]`.
 */
fernos_error_t sc_gfx_new_gfx_window(handle_t *h, gfx_color_t *(*shm_buf)[2]);

/**
 * Get the width and height of a graphics window!
 */
void sc_gfx_get_dimmensions(handle_t h, size_t *width, size_t *height);

/**
 * Block until the graphics window has pending events!
 */
fernos_error_t sc_gfx_wait_event(handle_t h);

/**
 * This basically follows the same interface as `sc_term_read_events`.
 *
 * Although, RESIZE events will be in units of pixels! 
 */
fernos_error_t sc_gfx_read_events(handle_t h, window_event_t *ev_buf, size_t num_buf_cells, size_t *cells_readden);

/**
 * Swap the back and front buffers of `h`'s graphics window!
 */
void sc_gfx_swap(handle_t h);
