
#include "u_startup/syscall_kb.h"
#include "s_bridge/shared_defs.h"

fernos_error_t sc_kb_open(handle_t *h) {
    return sc_plg_cmd(PLG_KEYBOARD_ID, PLG_KB_PCID_OPEN, (uint32_t)h, 0, 0, 0);
}

void sc_kb_skip_forward(handle_t h) {
    (void)sc_handle_cmd(h, PLG_KB_HCID_SKIP_FWD, 0, 0, 0, 0);
}
