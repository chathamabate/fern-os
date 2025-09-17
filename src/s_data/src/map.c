
#include "s_data/map.h"
#include "s_util/str.h"

void init_map(map_t *mp, const map_impl_t *impl, size_t ks, size_t vs) {
    *(const map_impl_t **)&(mp->impl) = impl;
    *(size_t *)&(mp->key_size) = ks;
    *(size_t *)&(mp->val_size) = vs;
}

static void delete_chained_hash_map(map_t *mp);
static fernos_error_t chm_get_kvp(map_t *mp, const void *key, const void **key_out, void **val_out);
static fernos_error_t chm_put(map_t *mp, const void *key, const void *value);
static bool chm_remove(map_t *mp, const void *key);
static size_t chm_len(map_t *mp);
static void chm_reset_iter(map_t *mp);
static fernos_error_t chm_get_iter(map_t *mp, const void **key, void **value);
static fernos_error_t chm_next_iter(map_t *mp, const void **key, void **value);

const map_impl_t CHM_IMPL = {
    .delete_map = delete_chained_hash_map,

    .mp_get_kvp = chm_get_kvp,
    .mp_put = chm_put,
    .mp_remove = chm_remove,
    .mp_len = chm_len,
    .mp_reset_iter = chm_reset_iter,
    .mp_get_iter = chm_get_iter,
    .mp_next_iter = chm_next_iter,
};

map_t *new_chained_hash_map(allocator_t *al, size_t key_size, size_t val_size, uint8_t rat_ind, 
        equator_ft k_eq, hasher_ft k_hash) {
    if (key_size == 0 || !k_eq || !k_hash || rat_ind < 2) {
        return NULL;
    }

    const size_t INIT_CAP = 1;

    chained_hash_map_t *chm = al_malloc(al, sizeof(chained_hash_map_t));
    chm_node_header_t **table = al_malloc(al, sizeof(chm_node_header_t *) * INIT_CAP);

    if (!chm || !table) {
        al_free(al, chm);
        al_free(al, table);

        return NULL;
    }

    init_map((map_t *)chm, &CHM_IMPL, key_size, val_size);
    *(allocator_t **)&(chm->al) = al;

    *(uint8_t *)&(chm->ratio_index) = rat_ind;
    *(equator_ft *)&(chm->key_eq) = k_eq;
    *(hasher_ft *)&(chm->key_hash) = k_hash;

    chm->cap = INIT_CAP;

    chm->len = 0;

    chm->table = table;

    // NULL out table to begin with.
    for (uint32_t i = 0; i < INIT_CAP; i++) {
        chm->table[i] = NULL;
    }

    chm->iter = NULL;

    return (map_t *)chm;
}

static void delete_chained_hash_map(map_t *mp) {
    chained_hash_map_t *chm = (chained_hash_map_t *)mp;

    for (size_t i = 0; i < chm->cap; i++) {
        chm_node_header_t *iter = chm->table[i];

        while (iter) {
            chm_node_header_t *next = iter->next;

            al_free(chm->al, iter);

            iter = next;
        }
    }

    al_free(chm->al, chm->table);
    al_free(chm->al, chm);
}

/**
 * This checks if a resize is needed, if so, it attempts to resize.
 * If there isn't enough memory to resize, that's ok. It just returns.
 */
static void chm_try_resize(chained_hash_map_t *chm) {
    uint32_t bound = (chm->cap * (chm->ratio_index - 1)) / chm->ratio_index;

    if (chm->len <= bound) {
        return;
    }

    uint32_t new_cap = chm->cap * 2;

    if (new_cap <= chm->cap) {
        return;
    }

    // We are using alloc and not realloc because we need to transfer the nodes to the new
    // table easily!
    chm_node_header_t **new_table = al_malloc(chm->al, sizeof(chm_node_header_t *) * new_cap);

    if (!new_table) {
        return;
    }

    for (uint32_t i = 0; i < new_cap; i++) {
        new_table[i] = NULL;
    }

    for (uint32_t i = 0; i < chm->cap; i++) {
        chm_node_header_t *iter = chm->table[i];
        while (iter) {
            chm_node_header_t *next_iter = iter->next;

            uint32_t new_ind = iter->hash % new_cap;
            if (new_table[new_ind]) {
                new_table[new_ind]->prev = iter;
            }

            iter->prev = NULL;
            iter->next = new_table[new_ind];
            new_table[new_ind] = iter;

            iter = next_iter;
        }
    }

    al_free(chm->al, chm->table);

    chm->table = new_table;
    chm->cap = new_cap;
}

/**
 * Return a pointer to a node which matches the given key.
 *
 * Returns NULL if there is no match!
 */
