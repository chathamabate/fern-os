
#pragma once

#include "k_startup/plugin.h"
#include "k_startup/handle.h"

/**
 * The max which will be written across a pipe in one go!
 */
#define KS_PIPE_TX_MAX_LEN (2048U)

/**
 * Max size of a pipe!
 */
#define KS_PIPE_MAX_LEN (0xFFFFFU)

/**
 * I have decided not to place this in `s_data` because it'd be nice if it could 
 * natively copy to userspace!
 */
typedef struct _pipe_t pipe_t;

struct _pipe_t {
    allocator_t * const al;

    /**
     * How many write handles exist for this pipe. 
     */
    size_t write_refs;

    /**
     * How many read handles exist for this pipe.
     */
    size_t read_refs;

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
     * Writing threads waiting for the pipe to be non-full.
     */
    basic_wait_queue_t * const w_wq;

    /**
     * Reading threads waiting for the pipe to be non-empty.
     */
    basic_wait_queue_t * const r_wq;
};

typedef struct _pipe_handle_state_t pipe_handle_state_t;

/**
 * Kinda confusing here, but both reader and writer handles will use the same structure.
 * Which implementation pointer is used determines the type!
 */
struct _pipe_handle_state_t {
    handle_state_t super;
    
    /**
     * The pipe referenced by the handle.
     */
    pipe_t * const pipe;
};

plugin_t *new_plugin_pipe(kernel_state_t *ks);

