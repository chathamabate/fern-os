
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "s_util/err.h"

/*
 * Page oriented structure useful for allocators.
 */

typedef struct _mem_manage_pair_t {
    /**
     * This is a function which will be used by the an allocator to request pages of memory.
     * 
     * s and e MUST be 4K aligned.
     * true_e MUST be non-null.
     *
     * If success, the entire range was successfully allocated. (true_e is set to e).
     * An error may occur, if there isn't enough memory. OR, if the region requested already overlaps
     * with an allocated region. In both cases the allocation stops, and true_e is set to the end of 
     * the newly allocated region.
     */
    fernos_error_t (*request_mem)(void *s, const void *e, const void **true_e);

    /**
     * This function is used by an allocator to return pages of memory.
     * Both s and e must be 4K aligned.
     */
    void (*return_mem)(void *s, const void *e);
} mem_manage_pair_t;

/*
 * Memory block primitives which may be useful for allocators.
 */

typedef uint32_t mem_block_border_t;

static inline void mbb_set_size(mem_block_border_t *mbb, uint32_t size) {
    *mbb = (size & (~1UL)) | (*mbb & 1UL);
}

static inline uint32_t mbb_get_size(mem_block_border_t mbb) {
    return mbb & (~1UL);
}

static inline void mbb_set_allocated(mem_block_border_t *mbb, bool allocated) {
    *mbb = (*mbb & (~1UL)) | (allocated ? 1UL : 0UL);
}

static inline bool mbb_get_allocated(mem_block_border_t mbb) {
    return (mbb & 1UL) == 1UL;
}

static inline void mbb_set(mem_block_border_t *mbb, uint32_t size, bool allocated) {
    *mbb = (size & (~1UL)) | (allocated ? 1UL : 0UL);
}

/* uint8_t is kinda a place holder here */
typedef uint8_t mem_block_t;

/*
 * NOTE: the "size" of a memory block is just how many internal bytes there are.
 * A memory block occupies (2 * sizeof(border)) + size bytes.
 */

static inline mem_block_border_t *mb_get_header(mem_block_t *mb) {
    return (mem_block_border_t *)(mb - sizeof(mem_block_border_t));
}

static inline mem_block_t *header_get_mb(mem_block_border_t *hdr) {
    return (mem_block_t *)(hdr + 1);
}

static inline mem_block_border_t *mb_get_footer(mem_block_t *mb) {
    uint32_t size = mbb_get_size(*mb_get_header(mb));
    return (mem_block_border_t *)(mb + size);
}

static inline mem_block_t *footer_get_mb(mem_block_border_t *ftr) {
    uint32_t size = mbb_get_size(*ftr);
    return (mem_block_t *)((uint8_t *)ftr - size);
}

static inline uint32_t mb_get_size(mem_block_t *mb) {
    return mbb_get_size(*mb_get_header(mb));
}

static inline bool mb_get_allocated(mem_block_t *mb) {
    return mbb_get_allocated(*mb_get_header(mb));
}

/*
 * Simple Doubly linked free block structure which may be helpful for allocators!
 */

struct _free_block_t;
typedef struct _free_block_t free_block_t;

struct _free_block_t {
    free_block_t *prev;
    free_block_t *next;
};

/*
 * If you are using the schema above, all blocks must have an even size, and no block can
 * have a size smaller than a free block.
 */

static inline size_t validate_mb_size(size_t mb_size) {
    size_t mbs = mb_size;

    if (mbs < sizeof(free_block_t)) {
        mbs = sizeof(free_block_t);
    }

    if (mbs & 1) {
        mbs++;
    }

    return mbs;
}

/**
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
 *
 * The setter returns whichever allocator was previously set as the default.
 */
allocator_t *set_default_allocator(allocator_t *al);
allocator_t *get_default_allocator(void);

typedef struct _allocator_impl_t {
    void *(*al_malloc)(allocator_t *al, size_t bytes);
    void *(*al_realloc)(allocator_t *al, void *ptr, size_t bytes);
    void (*al_free)(allocator_t *al, void *ptr);

    size_t (*al_num_user_blocks)(allocator_t *al);

    // OPTIONAL
    void (*al_dump)(allocator_t *al, void (*pf)(const char *fmt, ...));

    void (*delete_allocator)(allocator_t *al);
} allocator_impl_t;

struct _allocator_t {
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
 * When NULL is returned on error, the original pointer given is left untouched.
 *
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
 * This function also primarily exists for debugging purposes. It is completely optional, and it's
 * behavoir is up to the implementer.
 *
 * The intention is that is uses the pf function to log some sort of text based representation of
 * the underlying memory.
 */
static inline void al_dump(allocator_t *al, void (*pf)(const char *fmt, ...)) {
    if (!al) {
        return;
    }

    if (!(al->impl->al_dump)) {
        return;
    }

    al->impl->al_dump(al, pf);
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
