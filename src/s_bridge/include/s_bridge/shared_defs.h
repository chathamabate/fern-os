
#pragma once

#include "s_data/id_table.h"

/*
 * This header includes type definitions which are significant in both user and kernel space.
 */

/**
 * The globally unique ID of a process. (Can be 0)
 */
typedef id_t proc_id_t;

/**
 * The process unique ID of a thread. (Can be 0)
 */
typedef id_t thread_id_t;

/**
 * Created threads must be entered via a function conforming to this siganture!
 */
typedef void *(*thread_entry_t)(void *arg);

/**
 * A vector for specifying which 
 */
typedef uint32_t join_vector_t;

static inline join_vector_t empty_join_vector(void) {
    return 0;
}

/**
 * A vector which accepts any thread
 */
static inline join_vector_t full_join_vector(void) {
    return ~0;
}

static inline void jv_add_tid(join_vector_t *jv, thread_id_t tid) {
    if (tid < 32) {
        *jv |= (1 << tid);
    }
}

static inline void jv_remove_tid(join_vector_t *jv, thread_id_t tid) {
    if (tid < 32) {
        *jv &= ~(1 << tid);
    }
}

/**
 * When a thread is joined, these values should be copied back to user space.
 */
typedef struct _thread_join_ret_t {
    thread_id_t joined;
    void *retval;
} thread_join_ret_t;

/**
 * A futex is just a 32-bit integer.
 */
typedef int32_t futex_t;
