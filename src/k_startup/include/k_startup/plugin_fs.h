
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

typedef struct _plugin_fs_t {
    plugin_t super;

    file_sys_t * const fs;

    /**
     * This maps 
     */
    map_t *nk_map;

    id_table_t *handle_tables[FOS_MAX_PROCS];
} plugin_fs_t;


