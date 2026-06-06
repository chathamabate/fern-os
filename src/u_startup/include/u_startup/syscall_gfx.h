#pragma once

#include "s_bridge/shared_defs.h"
#include "s_gfx/mono_fonts.h"
#include "s_gfx/window.h"
#include "s_gfx/gfx_manager.h"

#include "os_defs.h"

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
 * Use the handle endpoints defined in `syscall_term.h`!
 * Not the ones below!
 */
fernos_error_t sc_gfx_new_terminal(handle_t *h, const gfx_term_buffer_attrs_t *attrs);

/**
 * Create a new graphics window!
 *
 * FOS_E_BAD_ARGS if either `h` or `shm_buf` are NULL.
 *
 * On FOS_E_SUCCESS, the window handle will be written to `*h`. The starting addresses of both
 * buffers will be written to `*shm_buf`.
 *
 * A few notes:
 * 1. (*shm_buf)[0] will be the starting visible buffer!
 * 2. As hinted by its name `shm_buf` is populated with two shared memory areas which are mapped
 * in the calling process. They behave like any other shared memory areas!
 * 3. For window resources to be deleted, all referencing handles must be closed AND the window
 * must be closed from the desktop. On window cleanup, the shared memory areas just have their
 * kernel reference count decremented! They will still persist in userspace until being 
 * manually unmapped!
 * 4. The created bufs will each be fixed in size. (FERNOS_GFX_WIDTH * FERNOS_GFX_HEIGHT * sizeof(gfx_color_t))
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
 * A blocking wrapper around `sc_gfx_read_events`.
 *
 * FOS_E_SUCCESS means a single event was read into `*ev`.
 * Other errors possible.
 */
fernos_error_t sc_gfx_read_event_single(handle_t h, window_event_t *ev);

/**
 * Swap the back and front buffers of `h`'s graphics window!
 */
void sc_gfx_swap(handle_t h);
