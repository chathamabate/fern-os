

#include "s_mem/simple_heap.h"
#include "s_mem/allocator.h"
#include "s_util/err.h"
#include "s_util/misc.h"
#include "s_util/str.h"

static void *shal_malloc(allocator_t *al, size_t bytes);
static void *shal_realloc(allocator_t *al, void *ptr, size_t bytes);
static void shal_free(allocator_t *al, void *ptr);
static size_t shal_num_user_blocks(allocator_t *al);
static void shal_dump(allocator_t *al, void (*pf)(const char *fmt, ...));
static void delete_simple_heap_allocator(allocator_t *al);

static const allocator_impl_t SHAL_ALLOCATOR_IMPL = {
    .al_malloc = shal_malloc,
    .al_realloc = shal_realloc,
    .al_free = shal_free,
    .al_num_user_blocks = shal_num_user_blocks,
    .al_dump = shal_dump,
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
     * This means we cannot request any more memory.
     * The mapped heap is at its maximum size.
     * This is either because we reached attrs.end, we ran out of underlying memory, or we ran into
     * another area of mapped memory.
     */
    bool exhausted;

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
    const void *true_e;
    err = attrs.request_mem(attrs.start, ((const uint8_t *)(attrs.start)) + M_4K, &true_e);

    if (err != FOS_SUCCESS) {
        return NULL;
    }

    simple_heap_allocator_t *shal = (simple_heap_allocator_t *)(attrs.start);

    *(const allocator_impl_t **)&(shal->super.impl) = &SHAL_ALLOCATOR_IMPL;
    *(simple_heap_attrs_t *)&(shal->attrs) = attrs;

    // All blocks MUST have an even size.
    // Make sure the heap starts at an even offset.
    uint8_t *hs = (uint8_t *)(shal + 1);
    if ((size_t)hs & 1) {
        hs++; 
    }

    *(void **)&(shal->heap_start) = hs;

    shal->brk_ptr = true_e; // This will be the end of whatever our first mem request was.
    shal->exhausted = false;
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
 * Takes a pointer to a block and merges it with bordering free blocks.
 * The given block should NOT be a member of either of the free lists.
 *
 * Returns a pointer to the newly created free block. (Yet to be added to a free list)
 */
static mem_block_t *shal_coalesce(simple_heap_allocator_t *shal, mem_block_t *mb) {
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

    return (mem_block_t *)fb;
}

/**
 * Takes a pointer to a free block which doesn't belong to either of the free lists, and adds
 * it to the correct free list.
 */
static void shal_add_fb(simple_heap_allocator_t *shal, mem_block_t *mb) {
    size_t fb_size = mb_get_size(mb);

    free_block_t *fb = (free_block_t *)mb;
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

/**
 * Use the internal request memory function to advance the break pointer.
 *
 * new_brk MUST be 4K aligned and greater than or equal to the current break.
 * new_brk also must be less than or equal to the end pointer.
 * Returns a pointer to the free block which is created by this request.
 *
 * (The free block returned will not be added to a free list, that's your responsibility)
 *
 * NULL is returned if no mem could be retrieved.
 *
 * NOTE: THIS COALESCES! (i.e. it may modify either of the free lists!)
 */
static mem_block_t *shal_request_mem(simple_heap_allocator_t *shal, const void *new_brk) {
    if (shal->exhausted) {
        return NULL;
    }

    fernos_error_t err;

    void *s = (void *)(shal->brk_ptr);
    const void *true_e;

    err = shal->attrs.request_mem(s, new_brk, &true_e);

    mem_block_t *new_fb = NULL;

    if (s < true_e) {
        shal->brk_ptr = true_e; // advance break pointer.

        size_t new_b_size = (uint32_t)true_e - (uint32_t)s - (2 * sizeof(mem_block_border_t));
        mem_block_border_t *hdr = (mem_block_border_t *)s;

        mbb_set(hdr, new_b_size, false);
        mem_block_t *new_b = header_get_mb(hdr);
        mem_block_border_t *ftr = mb_get_footer(new_b);
        mbb_set(ftr, new_b_size, false);

        // Always remember to coalesce.
        new_fb = shal_coalesce(shal, new_b);
    }

    if (err != FOS_SUCCESS) {
        shal->exhausted = true;
    }

    return new_fb;
}

/**
 * Search either free list for a block of at least bytes size.
 * 
 * When large_list is true, the large list is searched.
 * If stop early is true, the search amount will be used, otherwise the whole list will be searched.
 *
 * The returned free block will be removed from its free list before being returned!
 */
static mem_block_t *shal_search_free_list(simple_heap_allocator_t *shal, 
        bool large_list, bool stop_early, size_t bytes, size_t *searched) {
    size_t stop_amt = 0;
    if (stop_early) {
        stop_amt = large_list ? shal->attrs.large_fl_search_amt : shal->attrs.small_fl_search_amt;
    }

    free_block_t *iter = large_list ? shal->large_fl_head : shal->small_fl_head;
    mem_block_t *fb = NULL;

    size_t i = 0;
    while (iter && (stop_amt == 0 || i < stop_amt)) {
        i++;

        // Is our free block large enough?
        if (mb_get_size((mem_block_t *)iter) >= bytes) {
            fb = (mem_block_t *)iter;
            shal_remove_fb(shal, fb);

            break;
        }
        
        iter = iter->next;
    }

    if (searched) {
        *searched = i;
    }

    return fb;
}

/**
 * Take a block which does not belong to a free list.
 *
 * Shrink the block to bytes size if possible.
 * Any free block created to the right will be coalesced and added to the free list.
 *
 * mb will be marked allocated.
 */
static void shal_shrink_left(simple_heap_allocator_t *shal, 
        mem_block_t *mb, size_t bytes) {
    // Since this is used internally, we assume mb is valid, and the given size is valid.

    size_t mb_size = mb_get_size(mb);
    
    if (bytes >= mb_size) {
        return; // Basically a bad input case, nothing is done.
    }

    size_t left_over = mb_size - bytes;

    // We only can shrink mb, if the left over bytes is large enough for a fresh free block.
    if (left_over >= (2 * sizeof(mem_block_border_t)) + sizeof(free_block_t)) {
        mem_block_border_t *shr_mb_hdr = mb_get_header(mb);
        mbb_set(shr_mb_hdr, bytes, true);
        mem_block_border_t *shr_mb_ftr = mb_get_footer(mb);
        mbb_set(shr_mb_ftr, bytes, true);

        mem_block_t *new_fb = (mem_block_t *)(shr_mb_ftr + 2);
        size_t new_fb_size = left_over - (2 * sizeof(mem_block_border_t));

        mem_block_border_t *new_fb_hdr = mb_get_header(new_fb);
        mbb_set(new_fb_hdr, new_fb_size, false);
        mem_block_border_t *new_fb_ftr = mb_get_footer(new_fb);
        mbb_set(new_fb_ftr, new_fb_size, false);
        
        new_fb = shal_coalesce(shal, new_fb);
        shal_add_fb(shal, new_fb);
    } else {
        // If no shrink, just set the current block as allocated.
        mem_block_border_t *hdr = mb_get_header(mb);
        mbb_set_allocated(hdr, true);
        mem_block_border_t *ftr = mb_get_footer(mb);
        mbb_set_allocated(ftr, true);
    }
}


static void *shal_malloc(allocator_t *al, size_t bytes) {
    simple_heap_allocator_t *shal = (simple_heap_allocator_t *)al;

    if (bytes == 0) {
        return NULL;
    }

    bytes = validate_mb_size(bytes);

    // We need to look for a free block which is at least bytes size.

    mem_block_t *fb = NULL;

    size_t small_fl_searched = 0;
    size_t large_fl_searched = 0;

    // Only search small list if we are requesting a small number of bytes.
    if (/* !fb && */ bytes < shal->attrs.small_fl_cutoff) {
        fb = shal_search_free_list(shal, false, true, bytes, &small_fl_searched);
    }

    // If we are yet to find a free block, let's search the large list.
    if (!fb) {
        fb = shal_search_free_list(shal, true, true, bytes, &large_fl_searched);
    }

    // After doing an initial search of both lists, are we still yet to find a large enough free 
    // block? If so, check if we can request more memory.
    if (!fb && !(shal->exhausted)) {
        size_t mem_needed = bytes;
        if (!IS_ALIGNED(mem_needed, M_4K)) {
            mem_needed = ALIGN(mem_needed + M_4K, M_4K);
        }

        size_t mem_left = (uint32_t)(shal->attrs.end) - (uint32_t)(shal->brk_ptr);
        if (mem_left < mem_needed) {
            mem_needed = mem_left; // allocate as much as we can.
        }

        // Remember, this potentially mutates the free lists.
        mem_block_t *new_fb = shal_request_mem(shal, (uint8_t *)(shal->brk_ptr) + mem_needed);

        if (new_fb) {
            if (mb_get_size(new_fb) >= bytes) {
                fb = new_fb; 
            } else {
                // If not large enough, just add it back to the free list.
                shal_add_fb(shal, new_fb);
            }
        }
    }

    // Have we still not found a match? Let's exhaust our small free list.
    // What the equals case does is confirm that there are possible blocks in the free list we didn't
    // search. If we didn't make it to the search_amt, we know the list's length is less than the 
    // search amt. So, we shouldn't try again. (We already looked at everything)
    //
    // The slight inefficiency here is that we will search over the inital free blocks again.
    if (!fb && bytes < shal->attrs.small_fl_cutoff && 
            small_fl_searched == shal->attrs.small_fl_search_amt) {
        fb = shal_search_free_list(shal, false, false, bytes, NULL);
    }

    // Still no match? Exhaust the large free list.
    if (!fb && large_fl_searched == shal->attrs.large_fl_search_amt) {
        fb = shal_search_free_list(shal, true, false, bytes, NULL);
    }

    // Ok, at this point, fb is either a pointer to a free block which does not belong to a free 
    // list. Or, fb is NULL. If fb is NULL, the allocation failed. If fb is non-null, we know it
    // is large enough for our boy. The question becomes, will this malloc require the full free
    // block, or just a small piece.
    
    if (fb) {
        // We take our free block and shrink it to the correct size. 
        // (Remember, this call also marks the free block as allocated!)
        shal_shrink_left(shal, fb, bytes);
        shal->num_user_blocks++;
    }

    return fb;
}

static void *shal_realloc(allocator_t *al, void *ptr, size_t bytes) {
    if (!ptr) {
        return shal_malloc(al, bytes);
    }

    simple_heap_allocator_t *shal = (simple_heap_allocator_t *)al;

    if (ptr < shal->heap_start || shal->brk_ptr <= ptr) {
        return NULL; // Can't realloc a pointer which isn't in the heap!
    }

    mem_block_t *mb = (mem_block_t *)ptr;
    
    if (!mb_get_allocated(mb)) {
        return NULL;  // Can't realloc a free block.
    }

    // Ok now for real logic.

    bytes = validate_mb_size(bytes);

    size_t mb_size = mb_get_size(mb);

    if (mb_size == bytes) {
        return mb; // No realloc needed.
    }

    if (bytes < mb_size) { // A shrink.
        shal_shrink_left(shal, mb, bytes);
        return mb;
    }

    // If we make it here, bytes > mb_size, a stretch is being requested.

    size_t addn_req = bytes - mb_size;
    
    mem_block_t *next = shal_mb_next(shal, mb);

    // How many bytes are gained by merging with free block to the right.
    size_t addn_amt = 0;
    if (next && !(mb_get_allocated(next))) {
        addn_amt = mb_get_size(next) + (2 * sizeof(mem_block_border_t));
    }

    if (addn_amt < addn_req) {
        // In here, the right block is either allocated, or not large enouugh for the merge.
        // We will need to do the labor intensive strat.
        
        mem_block_t *new_mb = shal_malloc(al, bytes);
        if (!new_mb) {
            return NULL; // Failed allocation.
        }

        mem_cpy(new_mb, mb, mb_size); // remember, we only copy original bytes.
        shal_free(al, mb);

        return new_mb;
    }

    // If we make it here, addn_amt >= addn_req, a merge is possible!

    // First remove next from the free lists.
    shal_remove_fb(shal, next);

    // This is kinda confuisng, but we need to determine a valid size to "shrink" the right block
    // to before merging it.
    
    size_t next_shr_amt = 0;
    if (addn_req > 2 * sizeof(mem_block_border_t)) {
        next_shr_amt = validate_mb_size(addn_req - (2 * sizeof(mem_block_border_t)));
    } else {
        next_shr_amt = validate_mb_size(0);
    }

    shal_shrink_left(shal, next, next_shr_amt);
    size_t next_shr_size = mb_get_size(next);
    size_t stretch_size = mb_size + (2 * sizeof(mem_block_border_t)) + next_shr_size;
    
    mem_block_border_t *stretch_hdr = mb_get_header(mb);
    mem_block_border_t *stretch_ftr = mb_get_footer(next);

    mbb_set(stretch_hdr, stretch_size, true);
    mbb_set(stretch_ftr, stretch_size, true);

    return mb;
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

    mem_block_t *new_fb = shal_coalesce(shal, mb);
    shal_add_fb(shal, new_fb);

    shal->num_user_blocks--;
}

static size_t shal_num_user_blocks(allocator_t *al) {
    simple_heap_allocator_t *shal = (simple_heap_allocator_t *)al;
    return shal->num_user_blocks;
}

static void shal_dump(allocator_t *al, void (*pf)(const char *fmt, ...)) {
    simple_heap_allocator_t *shal = (simple_heap_allocator_t *)al;

    pf("Simple Heap Allocator @ 0x%X\n", shal);
    pf("Pr: 0x%X, Hp: 0x%X, Brk: 0x%X, End: 0x%X\n", 
            shal->attrs.start, shal->heap_start, shal->brk_ptr, shal->attrs.end);
    pf("SFL Head: 0x%X, LFL Head, 0x%X\n", shal->small_fl_head, shal->large_fl_head);

    pf("--- Heap ---\n");

    mem_block_border_t *hdr = (mem_block_border_t *)(shal->brk_ptr);
    
}

static void delete_simple_heap_allocator(allocator_t *al) {
    simple_heap_allocator_t *shal = (simple_heap_allocator_t *)al;

    // Remember, we only return up to the break pointer.
    shal->attrs.return_mem(shal->attrs.start, shal->brk_ptr);
}
