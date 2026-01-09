
#include "k_sys//m2.h"

const m2_info_tag_base_t *m2_next_info_tag(const m2_info_tag_base_t *it) {
    if (!it) {
        return NULL;
    }

    if (it->type == M2_ITT_NULL) {
        return NULL;
    }

    const uint8_t *next = ((const uint8_t *)it + it->size);
    if (!(IS_ALIGNED(next, 8))) {
        next += 8;
        next = (const uint8_t *)ALIGN(next, 8);
    }

    const m2_info_tag_base_t *next_tag = (const m2_info_tag_base_t *)next;

    if (next_tag->type == M2_ITT_NULL) {
        return NULL;
    }

    return next_tag;
}

const m2_info_tag_base_t *m2_find_info_tag(const m2_info_tag_base_t *it, uint32_t type) {
    while (it) {
        if (it->type == type) {
            return it;
        }

        it = m2_next_info_tag(it);
    }

    return NULL;
}
