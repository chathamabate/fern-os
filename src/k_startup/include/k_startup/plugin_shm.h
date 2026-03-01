
#pragma once

#include "k_startup/plugin.h"
#include "s_data/binary_search_tree.h"

typedef struct _plugin_shm_t plugin_shm_t;
typedef struct _plugin_shm_range_t plugin_shm_range_t;

/**
 * When a range structure exists like this it corresponds to an allocated area of
 * memory in the kernel memory space.
 */
struct _plugin_shm_range_t {
    /**
     * Inclusive start of the range.
     *
     * 4K-Aligned.
     */
    void * const start;

    /**
     * Exclusive end of the range.
     *
     * 4K-Aligned.
     */
    const void * const end;

    size_t references;
};

struct _plugin_shm_t {
    plugin_t super;

    binary_search_tree_t * const range_tree;
};

plugin_t *new_plugin_shm(kernel_state_t *ks);
