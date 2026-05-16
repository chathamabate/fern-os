#include "s_gfx/gfx_manager.h"

void init_gfx_manager_base(gfx_manager_t *gm, const gfx_manager_impl_t *impl) {
    *(const gfx_manager_impl_t **)&(gm->impl) = impl;
}

static void delete_dynamic_gfx_manager(gfx_manager_t *gm);
static gfx_buffer_t dgm_get_front(gfx_manager_t *gm);
static gfx_buffer_t dgm_get_back(gfx_manager_t *gm);
static void dgm_swap(gfx_manager_t *gm);
static fernos_error_t dgm_resize(gfx_manager_t *gm, uint16_t width, uint16_t height);

static const gfx_manager_impl_t DGM_IMPL = {
    .gm_get_front = dgm_get_front,
    .delete_gfx_manager = delete_dynamic_gfx_manager,
    .gm_get_back = dgm_get_back,
    .gm_swap = dgm_swap,
    .gm_resize = dgm_resize
};

gfx_manager_t *new_dynamic_gfx_manager(allocator_t *al) {
    if (!al) {
        return NULL;
    }
    
    dynamic_gfx_manager_t *dgm = al_malloc(al, sizeof(dynamic_gfx_manager_t));
    if (!dgm) {
        return NULL;
    }

    init_gfx_manager_base((gfx_manager_t *)dgm, &DGM_IMPL);
    *(allocator_t **)&(dgm->al) = al;
    dgm->front_i = 0;
    dgm->buf_len = 0;
    dgm->bufs[0] = NULL;
    dgm->bufs[1] = NULL;
    dgm->width = 0;
    dgm->width = 0;
    
    return (gfx_manager_t *)dgm;
}

static void delete_dynamic_gfx_manager(gfx_manager_t *gm) {
    dynamic_gfx_manager_t *dgm = (dynamic_gfx_manager_t *)gm;
    al_free(dgm->al, dgm->bufs[0]);
    al_free(dgm->al, dgm->bufs[1]);
    al_free(dgm->al, dgm);
}

static gfx_buffer_t dgm_get_front(gfx_manager_t *gm) {
    dynamic_gfx_manager_t *dgm = (dynamic_gfx_manager_t *)gm;
    return (gfx_buffer_t) {
        .buffer = dgm->bufs[dgm->front_i],
        .width = dgm->width, .height = dgm->height
    };
}

static gfx_buffer_t dgm_get_back(gfx_manager_t *gm) {
    dynamic_gfx_manager_t *dgm = (dynamic_gfx_manager_t *)gm;
    return (gfx_buffer_t) {
        .buffer = dgm->bufs[dgm->front_i ^ 1],
        .width = dgm->width, .height = dgm->height
    };
}

static void dgm_swap(gfx_manager_t *gm) {
    dynamic_gfx_manager_t *dgm = (dynamic_gfx_manager_t *)gm;
    dgm->front_i ^= 1;
}

static fernos_error_t dgm_resize(gfx_manager_t *gm, uint16_t width, uint16_t height) {
    dynamic_gfx_manager_t *dgm = (dynamic_gfx_manager_t *)gm;

    size_t new_buf_len = width * height;

    // Only resize in the case of a stretch!
    if (new_buf_len > dgm->buf_len) {
        // Here it is IMPOSSIBLE for the value of `new_buf_len` to be 0.
        dgm->bufs[0] = al_realloc(dgm->al, dgm->bufs[0], new_buf_len);
        dgm->bufs[1] = al_realloc(dgm->al, dgm->bufs[1], new_buf_len);

        if (!(dgm->bufs[0]) || !(dgm->bufs[1])) {
            // Error case just resets the dgm.
            al_free(dgm->al, dgm->bufs[0]);
            al_free(dgm->al, dgm->bufs[1]);

            dgm->bufs[0] = NULL;
            dgm->bufs[1] = NULL;

            dgm->buf_len = 0;
            dgm->width = 0;
            dgm->height = 0;

            return FOS_E_NO_MEM;
        }

        dgm->buf_len = new_buf_len;
    }

    dgm->width = width;
    dgm->height = height;

    return FOS_E_SUCCESS;
}
