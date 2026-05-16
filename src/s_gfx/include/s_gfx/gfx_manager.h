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

    /*
     * Every buffer managed by one manager MUST have the same abstract width and height!
     */

    uint16_t width;
    uint16_t height;
};

struct _gfx_manager_impl_t {
    /**
     * REQUIRED
     */
    gfx_color_t *(*gm_get_front)(gfx_manager_t *gm);

    /**
     * OPTIONAL
     */
    void (*delete_gfx_manager)(gfx_manager_t *gm);
    gfx_color_t *(*gm_get_back)(gfx_manager_t *gm);
    void (*gm_swap)(gfx_manager_t *gm);

    /**
     * This handle will always receive non-NULL pointers for `width` and `height`.
     * When called `*width` and `*height` will equal the requested new dimmensions of 
     * manager.
     *
     * On both ERROR and SUCCESS, this handler should write the NEW dimmensions to `*width`
     * and `*height`.
     * This handler should NOT edit `super.width` or `super.height` this will be done
     * automatically by the wrapper!
     *
     * This should only return FOS_E_SUCCESS if the buffers were resized corectly
     * to sizes `*width` and `*height.
     */
    fernos_error_t (*gm_resize)(gfx_manager_t *gm, uint16_t *width, uint16_t *height);
};

/**
 * Initialize the gfx manager base structure.
 * As always `impl` has its POINTER copied into `gm`... so make sure `*impl` is a global const!
 */
void init_gfx_manager_base(gfx_manager_t *gm, const gfx_manager_impl_t *impl, uint16_t width, uint16_t height);

static inline uint16_t gm_get_width(gfx_manager_t *gm) {
    return gm->width;
}

static inline uint16_t gm_get_height(gfx_manager_t *gm) {
    return gm->height;
}

/**
 * Delete a graphics manager, does nothing if the manager has no destructor!
 */
static inline void delete_gfx_manager(gfx_manager_t *gm) {
    if (gm && gm->impl->delete_gfx_manager) {
        gm->impl->delete_gfx_manager(gm);
    }
}

/**
 * A graphics manager may manage two buffers (a back and front), but it may also just have 1 
 * buffer. In most cases this should really cause any differences in code. Just understand
 * a single buffer may result in screen tearing depending on how it's used.
 *
 * HOWEVER, If your rendering actually tries to remember the state of each buffer to reduce
 * redundant rendering, you should always check if the graphics manager actually has a back buffer!
 * If it does not, your strategy may not work as expected!
 */
static inline bool gm_has_back(gfx_manager_t *gm) {
    return gm->impl->gm_get_back != NULL;
}

/**
 * Get the front buffer!
 */
static inline gfx_buffer_t gm_get_front(gfx_manager_t *gm) {
    return (gfx_buffer_t) {
        .buffer = gm->impl->gm_get_front(gm),
        .width = gm->width, .height = gm->height
    };
}

/**
 * Get the back buffer!
 *
 * If `gm_get_back` is not implemented, this just defers to `gm_get_front`.
 */
static inline gfx_buffer_t gm_get_back(gfx_manager_t *gm) {
    if (gm->impl->gm_get_back) {
        return (gfx_buffer_t) {
            .buffer = gm->impl->gm_get_back(gm),
            .width = gm->width, .height = gm->height
        };
    }
    return gm_get_front(gm);
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
 * On FOS_E_SUCCESS, all managed buffers will have dimmensions `width * height`.
 * On any other error value, make sure to check `gm_get_width` and `gm_get_height`,
 * to determine the buffer dimmensions after this call.
 */
fernos_error_t gm_resize(gfx_manager_t *gm, uint16_t width, uint16_t height);

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
} dynamic_gfx_manager_t;

/**
 * Create a new dynamic graphics manager.
 *
 * On success, the two dynamic buffers will have sizes `width` by `height.
 */
gfx_manager_t *new_dynamic_gfx_manager(allocator_t *al, uint16_t width, uint16_t height);