static chm_node_header_t *chm_get_node(chained_hash_map_t *chm, const void *key) {
    uint32_t hash = chm->key_hash(key);
    uint32_t table_ind = hash % chm->cap;

    chm_node_header_t *iter = chm->table[table_ind];

    while (iter) {
        if (iter->hash == hash) {
            // Remember it goes [header] [key] [value] back to back to back.
            const void *iter_key = (const void *)(iter + 1);
            if (chm->key_eq(iter_key, key)) {
                return iter;
            }
        }

        iter = iter->next;
    }

    return NULL;
}

static fernos_error_t chm_get_kvp(map_t *mp, const void *key, const void **key_out, void **val_out) {
    chained_hash_map_t *chm = (chained_hash_map_t *)mp;

    chm_node_header_t *node = chm_get_node(chm, key);

    if (!node) {
        return FOS_E_EMPTY;
    }

    if (key_out) {
        *key_out = (const void *)(node + 1);
    }

    if (val_out) {
        *val_out = (void *)((uint8_t *)(node + 1) + chm->super.key_size);
    }

    return FOS_E_SUCCESS;
}

static fernos_error_t chm_put(map_t *mp, const void *key, const void *value) {
    chained_hash_map_t *chm = (chained_hash_map_t *)mp;

    chm_node_header_t *node = chm_get_node(chm, key);

    if (node) {
        // Easy overwrite!

        const void *key_ptr = node + 1;
        uint8_t *val_ptr = (uint8_t *)key_ptr + chm->super.key_size;
        
        mem_cpy(val_ptr, value, chm->super.val_size);

        return FOS_E_SUCCESS;
    }

    // We must add a new node to the map!

    chm_node_header_t *new_node = al_malloc(chm->al, 
            sizeof(chm_node_header_t) + chm->super.key_size + chm->super.val_size);

    if (!new_node) {
        return FOS_E_NO_MEM;
    }

    // Redundant work here, but whatevs.

    uint32_t hash = chm->key_hash(key);
    const void *new_key_ptr = new_node + 1;
    void *new_val_ptr = (uint8_t *)new_key_ptr + chm->super.key_size;

    new_node->hash = hash;
    mem_cpy((void *)new_key_ptr, key, chm->super.key_size);
    mem_cpy(new_val_ptr, value, chm->super.val_size);

    uint32_t index = hash % chm->cap;

    // Quick push front.

    chm_node_header_t *old_head = chm->table[index];

    new_node->prev = NULL;
    new_node->next = old_head;

    if (old_head) {
        old_head->prev = new_node;
    }

    chm->table[index] = new_node;

    chm->len++;

    chm_try_resize(chm);

    return FOS_E_SUCCESS;
}

static bool chm_remove(map_t *mp, const void *key) {
    chained_hash_map_t *chm = (chained_hash_map_t *)mp;

    if (!key) {
        return false;
    }

    chm_node_header_t *node = chm_get_node(chm, key);

    if (!node) {
        return false;
    }

    if (node->next) {
        node->next->prev = node->prev;
    }

    if (node->prev) {
        node->prev->next = node->next;
    } else {
        chm->table[node->hash % chm->cap] = node->next;
    }

    al_free(chm->al, node);
    chm->len--;

    return true;
}

static size_t chm_len(map_t *mp) {
    chained_hash_map_t *chm = (chained_hash_map_t *)mp;

    return chm->len;
}

static void chm_reset_iter(map_t *mp) {
    chained_hash_map_t *chm = (chained_hash_map_t *)mp;

    chm->iter = NULL;
    
    for (uint32_t i = 0; i < chm->cap; i++) {
        if (chm->table[i]) {
            chm->iter = chm->table[i];
            break;
        }
    }
}

static fernos_error_t chm_get_iter(map_t *mp, const void **key, void **value) {
    chained_hash_map_t *chm = (chained_hash_map_t *)mp;

    fernos_error_t err = FOS_E_EMPTY;

    const void *key_ptr = NULL;
    void *val_ptr = NULL;

    if (chm->iter) {
        key_ptr = chm->iter + 1;
        val_ptr = (uint8_t *)key_ptr + chm->super.key_size;
        
        err = FOS_E_SUCCESS;
    }

    if (key) {
        *key = key_ptr;
    }

    if (value) {
        *value = val_ptr;
    }

    return err;
}

static fernos_error_t chm_next_iter(map_t *mp, const void **key, void **value) {
    chained_hash_map_t *chm = (chained_hash_map_t *)mp;

    // Advance iter, than just call normal get.

    if (chm->iter->next) {
        chm->iter = chm->iter->next;
    } else {
        uint32_t i = (chm->iter->hash % chm->cap) + 1;
        chm->iter = NULL;

        for (; i < chm->cap; i++) {
            if (chm->table[i]) {
                chm->iter = chm->table[i];
                break;
            }
        }
    }

    return chm_get_iter(mp, key, value);
}


