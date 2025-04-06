

#include "s_mem/simple_heap.h"
#include "s_mem/allocator.h"

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

    /**
     * The start of the heap region.
     */
    void * const heap_start; 

    /**
     * The exclusive end of the allocated area of the heap region.
     */
    const void *brk_ptr;

    /**
     * The exclusive end of the heap region.
     */
    const void * const heap_end;

    /**
     * Number of user allocated blocks in the heap.
     */
    size_t num_user_blocks;

    /**
     * Ok, what is the free list structure??
     */
} simple_heap_allocator_t;

allocator_t *new_simple_heap_allocator(void *s, const void *e, request_mem_ft rqm) {
    return NULL;
}

static void *shal_malloc(allocator_t *al, size_t bytes) {
    return NULL;
}

static void *shal_realloc(allocator_t *al, void *ptr, size_t bytes) {
    return NULL;
}

static void shal_free(allocator_t *al, void *ptr) {
}

static size_t shal_num_user_blocks(allocator_t *al) {
    return 0;
}

static void delete_simple_heap_allocator(allocator_t *al) {
}
