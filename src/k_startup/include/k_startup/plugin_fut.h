
#pragma once

#include "k_startup/plugin.h"
#include "s_data/map.h"
#include "s_data/wait_queue.h"

/*
 * NOTE: Futexes used to be built directly into the base kernel.
 *
 * I think having a futex specific plugin is a better design though.
 */

typedef struct _plugin_fut_t plugin_fut_t;

struct _plugin_fut_t {
    plugin_t super;

    /**
     * Each process will have a futex map.
     *
     * (futex_t *) -> (basic_wait_queue_t *)
     */
    map_t *fut_maps[FOS_MAX_PROCS];
};

/**
 * Create a new futex plugin. 
 *
 * All processes currently in `ks` are given an initialized futex map.
 *
 * Returns NULL on error.
 */
plugin_t *new_plugin_fut(kernel_state_t *ks);
