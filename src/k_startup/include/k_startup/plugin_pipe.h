
#pragma once

#include "k_startup/plugin.h"
#include "k_startup/handle.h"

/**
 * The max which will be written across a pipe in one go!
 */
#define KS_PIPE_TX_MAX_LEN (2048U)

/**
 * I have decided not to place this in `s_data` because it'd be nice if it could 
 * natively copy to userspace!
 */
typedef struct _pipe_t pipe_t;

struct _pipe_t {
    allocator_t * const al;

    /**
     * How many handle states currently exist which reference this pipe!
     */
    size_t ref_count;

    /**
     * Start of data within the pipe.
     */
    size_t i;

    /**
     * Exlusive end of data within the pipe.
     *
     * NOTE: `j` ALWAYS points to an empty byte.
     * When `i == j`, the buffer holds no data!
     * When `(j + 1) % cap == i` the buffer is full.
     *
     * This being said, the buffer MUST be at least 2 bytes or else
     * it won't be able to hold any data!
     */
    size_t j;

    /**
     * Size of the buffer in bytes. 
     * (Remember, the pipe will be able to hold only `cap - 1` significant data bytes)
     */
    const size_t cap;

    /**
     * The buffer itself.
     */
    uint8_t * const buf; 

    /**
     * When the buffer is empty (`i == j`), this holds threads which called
     * `wait_read_ready`.
     *
     * When the buffer is full, this holds threads which called `wait_write_ready`.
     */
    basic_wait_queue_t * const wq;
};

typedef struct _pipe_handle_state_t pipe_handle_state_t;

struct _pipe_handle_state_t {
    handle_state_t super;
    
    /**
     * The pipe referenced by the handle.
     */
    pipe_t * const pipe;
};

plugin_t *new_pipe_plugin(kernel_state_t *ks);

