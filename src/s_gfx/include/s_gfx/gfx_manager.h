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
    fernos_error_t (*gm_resize)(gfx_manager_t *gm, uint16_t width, uint16_t height);
};

void init_gfx_manager_base(gfx_manager_t *gm, gfx_manager_impl_t *impl, size_t num_buffers);

static inline gfx_buffer_t gm_get_buffer(gfx_manager_t *gm, size_t i) {
    if (i >= gm->num_buffers) {
        return (gfx_buffer_t){0};
    }

    return gm->impl->gm_get_buffer(gm, i); 
}

static inline fernos_error_t gm_resize(gfx_manager_t *gm, uint16_t width, uint16_t height) {
    if (gm->impl->gm_resize) {
        return gm->impl->gm_resize(gm, width, height);
    }
    return FOS_E_NOT_IMPLEMENTED;
}
