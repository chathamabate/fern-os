
#pragma once


#include "s_util/err.h"
#include "k_startup/state.h"
#include "s_bridge/shared_defs.h"
#include "k_startup/thread.h"

/*
 *  A plugin will be a way of extending the kernel without needing to manually
 *  change any of the core kernel structures (state, process, or thread).
 *
 *  As of today (9/8/2025), a plugin's interface will be somewhat simple. Essentially a plugin is
 *  just a listener for certain system events. Right now, only a few events are listened for,
 *  later if I feel the need, I will expand this interface. 
 *
 *  VERY IMPORTANT: As of now, plugins are not meant to be removeable/loadable at runtime.
 *  They are meant to be set in the kernel at system startup. A plugin will be active for
 *  the entire life time of the operating system. 
 */


typedef struct _plugin_impl_t {
    /*
     * Below calls are ALL OPTIONAL!
     */

    void (*plg_on_shutdown)(plugin_t *plg);
    fernos_error_t (*plg_kernel_cmd)(plugin_t *plg, plugin_kernel_cmd_id_t kcmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);

    fernos_error_t (*plg_tick)(plugin_t *plg);
    fernos_error_t (*plg_cmd)(plugin_t *plg, plugin_cmd_id_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);
    fernos_error_t (*plg_on_fork_proc)(plugin_t *plg, proc_id_t cpid);
    fernos_error_t (*plg_on_reset_proc)(plugin_t *plg, proc_id_t pid);
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
 * A plugin is never deleted. It lives until the system shuts down.
 *
 * This handler is called when shutdown occurs. NOTE: FernOS may shutdown in case of a system 
 * error. In this situation, the state of the kernel may not be 100% known. 
 *
 * The implementation of this function should only perform extremely safe and essential clean
 * up calls. For example, maybe flushing the file system to disk. This call is NOT meant to
 * free allocated resources of this plugin. Avoid anything that is not absolutely critical!
 */
static inline void plg_on_shutdown(plugin_t *plg) {
    if (plg->impl->plg_on_shutdown) {
        plg->impl->plg_on_shutdown(plg);
    }
}

/**
 * This function gives a plugin and endpoint which is safe to call from within the kernel.
 * For example, what if one plugin wants to invoke a behavior of another plugin?
 * `plg_cmd` is always relative to a calling thread, it is a system call.
 *
 * This function is NOT! It has no implicitrelation ship with the current thread or userspace.
 */
static inline fernos_error_t plg_kernel_cmd(plugin_t *plg, plugin_kernel_cmd_id_t kcmd, uint32_t arg0,
        uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    if (plg->impl->plg_kernel_cmd) {
        return plg->impl->plg_kernel_cmd(plg, kcmd, arg0, arg1, arg2, arg3);
    }
    return FOS_E_SUCCESS;
}

/**
 * NOTE: The below calls all return fernos errors.
 *
 * When FOS_E_SUCCESS is returned, life is good! continue execution as normal.
 * In the case of ANY OTHER error, the system will shutdown!
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
    return FOS_E_SUCCESS;
}

/**
 * Execute `plg_tick` on an array of plugins. 
 *
 * Returns FOS_E_SUCCESS if and only if every tick call succeeds. 
 * Returns early if an error is encountered.
 */
fernos_error_t plgs_tick(plugin_t **plgs, size_t plgs_len);

/**
 * The kernel state's current thread requested a custom command be executed by this plugin!
 *
 * Any non-success error code returned here in kernel-space shuts down the system.
 */
KS_SYSCALL static inline fernos_error_t plg_cmd(plugin_t *plg, plugin_cmd_id_t cmd_id, uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    if (plg->impl->plg_cmd) {
        return plg->impl->plg_cmd(plg, cmd_id, arg0, arg1, arg2, arg3);
    }

    thread_t *thr = (thread_t *)(plg->ks->schedule.head);

    if (thr) {
        DUAL_RET(thr, FOS_E_NOT_IMPLEMENTED, FOS_E_SUCCESS);
    } else {
        return FOS_E_STATE_MISMATCH;
    }
}

/**
 * When the kernel state's current thread calls fork, this is called AFTER all core
 * resources of the child process are created. 
 *
 * `cpid` is the process ID of the child process which was created.
 */
static inline fernos_error_t plg_on_fork_proc(plugin_t *plg, proc_id_t cpid) {
    if (plg->impl->plg_on_fork_proc) {
        return plg->impl->plg_on_fork_proc(plg, cpid);
    }
    return FOS_E_SUCCESS;
}

/**
 * Execute `plg_on_fork` on an array of plugins. 
 */
fernos_error_t plgs_on_fork_proc(plugin_t **plgs, size_t plgs_len, proc_id_t cpid);

/**
 * When `proc_exec` is called, a process is completely overwritten (except for its default IO handles).
 * All of its children/zombie process are adopted by the root process.
 * All it's threads are deleted or reset. And its memory space is replaced.
 *
 * BEFORE ANY of these things happen though, this handler will be called with the `pid` of the
 * process which is about to be reset.
 */
static inline fernos_error_t plg_on_reset_proc(plugin_t *plg, proc_id_t pid) {
    if (plg->impl->plg_on_reset_proc) {
        return plg->impl->plg_on_reset_proc(plg, pid);
    }
    return FOS_E_SUCCESS;
}

/**
 * Execute `plg_on_reset_proc` on an array of plugins.
 */
fernos_error_t plgs_on_reset_proc(plugin_t **plgs, size_t plgs_len, proc_id_t pid);

/**
 * When a process is reaped by the kernel state current thread, this will be called
 * BEFORE any core process resources are cleaned up. (This includes handles)
 *
 * NOTE: as it is gauranteed a process exits before it can be reaped, all of `rpid`'s threads 
 * will be detatched on entry to this handler.
 *
 * `rpid` is the process ID of the process to be reaped.
 */
static inline fernos_error_t plg_on_reap_proc(plugin_t *plg, proc_id_t rpid) {
    if (plg->impl->plg_on_reap_proc) {
        return plg->impl->plg_on_reap_proc(plg, rpid);
    }
    return FOS_E_SUCCESS;
}

/**
 * Execute `plg_on_reap` on an array of plugins. 
 */
fernos_error_t plgs_on_reap_proc(plugin_t **plgs, size_t plgs_len, proc_id_t rpid);

