

#include "s_util/str.h"
#include "s_mem/test/allocator.h"
#include "s_mem/allocator.h"
#include "k_startup/vga_cd.h"


static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

/**
 * The allocator generator used by each test.
 */
static allocator_t *(*gen_allocator)(void) = NULL;
static allocator_t *al = NULL;

static bool pretest(void) {
    al = gen_allocator();

    TEST_TRUE(al != NULL);
    TEST_EQUAL_UINT(0, al_num_user_blocks(al));

    TEST_SUCCEED();
}

static bool posttest(void) {
    TEST_EQUAL_UINT(0, al_num_user_blocks(al));
    delete_allocator(al);
    al = NULL;

    TEST_SUCCEED();
}

static bool test_nop_args(void) {
    TEST_TRUE(NULL == al_malloc(al, 0));
    al_free(al, NULL);
    TEST_TRUE(NULL == al_realloc(al, NULL, 0));

    TEST_SUCCEED();
}

static bool test_simple_malloc(void) {
    uint32_t *block = al_malloc(al, sizeof(uint32_t));

    *block = 100;
    TEST_EQUAL_UINT(100, *block);

    al_free(al, block);

    TEST_SUCCEED();
}

static bool test_simple_malloc_and_free(void) {
    void *blocks[4];

    blocks[0] = al_malloc(al, 0x10);
    blocks[1] = al_malloc(al, 0x20);
    blocks[2] = al_malloc(al, 0x10);
    blocks[3] = al_malloc(al, 0x20);

    al_free(al, blocks[0]);
    al_free(al, blocks[2]);

    blocks[0] = al_malloc(al, 0x10);

    al_free(al, blocks[0]);
    al_free(al, blocks[1]);
    al_free(al, blocks[3]);

    TEST_SUCCEED();
}

static bool test_simple_realloc(void) {
    void *blocks[4];

    blocks[0] = al_malloc(al, 30);
    blocks[1] = al_malloc(al, 30);
    blocks[2] = al_malloc(al, 30);
    blocks[3] = al_malloc(al, 30); 

    blocks[0] = al_realloc(al, blocks[0], 45);
    blocks[1] = al_realloc(al, blocks[1], 15);
    blocks[2] = al_realloc(al, blocks[2], 45);
    blocks[3] = al_realloc(al, blocks[3], 29);

    al_free(al, blocks[0]);
    al_free(al, blocks[1]);
    al_free(al, blocks[2]);
    al_free(al, blocks[3]);

    TEST_SUCCEED();
}

static bool test_failed_realloc(void) {
    void *block = al_malloc(al, 0x100);
    TEST_TRUE(block != NULL);

    void *reblock = al_realloc(al, block, M_256M);
    TEST_TRUE(reblock == NULL);

    // Should be able to see here that our block is still allocated
    // and the original size.
    // al_dump(al, term_put_fmt_s);

    al_free(al, block);

    TEST_SUCCEED();
}

static bool test_repeated_malloc0(void) {
    // Malloc a bunch, then free.
    
    uint32_t *blocks[20];
    uint32_t num_blocks = sizeof(blocks) / sizeof(blocks[0]);

    for (uint32_t i = 0; i < num_blocks; i++) {
        uint32_t block_eles = (i + 1);
        uint32_t *block = al_malloc(al, block_eles * sizeof(uint32_t));
        TEST_TRUE(block != NULL);

        for (uint32_t j = 0; j < block_eles; j++) {
            block[j] = i;
        }

        blocks[i] = block;
    }

    TEST_EQUAL_UINT(num_blocks, al_num_user_blocks(al));

    for (uint32_t i = 0; i < num_blocks; i++) {
        uint32_t *block = blocks[i];
        uint32_t block_eles = (i + 1);

        for (uint32_t j = 0; j < block_eles; j++) {
            TEST_EQUAL_UINT(i, block[j]);
        }

        al_free(al, block);
    }

    TEST_SUCCEED();
}

