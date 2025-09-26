
#pragma once

#include "plugin.h"
#include "s_data/wait_queue.h"

typedef struct _plugin_kb_t plugin_kb_t;
typedef struct _plugin_kb_handle_state_t plugin_kb_handle_state_t;

struct _plugin_kb_handle_state_t {
    handle_t super;

    /**
     * The keyboard plugin which created this handle state.
     */
    plugin_kb_t * const plg_kb;

    /**
     * The next position in the scancode buffer to attempt to read from!
     */
    size_t pos;
};

#define PLG_KB_BUFFER_SIZE (128)

struct _plugin_kb_t {
    plugin_t super;

    /**
     * This is the next position to write to within the scancode buffer.
     *
     * Reading from this position will always result in FOS_EMPTY.
     * Waiting at this position will cause the calling thread to block.
     *
     * NOTE: Just keep in mind that if the user doesn't read fast enough,
     * key events will be lost from their perspective. This plugin does not 
     * keep track of which scancodes have been read by the user or not, it just
     * keeps overwritting the buffer. 
     */
    size_t sc_buf_pos;

    /**
     * `sc_buf` will be a simple cyclic buffer which contains all
     * the valid scan codes received from the keyboard.
     * (Assuming scan code set1)
     *
     * We are using uint16_t to make it easy to store extended scancodes and normal scancodes.
     */
    uint16_t sc_buf[PLG_KB_BUFFER_SIZE];

    /**
     * For threads waiting on a keypress.
     */
    basic_wait_queue_t * const bwq;
};

plugin_t *new_plugin_kb(kernel_state_t *ks);


