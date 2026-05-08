
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
     * How many references in the kernel exist for this range.
     */
    uint32_t kernel_refs;

    /**
     * In what USER processes is this range mapped?
     *
     * Realize, that we could just check a process's page tables to see
     * if this range is mapped, but that is probably slower and more confusing than this.
     */
    uint8_t refs[FOS_MAX_PROCS / 8];

    /*
     * Realize that if `kernel_refs` is 0 AND `refs` is entirely 0, this range is no
     * longer used either in kernel or userspace!
     *
     * It should be unmapped!
     */
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
    binary_search_tree_t * const range_tree;
};

plugin_t *new_plugin_shm(kernel_state_t *ks);
