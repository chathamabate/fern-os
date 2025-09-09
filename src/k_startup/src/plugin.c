
#include "k_startup/plugin.h"

fernos_error_t plgs_tick(plugin_t **plgs, size_t plgs_len) {
    for (size_t i = 0; i < plgs_len; i++) {
        plugin_t *plg = plgs[i];
        if (plg && plg_tick(plg) == FOS_ABORT_SYSTEM) {
            return FOS_ABORT_SYSTEM;
        }
    }

    return FOS_SUCCESS;
}

fernos_error_t plgs_on_fork_proc(plugin_t **plgs, size_t plgs_len, proc_id_t cpid) {
    for (size_t i = 0; i < plgs_len; i++) {
        plugin_t *plg = plgs[i];
        if (plg && plg_on_fork_proc(plg, cpid) == FOS_ABORT_SYSTEM) {
            return FOS_ABORT_SYSTEM;
        }
    }

    return FOS_SUCCESS;
}

fernos_error_t plgs_on_reap_proc(plugin_t **plgs, size_t plgs_len, proc_id_t rpid) {
    for (size_t i = 0; i < plgs_len; i++) {
        plugin_t *plg = plgs[i];
        if (plg && plg_on_reap_proc(plg, rpid) == FOS_ABORT_SYSTEM) {
            return FOS_ABORT_SYSTEM;
        }
    }

    return FOS_SUCCESS;
}
