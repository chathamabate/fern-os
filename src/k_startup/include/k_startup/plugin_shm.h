
#pragma once

#include "s_bridge/shared_defs.h"
#include "k_startup/plugin.h"
#include "s_data/binary_search_tree.h"
#include "s_data/wait_queue.h"
#include "s_data/id_table.h"
#include "s_util/constraints.h"

/**
 * Max number of semaphores allowed globally at once.
 */
#define KS_SHM_MAX_SEMS (64U)

/**
 * Maximum number of shared memory areas allowed globally at once.
 */
#define KS_SHM_MAX_SHMS (128U)

typedef struct _plugin_shm_t plugin_shm_t;
typedef struct _plugin_shm_range_t plugin_shm_range_t;
typedef struct _plugin_shm_sem_t plugin_shm_sem_t;

/**
 * ID of a shared memory range.
 */
typedef id_t shm_id_t;

struct _plugin_shm_sem_t {
    /**
     * The ID of this semaphore in the global sempahore table.
     */
    const sem_id_t id;

    /**
     * The maximum number of passes which can be lent out by this semaphore!
     */
    const uint32_t max_passes;

    /**
     * The number of passes which can still be lent out.
     *
     * When this is 0, threads must wait!
     */
    uint32_t passes;

    /**
     * Threads waiting for a pass.
     */
    basic_wait_queue_t * const bwq;

    /**
     * This represents which processes have access to this semaphore!
     *
     * If this bit vector is entirely 0, the kernel should dispose of this semaphore!
     */
    uint8_t refs[FOS_MAX_PROCS / 8];
};

/**
 * When a range structure exists like this it corresponds to an allocated area of
 * memory in the kernel memory space.
 *
 * Ranges will always be inside the FOS_SHARED_AREA!
 */
struct _plugin_shm_range_t {
    /**
     * ID of this shared memory area in the global shm table.
     */
    const shm_id_t shm_id;

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
    uint32_t references;
};

struct _plugin_shm_t {
    plugin_t super;

    /**
     * A global ID table of all semaphores!
     *
     * Will have a max cap of `KS_SHM_MAX_SEMS`.
     *
     * This maps sem_id_t -> plugin_shm_sem_t *
     */
    id_table_t * const sem_table;

    /**
     * Each node in this tree will hold a `plugin_shm_range_t` which corresponds to a real
     * mapped range within the kernel memory space!
     */
    //binary_search_tree_t * const range_tree;

    /**
     * This table maps each `pid` to a `map<void *, NULL>`.
     * Essentially, each map is really just a set holding the starting position of every range
     * mapped in the corresponding process. This is needed so that when a process exits
     * or resets, we know what ranges to dereference!
     *
     * This will also be lazy initialized. `range_maps[pid]` is NULL until 
     * a process first requests shared memory. (Or is forked from a process with shared memory)
     */
    //map_t *range_sets[FOS_MAX_PROCS];
};

plugin_t *new_plugin_shm(kernel_state_t *ks);
