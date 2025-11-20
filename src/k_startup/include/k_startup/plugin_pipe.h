
#pragma once

#include "k_startup/plugin.h"
#include "k_startup/handle.h"

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
     * This will hold threads which are waiting for there to be data to read.
     * The wait context will be empty, as threads are just notified that there may be data.
     */
    basic_wait_queue_t * const read_wq;

    /**
     * This will hold threads which are waiting to write data. 
     * This is a unique scenario specific to this plugin. Vanilla writing will block
     * until there is space in the pipe to write at least 1 byte.
     *
     * wait_ctx[0] = (const void *)u_src
     * wait_ctx[1] = (size_t)len
     * wait_ctx[2] = (size_t *)u_written
     *
     * NOTE: pipe handles will also expose other versions of write which will
     * never block. We need a blocking vanilla wait though for this function
     * to work the way the helpers expect.
     */
    basic_wait_queue_t * const write_wq;
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

