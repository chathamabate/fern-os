
#include "u_startup/syscall.h"
#include "u_startup/syscall_term.h"

void sc_term_get_dimmensions(handle_t h, size_t *rows, size_t *cols) {
    (void)sc_handle_cmd(h, TERM_HCID_GET_DIMS, (uint32_t)rows, (uint32_t)cols, 0, 0);
}

fernos_error_t sc_term_wait_event(handle_t h) {
    return sc_handle_cmd(h, TERM_HCID_WAIT_EVENT, 0, 0, 0, 0);
}

fernos_error_t sc_term_read_events(handle_t h, window_event_t *ev_buf, size_t num_buf_cells, size_t *cells_readden) {
    return sc_handle_cmd(h, TERM_HCID_READ_EVENTS, (uint32_t)ev_buf, (uint32_t)num_buf_cells, (uint32_t)cells_readden, 0);
}
