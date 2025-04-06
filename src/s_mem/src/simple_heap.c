

#include "s_mem/simple_heap.h"
#include "s_mem/allocator.h"
#include "s_util/err.h"
#include "s_util/misc.h"

static void *shal_malloc(allocator_t *al, size_t bytes);
static void *shal_realloc(allocator_t *al, void *ptr, size_t bytes);
static void shal_free(allocator_t *al, void *ptr);
static size_t shal_num_user_blocks(allocator_t *al);
static void delete_simple_heap_allocator(allocator_t *al);

static const allocator_impl_t SHAL_ALLOCATOR_IMPL = {
    .al_malloc = shal_malloc,
    .al_realloc = shal_realloc,
    .al_free = shal_free,
    .al_num_user_blocks = shal_num_user_blocks,
    .delete_allocator = delete_simple_heap_allocator
};

typedef struct _simple_heap_allocator_t {
    allocator_t super;
    const simple_heap_attrs_t attrs;

    /*
     * The heap allocator breaks its area up into two pieces.
     * The prologue and the heap.
     *
     * The prologue contains this struct (i.e. all bookkeeping info).
     * The heap contains the actually allocated/free blocks.
     *
     * NOTE: The brk_ptr will ALWAYS be 4K aligned.
     * Pages between the brk_ptr and attrs.end are expected to be unmapped.
     *
     * attrs.start  heap_start                 attrs.end 
     * |            |          brk_ptr         |
     * V            V          |               V
     *                         V
     * | Prologue   | Mapped   | Unmapped      |
     * |            |                          | 
     * |            # ------ Heap ------------ | 
     * # --------- Allocator Region ---------- # 
     */

    void * const heap_start;

    /**
     * The exclusive end of the allocated area of the heap region.
     */
    const void *brk_ptr;

    /**
     * Number of user allocated blocks in the heap.
     */
    size_t num_user_blocks;

    /**
     * Heads of both free lists, NULL when empty.
     */
    free_block_t *small_fl_head;
    free_block_t *large_fl_head;
} simple_heap_allocator_t;

allocator_t *new_simple_heap_allocator(simple_heap_attrs_t attrs) {
    if (!IS_ALIGNED(attrs.start, M_4K) || !IS_ALIGNED(attrs.end, M_4K)) {
        return NULL;
    }

    // Must be non-empty range.
    if (attrs.end <= attrs.start) {
        return NULL;
    }

    if (!attrs.request_mem || !attrs.return_mem) {
        return NULL;
    }

    fernos_error_t err;

    // Start by reserving a single page.
    void *true_e;
    err = attrs.request_mem(attrs.start, ((const uint8_t *)(attrs.start)) + M_4K, &true_e);

    if (err != FOS_SUCCESS) {
        return NULL;
    }

    simple_heap_allocator_t *shal = (simple_heap_allocator_t *)(attrs.start);

    *(allocator_impl_t *)&(shal->super.impl) = SHAL_ALLOCATOR_IMPL;
    *(simple_heap_attrs_t *)&(shal->attrs) = attrs;

    // All blocks MUST have an even size.
    // Make sure the heap starts at an even offset.
    uint8_t *hs = (uint8_t *)(shal + 1);
    if ((size_t)hs & 1) {
        hs++; 
    }

    *(void **)&(shal->heap_start) = hs;

    shal->brk_ptr = true_e; // This will be the end of whatever our first mem request was.
    shal->num_user_blocks = 0;

    // Ok, now we gotta configure the first free block.

    size_t first_fb_size = (size_t)(shal->brk_ptr) - (size_t)(shal->heap_start) 
        - (2 * sizeof(mem_block_border_t));

    // This if statement should really never execute.
    // This would error out in the case that our simple heap allocator structure is so large
    // that it takes up the entire first page.
    if (first_fb_size < sizeof(free_block_t)) {
        shal->attrs.return_mem(shal->attrs.start, shal->brk_ptr);
        return NULL;
    }

    mem_block_border_t *first_fb_hdr = (mem_block_border_t *)(shal->heap_start);

    mbb_set(first_fb_hdr, first_fb_size, false);
    mem_block_t *first_fb = header_get_mb(first_fb_hdr);
    *(free_block_t *)first_fb = (free_block_t) {
        .next = NULL,
        .prev = NULL
    };
    mbb_set(mb_get_footer(first_fb), first_fb_size, false);

    if (first_fb_size < shal->attrs.small_fl_cutoff) {
        shal->small_fl_head = (free_block_t *)first_fb;
        shal->large_fl_head = NULL;
    } else {
        shal->small_fl_head = NULL;
        shal->large_fl_head = (free_block_t *)first_fb;
    }

    return (allocator_t *)shal;
}

