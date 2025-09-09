
#pragma once

#include "s_util/err.h"
#include "k_startup/state.h"
#include "s_bridge/shared_defs.h"

/*
 *  A plugin will be a way of extending the kernel without needing to manually
 *  change any of the core kernel structures (state, process, or thread).
 *
 *  As of today (9/8/2025), a plugin's interface will be somewhat simple. Essentially a plugin is
 *  just a listener for certain system events. Right now, only a few events are listened for,
 *  later if I feel the need, I will expand this interface. 
 */


typedef struct _plugin_impl_t {
    // REQUIRED
    void (*delete_plugin)(plugin_t *plg);

    /*
     * Below calls are ALL OPTIONAL!
     */

    fernos_error_t (*plg_tick)(plugin_t *plg);
    fernos_error_t (*plg_cmd)(plugin_t *plg, plugin_cmd_id_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);
    fernos_error_t (*plg_on_fork_proc)(plugin_t *plg, proc_id_t cpid);
    fernos_error_t (*plg_on_reap_proc)(plugin_t *plg, proc_id_t rpid);
} plugin_impl_t;


struct _plugin_t {
    const plugin_impl_t * const impl;

    /**
     * All plugins must belong to one and only one kernel state.
     */
    kernel_state_t * const ks;
};

static inline void init_base_plugin(plugin_t *plg, const plugin_impl_t *impl, kernel_state_t *ks) {
    *(const plugin_impl_t **)&(plg->impl) = impl; 
    *(kernel_state_t **)&(plg->ks) = ks;
}

/**
 * Cleanup this plugin's resources.
 *
 * When the system exits, this will be called on all plugins within the kernel.
 */
static inline void delete_plugin(plugin_t *plg) {
    if (plg) {
        plg->impl->delete_plugin(plg);
    }
}

/**
 * NOTE: The below calls all return fernos errors.
 *
 * When FOS_SUCCESS is returned, life is good! continue execution as normal.
 * When FOS_ABORT_SYSTEM is returned, ths OS locks up.
 * When any other error is returned, the plugin is deleted and removed from the system.
 */

/**
 * This function may be called on every kernel tick, or on a multiple of the ticks.
 * (For example, every 4th or every 16th tick)
 *
 * NOTE: this will be called AFTER the current thread has been advanced. The value of 
 * `curr_thread` at the time of this will be the thread which is about to be returned to.
 * (Unless there is no current thread i.e. the halt state)
 */
static inline fernos_error_t plg_tick(plugin_t *plg) {
    if (plg->impl->plg_tick) {
        return plg->impl->plg_tick(plg);
    }
    return FOS_SUCCESS;
}

/**
 * Execute `plg_tick` on an array of plugins. 
 *
 * If a plugin returns an error code other than FOS_ABORT_SYSTEM or FOS_SUCCESS, the plugin will
 * be deleted and overwritten with NULL in `plgs`.
 *
 * Returns FOS_ABORT_SYSTEM if any plugins return FOS_ABORT_SYSTEM during their tick handler.
 * Otherwise, returns FOS_SUCCESS.
 */
fernos_error_t plgs_tick(plugin_t **plgs, size_t plgs_len);

/**
 * The kernel state's current thread requested a custom command be executed by this plugin!
 *
 * `cmd_id` will likely by `arg0` from the syscall handler. Hence why the other args start at "1".
 */
fernos_error_t plg_cmd(plugin_t *plg, plugin_cmd_id_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    if (plg->impl->plg_cmd) {
        return plg->impl->plg_cmd(plg, cmd_id, arg0, arg1, arg2, arg3);
    }
    return FOS_SUCCESS;
}

/**
 * When the kernel state's current thread calls fork, this is called AFTER all core
 * resources of the child process are created. 
 *
 * `cpid` is the process ID of the child process which was created.
 */
fernos_error_t plg_on_fork_proc(plugin_t *plg, proc_id_t cpid) {
    if (plg->impl->plg_on_fork_proc) {
        return plg->impl->plg_on_fork_proc(plg, cpid);
    }
    return FOS_SUCCESS;
}

/**
 * Execute `plg_on_fork` on an array of plugins. 
 *
 * If a plugin returns an error code other than FOS_ABORT_SYSTEM or FOS_SUCCESS, the plugin will
 * be deleted and overwritten with NULL in `plgs`.
 *
 * Returns FOS_ABORT_SYSTEM if any plugins return FOS_ABORT_SYSTEM during their on fork handler.
 * Otherwise, returns FOS_SUCCESS.
 */
fernos_error_t plgs_on_fork_proc(plugin_t **plgs, size_t plgs_len, proc_id_t cpid);

/**
 * When a process is reaped by the kernel state current thread, this will be called
 * BEFORE any core process resources are cleaned up.
 *
 * NOTE: as it is gauranteed a process exits before it can be reaped, all of `rpid`'s threads 
 * will be detatched on entry to this handler.
 *
 * `rpid` is the process ID of the process to be reaped.
 */
fernos_error_t plg_on_reap_proc(plugin_t *plg, proc_id_t rpid) {
    if (plg->impl->plg_on_reap_proc) {
        return plg->impl->plg_on_reap_proc(plg, rpid);
    }
    return FOS_SUCCESS;
}

/**
 * Execute `plg_on_reap` on an array of plugins. 
 *
 * If a plugin returns an error code other than FOS_ABORT_SYSTEM or FOS_SUCCESS, the plugin will
 * be deleted and overwritten with NULL in `plgs`.
 *
 * Returns FOS_ABORT_SYSTEM if any plugins return FOS_ABORT_SYSTEM during their on reap handler.
 * Otherwise, returns FOS_SUCCESS.
 */
fernos_error_t plgs_on_reap_proc(plugin_t **plgs, size_t plgs_len, proc_id_t rpid);

