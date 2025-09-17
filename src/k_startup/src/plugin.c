
#include "k_startup/plugin.h"

fernos_error_t plgs_tick(plugin_t **plgs, size_t plgs_len) {
    fernos_error_t err;
    for (size_t i = 0; i < plgs_len; i++) {
        plugin_t *plg = plgs[i];

        if (plg) {
            err = plg_tick(plg);

            if (err != FOS_E_SUCCESS) {
                return err;
            }
        }
    }

    return FOS_E_SUCCESS;
}

fernos_error_t plgs_on_fork_proc(plugin_t **plgs, size_t plgs_len, proc_id_t cpid) {
    fernos_error_t err;
    for (size_t i = 0; i < plgs_len; i++) {
        plugin_t *plg = plgs[i];

        if (plg) {
            err = plg_on_fork_proc(plg, cpid);

            if (err != FOS_E_SUCCESS) {
                return err;
            }
        }
    }

    return FOS_E_SUCCESS;
}

fernos_error_t plgs_on_reap_proc(plugin_t **plgs, size_t plgs_len, proc_id_t rpid) {
    fernos_error_t err;
    for (size_t i = 0; i < plgs_len; i++) {
        plugin_t *plg = plgs[i];

        if (plg) {
            err = plg_on_reap_proc(plg, rpid);

            if (err != FOS_E_SUCCESS) {
                return err;
            }
        }
    }

    return FOS_E_SUCCESS;
}
