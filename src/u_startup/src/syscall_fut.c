
#include "u_startup/syscall.h"
#include "u_startup/syscall_fut.h"

fernos_error_t sc_fut_register(futex_t *futex) {
    return sc_plg_cmd(PLG_FUTEX_ID, PLG_FUT_PCID_REGISTER, (uint32_t)futex, 0, 0, 0);
}

void sc_fut_deregister(futex_t *futex) {
    (void)sc_plg_cmd(PLG_FUTEX_ID, PLG_FUT_PCID_DEREGISTER, (uint32_t)futex, 0, 0, 0);
}

fernos_error_t sc_fut_wait(futex_t *futex, futex_t exp_val) {
    return sc_plg_cmd(PLG_FUTEX_ID, PLG_FUT_PCID_WAIT, (uint32_t)futex, exp_val, 0, 0);
}

fernos_error_t sc_fut_wake(futex_t *futex, bool all) {
    return sc_plg_cmd(PLG_FUTEX_ID, PLG_FUT_PCID_WAKE, (uint32_t)futex, (uint32_t)all, 0, 0);
}

