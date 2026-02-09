
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
