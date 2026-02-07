
#pragma once

#include "s_bridge/shared_defs.h"
#include "s_gfx/window.h"

/*
 * A terminal handle is a special type of handle which is gauranteed to support the
 * below endpoints!
 */

/**
 * Get the rows and columns of a terminal handle!
 *
 * Both `rows` and `cols` are optional.
 */
void sc_term_get_dimmensions(handle_t h, size_t *rows, size_t *cols);

/**
 * A terminal handle receives window events!
 * NOTE: That the window event structure is just used for convenience. The handle
 * might not actually communicate with a window at all!
 *
 * Sleep current thread until this terminal handle has a pending event!
 *
 * Returns FOS_E_SUCCESS when there is a pending event.
 */
fernos_error_t sc_term_wait_event(handle_t h);

/**
 * Read terminal events!
 *
 * This reads AT MOST `num_buf_cells` events into `ev_buf`.
 *
 * NON-BLOCKING!
 *
 * Design Note: You may think that `sc_term_wait_event` and `sc_term_read_events` couldn've just
 * been implementations of the `handle_read` and `handle_wait_read_ready` functions. I decided
 * against this though because `handle_read` is intended to be bytewise.
 * The user should not be able to read *part* of an event! This design ensures the usure can
 * only read full events!
 *
 * `ev_buf` is a buffer to be filled with events.
 * `ev_buf` must have size at least `num_buf_cells` * `sizeof(window_event_t)`.
 * `cells_readden` is optional. On success, the number of events written to `ev_buf` is written
 * to `*cells_readden`.
 *
 * Returns FOS_E_BAD_ARGS if `num_buf_cells > 0` and `ev_buf` is NULL.
 * Returns FOS_E_EMPTY iff there are no pending events.
 * Returns FOS_E_SUCCESS iff:
 *  1) `num_buf_cells == 0` AND there are pending events.
 *  OR
 *  2) `num_buf_cells > 0` AND at least 1 event was read into `ev_buf`.
 *
 * Can return FOS_E_UNKNOWN_ERROR in other error cases.
 */
fernos_error_t sc_term_read_events(handle_t h, window_event_t *ev_buf, size_t num_buf_cells, size_t *cells_readden);
