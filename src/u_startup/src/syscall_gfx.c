
#include "u_startup/syscall.h"
#include "u_startup/syscall_gfx.h"

fernos_error_t sc_gfx_new_dummy(void) {
    return sc_plg_cmd(PLG_GRAPHICS_ID, PLG_GFX_PCID_NEW_DUMMY,
            0, 0, 0, 0);
}