/**
 * Given a pointer to a valid free block, remove it from whatever free list it exists in.
 *
 * NOTE: This assumes we already know mb is a free block. (NO CHECKS)
 */
static void shal_remove_fb(simple_heap_allocator_t *shal, mem_block_t *mb) {
    size_t fb_size = mb_get_size(mb); 

    free_block_t *fb = (free_block_t *)mb;

    if (fb->prev) {
        fb->prev->next = fb->next;
    } else {
        // If there is no previous, we are working with the head of a free list.
        // We must make sure to update the correct free list.

        if (fb_size < shal->attrs.small_fl_cutoff) {
            shal->small_fl_head = fb->next;
        } else {
            shal->large_fl_head = fb->next;
        }
    }

    if (fb->next) {
        fb->next->prev = fb->prev;
    }

    fb->prev = NULL;
    fb->next = NULL;
}

static mem_block_t *shal_mb_next(simple_heap_allocator_t *shal, mem_block_t *mb) {
    mem_block_border_t *footer = mb_get_footer(mb);
    mem_block_t *next = (mem_block_t *)(footer + 2);

    if ((void *)next >= shal->brk_ptr) {
        next = NULL;
    }

    return next;
}

static mem_block_t *shal_mb_prev(simple_heap_allocator_t *shal, mem_block_t *mb) {
    mem_block_t *prev;
    mem_block_border_t *header = mb_get_header(mb);

    if (header == shal->heap_start) {
        prev = NULL;
    } else {
        prev = footer_get_mb(header - 1);
    }

    return prev;
}

/**
 * Take a pointer to a block which is to be added to the free lists.
 *
 * NOTE: mb MUST have a valid size initialized!
 */
static void shal_add_fb(simple_heap_allocator_t *shal, mem_block_t *mb) {
    mem_block_border_t *fb_hdr = mb_get_header(mb);
    mem_block_border_t *fb_ftr = mb_get_footer(mb);

    mem_block_t *prev = shal_mb_prev(shal, mb);
    if (prev && !mb_get_allocated(prev)) {
        shal_remove_fb(shal, prev);
        fb_hdr = mb_get_header(prev);
    }

    mem_block_t *next = shal_mb_next(shal, mb);
    if (next && !mb_get_allocated(next)) {
        shal_remove_fb(shal, next);
        fb_ftr = mb_get_footer(next);
    }

    free_block_t *fb = (free_block_t *)(fb_hdr + 1);
    uint32_t fb_size =  (uint32_t)fb_ftr - (uint32_t)(fb);
    
    mbb_set(fb_hdr, fb_size, false);
    mbb_set(fb_ftr, fb_size, false);
    
    fb->prev = NULL;

    if (fb_size < shal->attrs.small_fl_cutoff) {
        fb->next = shal->small_fl_head;
        if (shal->small_fl_head) {
            shal->small_fl_head->prev = fb;
        }

        shal->small_fl_head = fb;
    } else {
        fb->next = shal->large_fl_head;
        if (shal->large_fl_head) {
            shal->large_fl_head->prev = fb;
        }

        shal->large_fl_head = fb;
    }
}

static void *shal_malloc(allocator_t *al, size_t bytes) {
    simple_heap_allocator_t *shal = (simple_heap_allocator_t *)al;

    if (bytes & 1) {
        bytes++; // Always round bytes up to an even number!
    }

    // We need to look for a free block which is at least bytes size.

    if (bytes < 

    // Ok, so, how do we do this exactly???


    return NULL;
}

static void *shal_realloc(allocator_t *al, void *ptr, size_t bytes) {



    // Realloc is probably easier than alloc tbh!
    return NULL;
}

static void shal_free(allocator_t *al, void *ptr) {
    simple_heap_allocator_t *shal = (simple_heap_allocator_t *)al;

    if (ptr < shal->heap_start || shal->brk_ptr <= ptr) {
        return; // Can't free a pointer which isn't in the heap!
    }

    mem_block_t *mb = (mem_block_t *)ptr;

    if (!mb_get_allocated(mb)) {
        return; // Can't free a block which isn't allocated!
    }

    shal_add_fb(shal, mb);

    shal->num_user_blocks--;
}

static size_t shal_num_user_blocks(allocator_t *al) {
    simple_heap_allocator_t *shal = (simple_heap_allocator_t *)al;
    return shal->num_user_blocks;
}

static void delete_simple_heap_allocator(allocator_t *al) {
}