static bool test_repeated_malloc1(void) {
    // malloc a bunch then free then malloc.

    uint8_t *blocks[30];
    uint32_t num_blocks = sizeof(blocks) / sizeof(blocks[0]);

    for (uint32_t i = 0; i < num_blocks; i++) {
        uint32_t block_eles = ((i % 3) + 1) * 4;
        uint8_t *block = al_malloc(al, block_eles * sizeof(uint8_t));
        TEST_TRUE(block != NULL);

        mem_set(block, i, block_eles);

        blocks[i] = block;
    }

    // Free every other block.
    for (uint32_t i = 0; i < num_blocks; i += 2) {
        al_free(al, blocks[i]);
        blocks[i] = NULL;
    }

    TEST_EQUAL_UINT(num_blocks / 2, al_num_user_blocks(al));

    // Alloc every other block again, but with different sizes!
    for (uint32_t i = 0; i < num_blocks; i += 2) {
        uint32_t block_eles = ((i % 4) + 2) * 3;
        uint8_t *block = al_malloc(al, block_eles * sizeof(uint8_t));
        TEST_TRUE(block != NULL);

        mem_set(block, i, block_eles);

        blocks[i] = block;
    }

    // Test internal values.
    for (uint32_t i = 0; i < num_blocks; i++) {
        uint32_t block_eles = i % 2 == 0 ? ((i % 4) + 2) * 3 : ((i % 3) + 1) * 4;
        uint8_t *block = blocks[i]; 

        TEST_TRUE(mem_chk(block, i, block_eles));
    }

    // Finally free everything.
    for (uint32_t i = 0; i < num_blocks; i++) {
        al_free(al, blocks[i]);
    }

    TEST_SUCCEED();
}

static bool test_large_malloc(void) {
    // Some bigger malloc's
    // Not really testing internal values here, just seeing what happens when
    // we require a lot of space.

    uint8_t *blocks[15];
    uint32_t num_blocks = sizeof(blocks) / sizeof(blocks[0]);

    for (uint32_t i = 0; i < num_blocks; i++) {
        size_t block_size = (((i % 5) + 1) * M_4K + 13);
        uint8_t *block = al_malloc(al, block_size);
        TEST_TRUE(block != NULL);

        blocks[i] = block;
    }

    for (uint32_t i = 1; i < num_blocks; i += 3) {
        al_free(al, blocks[i]);
        blocks[i] = NULL;
    }


    for (uint32_t i = 1; i < num_blocks; i += 3) {
        size_t block_size = 3 * M_4K;
        uint8_t *block = al_malloc(al, block_size);

        TEST_TRUE(block != NULL);

        blocks[i] = block;
    }

    for (uint32_t i = 0; i < num_blocks; i++) {
        mem_block_t *mb = blocks[i];
        al_free(al, mb);
    }
    
    TEST_SUCCEED();
}

bool test_realloc(void) {
    uint8_t *blocks[30];
    uint32_t num_blocks = sizeof(blocks) / sizeof(blocks[0]);

    for (uint32_t i = 0; i < num_blocks; i++) {
        uint32_t block_eles = 30;
        uint8_t *block = al_realloc(al, NULL, block_eles * sizeof(uint8_t));
        TEST_TRUE(block != NULL);
    
        for (uint32_t j = 0; j < block_eles; j++) {
            block[j] = i;
        }

        blocks[i] = block;
    }

    for (uint32_t i = 0; i < num_blocks; i++) {
        uint32_t block_eles = i % 2 == 0 ? 45 : 15; 
        uint8_t *realloc_block = al_realloc(al, blocks[i], block_eles * sizeof(uint8_t));

        TEST_TRUE(realloc_block != NULL);

        // For any block which was resized as larger than the original 30 elements,
        // Fill in the blank space.
        for (uint32_t j = 30; j < block_eles; j++) {
            realloc_block[j] = i;
        }

        blocks[i] = realloc_block;
    }

    TEST_EQUAL_UINT(num_blocks, al_num_user_blocks(al));

    for (uint32_t i = 0; i < num_blocks; i++) {
        // Only check elements which would've been copied.
        uint32_t block_eles = i % 2 == 0 ? 45 : 15;
        uint8_t *block = blocks[i];
        
        for (uint32_t j = 0; j < block_eles; j++) {
            TEST_EQUAL_UINT(i, block[j]);
        }
    }

    // Finally free everything using realloc once again.
    for (uint32_t i = 0; i < num_blocks; i++) {
        TEST_TRUE(NULL == al_realloc(al, blocks[i], 0));
    }

    TEST_SUCCEED();
}

