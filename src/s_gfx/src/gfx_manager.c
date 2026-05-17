#include "s_gfx/gfx_manager.h"

void init_gfx_manager_base(gfx_manager_t *gm, const gfx_manager_impl_t *impl, uint16_t width, uint16_t height) {
    *(const gfx_manager_impl_t **)&(gm->impl) = impl;
    gm->width = width;
    gm->height = height;
}

fernos_error_t gm_resize(gfx_manager_t *gm, uint16_t width, uint16_t height) {
    fernos_error_t err;

    if (!(gm->impl->gm_resize)) {
        return FOS_E_NOT_IMPLEMENTED;
    }

    err = gm->impl->gm_resize(gm, width, height);
    if (err != FOS_E_SUCCESS) {
        // Remember that on error, the old `width` and `height` should retain.
        return err;
    }

    gm->width = width;
    gm->height = height;
    return FOS_E_SUCCESS;
}

static void delete_dynamic_gfx_manager(gfx_manager_t *gm);
static gfx_color_t *dgm_get_front(gfx_manager_t *gm);
static gfx_color_t *dgm_get_back(gfx_manager_t *gm);
static void dgm_swap(gfx_manager_t *gm);
static fernos_error_t dgm_resize(gfx_manager_t *gm, uint16_t width, uint16_t height);

static const gfx_manager_impl_t DGM_IMPL = {
    .gm_get_front = dgm_get_front,
    .delete_gfx_manager = delete_dynamic_gfx_manager,
    .gm_get_back = dgm_get_back,
    .gm_swap = dgm_swap,
    .gm_resize = dgm_resize
};

gfx_manager_t *new_dynamic_gfx_manager(allocator_t *al, uint16_t width, uint16_t height) {
    if (!al) {
        return NULL;
    }

    size_t init_full_buf_len = 2 * (size_t)width * (size_t)height;
    
    dynamic_gfx_manager_t *dgm = al_malloc(al, sizeof(dynamic_gfx_manager_t));
    gfx_color_t *full_buf = al_malloc(al, init_full_buf_len * sizeof(gfx_color_t));

    if (!dgm || 
            (init_full_buf_len > 0 && !full_buf)
        ) {
        al_free(al, full_buf);
        al_free(al, dgm);

        return NULL;
    }

    init_gfx_manager_base((gfx_manager_t *)dgm, &DGM_IMPL, width, height);
    *(allocator_t **)&(dgm->al) = al;
    dgm->front_i = 0;

    dgm->bufs[0] = full_buf + (0 * 0);
    dgm->bufs[1] = full_buf + ((size_t)width * (size_t)height);

    dgm->full_buf_len = init_full_buf_len;
    dgm->full_buf = full_buf;
    
    return (gfx_manager_t *)dgm;
}

static void delete_dynamic_gfx_manager(gfx_manager_t *gm) {
    dynamic_gfx_manager_t *dgm = (dynamic_gfx_manager_t *)gm;
    al_free(dgm->al, dgm->bufs[0]);
    al_free(dgm->al, dgm->bufs[1]);
    al_free(dgm->al, dgm);
}

static gfx_color_t *dgm_get_front(gfx_manager_t *gm) {
    dynamic_gfx_manager_t *dgm = (dynamic_gfx_manager_t *)gm;
    return dgm->bufs[dgm->front_i];
}

static gfx_color_t *dgm_get_back(gfx_manager_t *gm) {
    dynamic_gfx_manager_t *dgm = (dynamic_gfx_manager_t *)gm;
    return dgm->bufs[dgm->front_i ^ 1];
}

static void dgm_swap(gfx_manager_t *gm) {
    dynamic_gfx_manager_t *dgm = (dynamic_gfx_manager_t *)gm;
    dgm->front_i ^= 1;
}

static fernos_error_t dgm_resize(gfx_manager_t *gm, uint16_t width, uint16_t height) {
    dynamic_gfx_manager_t *dgm = (dynamic_gfx_manager_t *)gm;

    size_t new_full_buf_len = 2 * (size_t)width * (size_t)height;

    // Only resize in the case of a stretch!
    if (new_full_buf_len > dgm->full_buf_len) {
        gfx_color_t *new_full_buf = al_realloc(dgm->al, dgm->full_buf, new_full_buf_len * sizeof(gfx_color_t));
        if (!new_full_buf) {
            // On resize failure, everything stays the same!
            return FOS_E_NO_MEM;
        }

        // Resize succeeded!

        dgm->full_buf_len = new_full_buf_len;
        dgm->full_buf = new_full_buf;

        dgm->bufs[0] = dgm->full_buf + (0 * 0);
        dgm->bufs[1] = dgm->full_buf + ((size_t)width * (size_t)height);
    }

    return FOS_E_SUCCESS;
}

static void delete_dynamic_gfx_manager_single(gfx_manager_t *gm);
static gfx_color_t *dgms_get_front(gfx_manager_t *gm);
static fernos_error_t dgms_resize(gfx_manager_t *gm, uint16_t width, uint16_t height);

static const gfx_manager_impl_t DGMS_IMPL = {
    .delete_gfx_manager = delete_dynamic_gfx_manager_single,
    .gm_get_front = dgms_get_front,
    .gm_resize = dgms_resize
};

gfx_manager_t *new_dynamic_gfx_manager_single(allocator_t *al, uint16_t width, uint16_t height) {
    if (!al) {
        return NULL;
    }

    size_t init_buf_len = (size_t)width * (size_t)height;
    
    dynamic_gfx_manager_single_t *dgms = al_malloc(al, sizeof(dynamic_gfx_manager_single_t));
    gfx_color_t *buf = al_malloc(al, init_buf_len * sizeof(gfx_color_t));

    if (!dgms || (init_buf_len > 0 && !buf)) {
        al_free(al, dgms);
        al_free(al, buf);
        return NULL;
    }

    init_gfx_manager_base((gfx_manager_t *)dgms, &DGMS_IMPL, width, height);  
    *(allocator_t **)&(dgms->al) = al;
    dgms->buf_len = init_buf_len;
    dgms->buf = buf;

    return (gfx_manager_t *)dgms;
}

static void delete_dynamic_gfx_manager_single(gfx_manager_t *gm) {
    dynamic_gfx_manager_single_t *dgms = (dynamic_gfx_manager_single_t *)gm;
    al_free(dgms->al, dgms->buf);
    al_free(dgms->al, dgms);

}

static gfx_color_t *dgms_get_front(gfx_manager_t *gm) {
    dynamic_gfx_manager_single_t *dgms = (dynamic_gfx_manager_single_t *)gm;
    return dgms->buf;
}

static fernos_error_t dgms_resize(gfx_manager_t *gm, uint16_t width, uint16_t height) {
    dynamic_gfx_manager_single_t *dgms = (dynamic_gfx_manager_single_t *)gm;
    size_t new_buf_len = (size_t)width * (size_t)height;

    if (new_buf_len > dgms->buf_len) {
        gfx_color_t *new_buf = al_realloc(dgms->al, dgms->buf, new_buf_len * sizeof(gfx_color_t));
        if (!new_buf) {
            return FOS_E_NO_MEM;
        }

        dgms->buf = new_buf;
        dgms->buf_len = new_buf_len;
    }

    return FOS_E_SUCCESS;
}
