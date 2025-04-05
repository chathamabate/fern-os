

#include "s_mem/test/allocator.h"
#include "s_mem/allocator.h"
#include "k_bios_term/term.h"


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

static bool test_simple_malloc(void) {
    uint32_t *block = al_malloc(al, sizeof(uint32_t));

    *block = 100;
    TEST_EQUAL_UINT(100, *block);

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

        for (uint32_t j = 0; j < block_eles; j++) {
            block[j] = i;
        }

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

        for (uint32_t j = 0; j < block_eles; j++) {
            block[j] = i; 
        }

        blocks[i] = block;
    }

    // Test internal values.
    for (uint32_t i = 0; i < num_blocks; i++) {
        uint32_t block_eles = i % 2 == 0 ? ((i % 4) + 2) * 3 : ((i % 3) + 1) * 4;
        uint8_t *block = blocks[i]; 

        for (uint32_t j = 0; j < block_eles; j++) {
            TEST_EQUAL_UINT(i, block[j]);
        }
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
        uint8_t *block = al_malloc(al, 3 * M_4K);
        blocks[i] = block;
    }

    for (uint32_t i = 0; i < num_blocks; i++) {
        al_free(al, blocks[i]);
    }
    
    TEST_SUCCEED();
}

bool test_realloc(void) {

    TEST_SUCCEED();
}

bool test_allocator(const char *name, allocator_t *(*gen)(void)) {
    gen_allocator = gen;

    BEGIN_SUITE(name);
    RUN_TEST(test_simple_malloc);
    RUN_TEST(test_repeated_malloc0);
    RUN_TEST(test_repeated_malloc1);
    RUN_TEST(test_large_malloc);
    RUN_TEST(test_realloc);
    return END_SUITE();
}
