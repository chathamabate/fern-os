
#include "u_startup/syscall.h"
#include "u_startup/syscall_gfx.h"

fernos_error_t sc_gfx_new_dummy(void) {
    return sc_plg_cmd(PLG_GRAPHICS_ID, PLG_GFX_PCID_NEW_DUMMY,
            0, 0, 0, 0);
}

fernos_error_t sc_gfx_new_terminal(handle_t *h, const gfx_term_buffer_attrs_t *attrs) {
    return sc_plg_cmd(PLG_GRAPHICS_ID, PLG_GFX_PCID_NEW_TERM,
            (uint32_t)h, (uint32_t)attrs, 0, 0);
}

fernos_error_t sc_gfx_new_gfx_window(handle_t *h, gfx_color_t *(*shm_buf)[2]) {
    return sc_plg_cmd(PLG_GRAPHICS_ID, PLG_GFX_PCID_NEW_GFX_WIN, 
            (uint32_t)h, (uint32_t)shm_buf, 0, 0);
}

void sc_gfx_get_dimmensions(handle_t h, size_t *width, size_t *height) {
    (void)sc_handle_cmd(h, PLG_GFX_HCID_GET_DIMS, (uint32_t)width, (uint32_t)height, 0, 0);
}

fernos_error_t sc_gfx_wait_event(handle_t h) {
    return sc_handle_cmd(h, PLG_GFX_HCID_WAIT_EVENT, 0, 0, 0, 0);
}

fernos_error_t sc_gfx_read_events(handle_t h, window_event_t *ev_buf, size_t num_buf_cells, size_t *cells_readden) {
    return sc_handle_cmd(h, PLG_GFX_HCID_READ_EVENTS, (uint32_t)ev_buf, (uint32_t)num_buf_cells, (uint32_t)cells_readden, 0);
}

void sc_gfx_swap(handle_t h) {
    (void)sc_handle_cmd(h, PLG_GFX_HCID_SWAP, 0, 0, 0, 0);
}

