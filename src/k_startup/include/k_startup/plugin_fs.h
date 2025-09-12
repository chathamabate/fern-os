
#pragma once

#include "k_startup/handle.h"
#include "k_startup/plugin.h"
#include "k_startup/state.h"
#include "s_block_device/file_sys.h"
#include "s_data/map.h"

typedef struct _plugin_fs_t plugin_fs_t;
typedef struct _plugin_fs_handle_state_t plugin_fs_handle_state_t;
typedef struct _plugin_fs_nk_map_entry_t plugin_fs_nk_map_entry_t;

/*
 * The file system plugin!
 *
 * This replaces the old explicit file system work within the kernel state.
 */

struct _plugin_fs_handle_state_t {
    handle_state_t super;

    plugin_fs_t * const plg_fs;

    /**
     * The position of this handle in the file.
     *
     * If `pos` = len(file), we are at the end of the file!
     */
    size_t pos;

    /**
     * The node key used by this handle.
     *
     * (Remember, not owned here!, the actual is in the `nk_map`, 
     * this is just a shallow copy)
     */
    fs_node_key_t nk;
};

struct _plugin_fs_nk_map_entry_t {
    /**
     * Number of places in this plugin 'nk' is used.
     * (other than as a key in the nk map)
     *
     * This includes in the cwd array and in fs handle states.
     */
    size_t references;

    /**
     * All threads which are waiting on this file to be appended to.
     */
    basic_wait_queue_t *bwq;
};

struct _plugin_fs_t {
    plugin_t super;

    file_sys_t * const fs;

    /**
     * This maps node_key_t's -> nk_map_entry_t *'s
     */
    map_t * const nk_map;

    /**
     * The current working directory of each process. 
     *
     * If a process exists, it will have a non-null value in this array!
     */
    fs_node_key_t cwds[FOS_MAX_PROCS];
};

/**
 * Create a new file system plugin.
 *
 * NOTE: On deletion, the underlying file system is deleted!
 *
 * NOTE: This plugin is only designed to be deleted on system shutdown.
 * The destructor ALWAYS will return FOS_ABORT_SYSTEM.
 * (remember, that when the system shutdown, all plugins are deleted, and there error
 * codes are ignored). All the destructor does is flush the file system and return.
 *
 * Returns NULL on error.
 */
plugin_t *new_plugin_fs(kernel_state_t *ks, file_sys_t *fs);