static bool test_complex_actions(void) {
    uint8_t *blocks[100];
    size_t sizes[sizeof(blocks) / sizeof(blocks[0])];
    uint32_t num_blocks = sizeof(blocks) / sizeof(blocks[0]);

    for (uint32_t i = 0; i < num_blocks / 2; i++) {
        sizes[i] = 0x10 * (i + 1);
        blocks[i] = al_malloc(al, sizes[i]);
        TEST_TRUE(blocks[i] != NULL);

        mem_set(blocks[i], i, sizes[i]);
    }

    for (uint32_t i = 0; i < num_blocks / 2; i += 2) {
        al_free(al, blocks[i]);
        blocks[i] = NULL;
    }

    for (uint32_t i = num_blocks / 2; i < num_blocks; i++) {
        sizes[i] = 300 * i;
        blocks[i] = al_malloc(al, sizes[i]);
        TEST_TRUE(blocks[i] != NULL);

        mem_set(blocks[i], i, sizes[i]);

    }

    for (uint32_t i = num_blocks / 2; i < num_blocks; i += 2) {
        al_free(al, blocks[i]);
        blocks[i] = NULL;
    }

    for (uint32_t i = 0; i < num_blocks / 2; i += 2) {
        sizes[i] = 5000;
        blocks[i] = al_malloc(al, sizes[i]);
        TEST_TRUE(blocks[i] != NULL);

        mem_set(blocks[i], i, sizes[i]);
    }

    for (uint32_t i = num_blocks / 2; i < num_blocks; i += 2) {
        sizes[i] = 300;
        blocks[i] = al_malloc(al, sizes[i]);
        TEST_TRUE(blocks[i] != NULL);

        mem_set(blocks[i], i, sizes[i]);
    }

    for (uint32_t i = 0; i < num_blocks; i++) {
        size_t old_size = sizes[i];
        size_t new_size = i % 3 == 0 ? old_size + (100 * i) : old_size / 2;

        blocks[i] = al_realloc(al, blocks[i], new_size);
        TEST_TRUE(blocks[i] != NULL);

        for (size_t j = old_size; j < new_size; j++) {
            blocks[i][j] = (uint8_t)i;
        }

        sizes[i] = new_size;
    }

    for (uint32_t i = 0; i < num_blocks; i++) {
        TEST_TRUE(mem_chk(blocks[i], i, sizes[i]));
    }

    for (uint32_t i = 0; i < num_blocks; i++) {
        al_free(al, blocks[i]);
        blocks[i] = NULL;
    }

    TEST_SUCCEED();
}

static bool test_exhaust(void) {
    // Here we just allocate a lot of memory... more than this allocator should have.

    uint8_t *blocks[256];
    uint32_t num_blocks = sizeof(blocks) / sizeof(blocks[0]);

    for (uint32_t i = 0; i < num_blocks; i++) {
        blocks[i] = al_malloc(al, M_1M);
    }

    TEST_TRUE(blocks[num_blocks - 1] == NULL);

    for (uint32_t i = 0; i < num_blocks; i++) {
        al_free(al, blocks[i]);
    }

    // Here we confirm that even after using up a lot of memory than
    // freeing it, we can still do some complex stuff.
    ENTER_SUBTEST(test_complex_actions);

    TEST_SUCCEED();
}

bool test_allocator(const char *name, allocator_t *(*gen)(void)) {
    gen_allocator = gen;

    BEGIN_SUITE(name);
    RUN_TEST(test_nop_args);
    RUN_TEST(test_simple_malloc);
    RUN_TEST(test_simple_malloc_and_free);
    RUN_TEST(test_simple_realloc);
    RUN_TEST(test_failed_realloc);
    RUN_TEST(test_repeated_malloc0);
    RUN_TEST(test_repeated_malloc1);
    RUN_TEST(test_large_malloc);
    RUN_TEST(test_realloc);
    RUN_TEST(test_complex_actions);
    RUN_TEST(test_exhaust);
    return END_SUITE();
}
