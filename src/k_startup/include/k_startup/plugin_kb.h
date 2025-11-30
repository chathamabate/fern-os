
#pragma once

#include "k_startup/plugin.h"
#include "k_startup/handle.h"
#include "s_data/wait_queue.h"

typedef struct _plugin_kb_t plugin_kb_t;
typedef struct _plugin_kb_handle_state_t plugin_kb_handle_state_t;

struct _plugin_kb_handle_state_t {
    handle_state_t super;

    /**
     * The keyboard plugin which created this handle state.
     */
    plugin_kb_t * const plg_kb;

    /**
     * The next position in the scancode buffer to attempt to read from!
     *
     * NOTE: Remember this will be a byte index! Scancodes will
     * always begin at even positions within the buffer. However, this
     * position can take on odd values if the user desires.
     */
    size_t pos;
};

/**
 * This MUST be divisible by the size of a scancode (2 bytes)
 */
#define PLG_KB_BUFFER_SIZE (256)

struct _plugin_kb_t {
    plugin_t super;

    /**
     * This is the next position to write to within the scancode buffer.
     *
     * Reading from this position will always result in FOS_EMPTY.
     * Waiting at this position will cause the calling thread to block.
     *
     * NOTE: This will always be a multiple of sizeof(scs1_code_t)
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
     * NOTE: This will be a buffer of `uint8_t` because the user is allowed to read a just a 
     * single byte should they choose. (This would be a stupid decision, but an allowed one)
     *
     * Scancodes are gauranteed to be stored little endian WITH 0 padding
     * for non-extended codes. (i.e. ALL codes will take up 2 bytes in the below buffer)
     */ 
    uint8_t sc_buf[PLG_KB_BUFFER_SIZE];

    /**
     * For threads waiting on a keypress.
     */
    basic_wait_queue_t * const bwq;
};

plugin_t *new_plugin_kb(kernel_state_t *ks);


