#pragma once


#include "s_bridge/shared_defs.h"

#include "s_gfx/mono_fonts.h"

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
 */
fernos_error_t sc_gfx_new_terminal(handle_t *h, const gfx_term_buffer_attrs_t *attrs);
