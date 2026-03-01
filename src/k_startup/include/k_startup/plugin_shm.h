
#pragma once

#include "k_startup/plugin.h"
#include "s_data/binary_search_tree.h"
#include "s_util/constraints.h"

typedef struct _plugin_shm_t plugin_shm_t;
typedef struct _plugin_shm_range_t plugin_shm_range_t;

/**
 * When a range structure exists like this it corresponds to an allocated area of
 * memory in the kernel memory space.
 *
 * Ranges will always be inside the FOS_SHARED_AREA!
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

    /**
     * How many reference this range has.
     *
     * When this reaches 0, the range will be unmapped in the kernel, and it's underlying pages 
     * returned to the page free list.
     */
    size_t references;
};

struct _plugin_shm_t {
    plugin_t super;

    /**
     * Each node in this tree will hold a `plugin_shm_range_t` which corresponds to a real
     * mapped range within the kernel memory space!
     */
    binary_search_tree_t * const range_tree;

    /**
     */
    map_t *range_maps[FOS_MAX_PROCS];
};

plugin_t *new_plugin_shm(kernel_state_t *ks);
