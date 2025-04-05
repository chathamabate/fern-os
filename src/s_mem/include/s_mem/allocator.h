
#pragma once

#include <stdint.h>
#include <stddef.h>

/*
 * An allocator is something used to request dynamic memory.
 *
 * How said memory is requested, and where it lives is up to the implementation.
 */

struct _allocator_t;
typedef struct _allocator_t allocator_t;

/**
 * There will be a default allocator.
 *
 * How the default allocator is used is up to you.
 *
 * It's really just a global field you can get/set.
 */

/**
 * NOTE: This setter is NOT threadsafe. Nor would it really make sense to use this in a multi 
 * threaded context. It's meant to be used maybe at the beginning of some task.
 */
void set_default_allocator(allocator_t *al);
allocator_t *get_default_allocator(void);

typedef struct _allocator_impl_t {
    void *(*al_malloc)(allocator_t *al, size_t bytes);
    void *(*al_realloc)(allocator_t *al, void *ptr, size_t bytes);
    void (*al_free)(allocator_t *al, void *ptr);

    size_t (*al_num_user_blocks)(allocator_t *al);

    void (*delete_allocator)(allocator_t *al);
} allocator_impl_t;

struct _allocator_t {
    /**
     * An allocator's implementation is never owned by the allocator itself. The idea is that when
     * creating an allocator you define some static constant table of its functions which never
     * changes and is pointed to by all instances of the allocator.
     */
    const allocator_impl_t * const impl;
};

/**
 * Allocate a block of memory of size at least bytes.
 *
 * If bytes is 0, this function should always return NULL.
 * If the allocation fails, this should return NULL.
 *
 * On success, a pointer to the new block is returned.
 */
static inline void *al_malloc(allocator_t *al, size_t bytes) {
    if (!al) {
        return NULL;
    }
        
    return al->impl->al_malloc(al, bytes);
}

/**
 * Reallocate a block of memory to size at least bytes.
 *
 * If bytes is 0, the block should be free'd. (NULL is returned)
 * If ptr is NULL, this should behave like al_malloc.
 *
 * If bytes is greater than 0, NULL is returned on error.
 * On success, a pointer to the resized block is returned.
 *
 * NOTE: The pointer returned is not required to be a different pointer than what is given.
 * (i.e. it is ok for the given block to be resized in place if possible)
 */
static inline void *al_realloc(allocator_t *al, void *ptr, size_t bytes) {
    if (!al) {
        return NULL;
    }

    return al->impl->al_realloc(al, ptr, bytes);
}

/**
 * Deallocate a block of memory which was previously allocated with al_malloc or al_realloc.
 * 
 * If ptr is NULL, this should do nothing.
 */
static inline void al_free(allocator_t *al, void *ptr) {
    if (!al) {
        return;
    }

    al->impl->al_free(al, ptr);
}

/**
 * This function primarily exists for debugging purposes.
 *
 * It should return the number of blocks malloc'd by the user which currently exist.
 * I specific "malloc'd" by the user, because blocks which are created internally for the use of the
 * allocator should be ignored.
 */
static inline size_t al_num_user_blocks(allocator_t *al) {
    if (!al) {
        return 0;
    }

    return al->impl->al_num_user_blocks(al);
}

/**
 * Delete the allocator.
 */
static inline void delete_allocator(allocator_t *al) {
    if (!al) {
        return;
    }

    al->impl->delete_allocator(al);
}
