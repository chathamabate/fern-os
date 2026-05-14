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

    /**
     * A manager will manage 1 or more buffers!
     */
    const size_t num_buffers;
};

struct _gfx_manager_impl_t {
    /**
     * REQUIRED
     */
    gfx_buffer_t (*gm_get_buffer)(gfx_manager_t *gm, size_t i);

    /**
     * OPTIONAL
     */
    void (*gm_swap)(gfx_manager_t *gm);
    fernos_error_t (*gm_resize)(gfx_manager_t *gm, uint16_t width, uint16_t height);
};

/**
 * Initialize the gfx manager base structure.
 * As always `impl` has its POINTER copied into `gm`... so make sure `*impl` is a global const!
 */
void init_gfx_manager_base(gfx_manager_t *gm, gfx_manager_impl_t *impl, size_t num_buffers);

/**
 * Get buffer `i`.
 *
 * VERY IMPORTANT: The point of this structure is that `gfx_buffer_t`'s should now be short lived
 * and quickly forgotten. It should be gauranteed that when calling this function, a valid buffer for 
 * immediate rendering is returned!
 * However, after a `gm_swap` or a `gm_resize` the previously returned `gfx_buffer_t` may be enitrely
 * unusable!
 * NEVER store the returned `gfx_buffer_t` in some structure for later use. Only call this function
 * if you are interested in rendering NOW!
 */
static inline gfx_buffer_t gm_get_buffer(gfx_manager_t *gm, size_t i) {
    if (i >= gm->num_buffers) {
        return (gfx_buffer_t){0};
    }

    return gm->impl->gm_get_buffer(gm, i); 
}

/**
 * This function should abstractly rotate/swap the buffers managed.
 *
 * Buffer 0 -> Buffer `num_buffers - 1`
 * Buffer 1 -> Buffer 0
 * Buffer 2 -> Buffer 1
 * ....
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
