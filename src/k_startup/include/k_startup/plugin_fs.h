
#pragma once

#include "k_startup/plugin.h"
#include "k_startup/state.h"
#include "s_block_device/file_sys.h"
#include "s_data/map.h"

/*
 * The file system plugin!
 *
 * This replaces the old explicit file system work within the kernel state.
 */

typedef struct _plugin_fs_nk_map_entry_t {
    /**
     * Number of places in this plugin 'nk' is used.
     * (other than as a key in the nk map)
     */
    size_t references;

    /**
     * All threads which are waiting on this file to be appended to.
     */
    basic_wait_queue_t *bwq;
} plugin_fs_nk_map_entry_t;

typedef struct _plugin_fs_handle_state_t {
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
} plugin_fs_handle_state_t;

typedef struct _plugin_fs_proc_state_t {
    /**
     * The current working directory of this process.
     */
    fs_node_key_t cwd;

    /**
     * This process's set of file handles.
     *
     * maps to plugin_fs_handle_state_t *'s
     */
    id_table_t *handle_table;
} plugin_fs_proc_state_t;

typedef struct _plugin_fs_t {
    plugin_t super;

    file_sys_t * const fs;

    /**
     * This maps node_key_t's -> nk_map_entry_t *'s
     */
    map_t * const nk_map;

    /**
     * Map from proc_id -> handle table *.
     * A "handle table" will map plugin fs handles -> plugin_fs_handle_state_t *
     *
     * If there is no process with proc_id `pid`, then handle_tables[pid] will be NULL.
     */
    id_table_t *handle_tables[FOS_MAX_PROCS];
} plugin_fs_t;

/**
 * Create a new file system plugin.
 *
 * NOTE: On deletion, the underlying file system is deleted!
 *
 * Returns NULL on error.
 */
plugin_t *new_plugin_fs(kernel_state_t *ks, file_sys_t *fs);


