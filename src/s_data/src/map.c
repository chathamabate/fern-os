
#include "s_data/map.h"

void init_map(map_t *mp, const map_impl_t *impl, size_t ks, size_t vs) {
    *(const map_impl_t **)&(mp->impl) = impl;
    *(size_t *)&(mp->key_size) = ks;
    *(size_t *)&(mp->val_size) = vs;
}
