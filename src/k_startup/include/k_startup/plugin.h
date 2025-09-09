
#pragma once

#include "s_util/err.h"
#include "k_startup/state.h"

/*
 *  A plugin will be a way of extending the kernel without needing to manually
 *  change any of the core kernel structures (state, process, or thread).
 */

typedef struct _plugin_t {

    /**
     * All plugins must belong to one and only one kernel state.
     */
    kernel_state_t * const ks;
} plugin_t;

/**
 * Cleanup this plugin's resources.
 *
 * When the system exits, this will be called on all plugins within the kernel.
 */
void delete_plugin(plugin_t *plg);

/**
 * The kernel state's current thread requested a custom command be executed by this plugin!
 */
fernos_error_t plg_cmd(plugin_t *plg, uint32_t cmd_id, ...);

/**
 * When the kernel state's current thread calls fork, this is called AFTER all core
 * resources of the child process are created. 
 *
 * `cpid` is the process ID of the child process which was created.
 */
fernos_error_t plg_on_fork_proc(plugin_t *plg, proc_id_t cpid);

/**
 * When a process is reaped by the kernel state current thread, this will be called
 * before any core process resources are cleaned up.
 *
 * `rpid` is the process ID of the process to be reaped.
 */
fernos_error_t plg_on_reap_proc(plugin_t *plg, proc_id_t rpid);
