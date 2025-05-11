
#pragma once

#include <stddef.h>
#include <stdbool.h>

#include "s_util/err.h"
#include "s_mem/allocator.h"

typedef struct _map_t map_t;
typedef struct _map_impl_t map_impl_t;

struct _map_impl_t {
    void (*delete_map)(map_t *mp);

    void *(*mp_get)(map_t *mp, const void *key);
    fernos_error_t (*mp_put)(map_t *mp, const void *key, const void *value);

    bool (*mp_remove)(map_t *mp, const void *key);

    size_t (*mp_len)(map_t *mp);

    void (*mp_reset_iter)(map_t *mp);

    fernos_error_t (*mp_get_iter)(map_t *mp, const void **key, void **value);
    fernos_error_t (*mp_next_iter)(map_t *mp, const void **key, void **value);
};

struct _map_t {
    const map_impl_t * const impl;

    /**
     * Size of a single key.
     */
    const size_t key_size;

    /**
     * Size of a single value.
     */
    const size_t val_size;
};

/**
 * Map super initializer.
 */
void init_map(map_t *mp, const map_impl_t *impl, size_t ks, size_t vs);

static inline size_t mp_get_key_size(map_t *mp) {
    return mp->key_size;
}

static inline size_t mp_get_val_size(map_t *mp) {
    return mp->val_size;
}

/**
 * Cleanup all the maps resources, does nothing if the given map pointer is NULL.
 */
static inline void delete_map(map_t *mp) {
    if (mp) {
        mp->impl->delete_map(mp);
    }
}

/**
 * Get a pointer to a key's corresponding value.
 *
 * Returns NULL if the given key is not present in the map, or if key is NULL.
 */
static inline void *mp_get(map_t *mp, const void *key) {
    return mp->impl->mp_get(mp, key);
}

/**
 * Place a key value pair in the map. (value can be NULL)
 *
 * If the key is already present, this should overwrite the original value!
 *
 * Returns an error if key is NULL, or if there are insufficient resources.
 */
static inline fernos_error_t mp_put(map_t *mp, const void *key, const void *value) {
    return mp->impl->mp_put(mp, key, value);
}

/**
 * Remove a key value pair from the map.
 *
 * Returns true if a value was actually removed.
 *
 * If key is NULL, false is always returned.
 *
 * NOTE: This doesn't return an error since this is a destructor-like call, removing should always
 * "succeed".
 */
static inline bool mp_remove(map_t *mp, const void *key) {
    return mp->impl->mp_remove(mp, key);
}

/**
 * Return the number of key value pairs in the map.
 */
static inline size_t mp_len(map_t *mp) {
    return mp->impl->mp_len(mp);
}

/*
 * The map iterator must loop over every key value pair in a map once and only once.
 * The order of iteration is arbitrary.
 *
 * Finally, these calls assume the map is not mutated at all during iteration!
 */

/**
 * Reset the iterator.
 */
static inline void mp_reset_iter(map_t *mp) {
    mp->impl->mp_reset_iter(mp);
}

/**
 * Get the current value of the iterator.
 *
 * The address of the key is written to *key.
 * The address of the value is written to *value.
 * (Either/both pointers are allowed to be NULL)
 *
 * Returns FOS_EMPTY if we've reached the end of iteration.
 */
static inline fernos_error_t mp_get_iter(map_t *mp, const void **key, void **value) {
    return mp->impl->mp_get_iter(mp, key, value);
}

/**
 * Advance the iterator, and output its value to key/value following the same rules
 * as `mp_get_iter`.
 */
static inline fernos_error_t mp_next_iter(map_t *mp, const void **key, void **value) {
    return mp->impl->mp_next_iter(mp, key, value);
}

typedef struct _chm_node_header_t chm_node_header_t;
typedef struct _chained_hash_map_t chained_hash_map_t;

typedef bool (*chm_key_eq_ft)(chained_hash_map_t *chm, const void *k0, const void *k1); 
typedef uint32_t (*chm_key_hash_ft)(chained_hash_map_t *chm, const void *k);

struct _chm_node_header_t {

    /*
     * Chains form non-cyclic doubly linked lists.
     */

    chm_node_header_t *next;
    chm_node_header_t *prev;

    /**
     * Hash of this nodes key.
     */
    uint32_t hash;
};

struct _chained_hash_map_t {
    map_t super;

    allocator_t * const al;

    /**
     * The hashmap resized when len > (1 - (1 / ratio_index)) * Capacity.
     *
     * Ratio index must be at least 2.
     *
     * Resizing will be a simple double for now.
     */
    const uint8_t ratio_index;

    /**
     * Hash and equals functions.
     */
    const chm_key_eq_ft key_eq;
    const chm_key_hash_ft key_hash;

    /**
     * Number of entries in the table.
     */
    size_t cap;

    /**
     * Number of entries in the hash map.
     */
    size_t len;

    chm_node_header_t **table;

    chm_node_header_t *iter;
};

/**
 * Returns NULL if arguments are bad or if there are insufficient resources!
 */
map_t *new_chained_hash_map(allocator_t *al, size_t key_size, size_t val_size, uint8_t rat_ind, 
        chm_key_eq_ft k_eq, chm_key_hash_ft k_hash);


