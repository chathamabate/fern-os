#pragma once

#include "s_gfx/gfx.h"

typedef struct _gfx_manager_t gfx_manager_t;
typedef struct _gfx_manager_impl_t gfx_manager_impl_t;

/**
 * This is supposed to be a solution to the confusion of managing static/dynamic/shared memory
 * gfx buffers from within the same struct.
 *
 * (It also optionally helps with buffer swapping)
 */
struct _gfx_manager_t {
    const gfx_manager_impl_t * const impl;
};

struct _gfx_manager_impl_t {
    /**
     * REQUIRED
     */
    gfx_buffer_t (*gm_get_front)(gfx_manager_t *gm);

    /**
     * OPTIONAL
     */
    void (*delete_gfx_manager)(gfx_manager_t *gm);
    gfx_buffer_t (*gm_get_back)(gfx_manager_t *gm);
    void (*gm_swap)(gfx_manager_t *gm);
    fernos_error_t (*gm_resize)(gfx_manager_t *gm, uint16_t width, uint16_t height);
};

/**
 * Initialize the gfx manager base structure.
 * As always `impl` has its POINTER copied into `gm`... so make sure `*impl` is a global const!
 */
void init_gfx_manager_base(gfx_manager_t *gm, const gfx_manager_impl_t *impl);

/**
 * Delete a graphics manager, does nothing if the manager has no destructor!
 */
static inline void delete_gfx_manager(gfx_manager_t *gm) {
    if (gm && gm->impl->delete_gfx_manager) {
        gm->impl->delete_gfx_manager(gm);
    }
}

/**
 * Get the front buffer!
 */
static inline gfx_buffer_t gm_get_front(gfx_manager_t *gm) {
    return gm->impl->gm_get_front(gm);
}

/**
 * Get the back buffer!
 *
 * If `gm_get_front` is not implemented, this just returns a gfx_buffer whose
 * buffer is NULL.
 */
static inline gfx_buffer_t gm_get_back(gfx_manager_t *gm) {
    if (gm->impl->gm_get_back) {
        return gm->impl->gm_get_back(gm);
    }
    return (gfx_buffer_t) {.buffer = NULL};
}


/**
 * This function should abstractly rotate/swap the buffers managed.
 * Front goes to back, and back goes to front.
 */
static inline void gm_swap(gfx_manager_t *gm) {
    if (gm->impl->gm_swap) {
        gm->impl->gm_swap(gm);
    }
}

/**
 * Resize ALL managed buffers to have dimmensions `width * height`.
 *
 * Your manager may be entirely static, and thus have no ability to resize anything!
 * This is totally ok, just don't provide a `resize` endpoint!
 */
static inline fernos_error_t gm_resize(gfx_manager_t *gm, uint16_t width, uint16_t height) {
    if (gm->impl->gm_resize) {
        return gm->impl->gm_resize(gm, width, height);
    }
    return FOS_E_NOT_IMPLEMENTED;
}

/**
 * A graphics manager which manages two resizeable buffers which are dynamically allocated.
 */
typedef struct _dynamic_gfx_manager_t {
    gfx_manager_t base;
    allocator_t * const al;

    /**
     * Index of the front buffer.
     */
    size_t front_i;

    /**
     * Both buffers in `bufs` will have size at least `sizeof(gfx_color_t) * buf_len`.
     */
    size_t buf_len;
    gfx_color_t *bufs[2]; 

    /**
     * The current width and height of both of the `bufs`.
     * It is promised that `width * height <= buf_len`.
     */
    uint16_t width, height;
} dynamic_gfx_manager_t;

/**
 * Create a new dynamic graphics manager.
 */
gfx_manager_t *new_dynamic_gfx_manager(allocator_t *al);
