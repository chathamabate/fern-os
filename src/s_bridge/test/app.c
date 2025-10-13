
#include "s_bridge/app.h"

#include "k_startup/vga_cd.h"

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

static size_t _num_al_blocks;

static bool pretest(void) {
    _num_al_blocks = al_num_user_blocks(get_default_allocator());

    TEST_SUCCEED();
}

static bool posttest(void) {
    TEST_EQUAL_HEX(_num_al_blocks, al_num_user_blocks(get_default_allocator()));

    TEST_SUCCEED();
}

/*
 * I technically could construct some sort of mock user app here just to test the destructor,
 * but ehhh. The destructor will be pretty well tested when I test loading a user app from an ELF 
 * file.
 */

/**
 * Test creating an args block with a NONZERO number of args.
 */
static bool test_new_args_block_p(const char * const *args, size_t num_args) {
    const size_t exp_tbl_area_size = (num_args + 1) * sizeof(uint32_t);

    size_t str_area_size = 0;
    for (size_t i = 0; i < num_args; i++) {
        str_area_size += str_len(args[i]) + 1;
    }
    const size_t exp_str_area_size = str_area_size;

    const size_t exp_block_size = exp_tbl_area_size + exp_str_area_size;

    const void *block;
    size_t block_len;
    TEST_SUCCESS(new_da_args_block(args, num_args, &block, &block_len));

    TEST_EQUAL_UINT(exp_block_size, block_len);

    size_t str_area_offset = 0;
    for (size_t i = 0; i < num_args; i++) {
        const size_t exp_offset = exp_tbl_area_size + str_area_offset;
        const char *embedded_str = (char *)block + exp_offset;

        TEST_EQUAL_HEX(exp_offset, ((uint32_t *)block)[i]);
        TEST_TRUE(str_eq(embedded_str, args[i]));

        str_area_offset += str_len(embedded_str) + 1;
    }

    TEST_EQUAL_HEX(0, ((uint32_t *)block)[num_args]);

    da_free((void *)block);

    TEST_SUCCEED();
}

static bool test_new_args_block(void) {
    const char *args0[] = {
        "a1", "arg2", 
    };
    const size_t num_args0 = sizeof(args0) / sizeof(args0[0]);

    TEST_TRUE(test_new_args_block_p(args0, num_args0));

    const char *args1[] = {
        "aasdfasfasffsd"
    };
    const size_t num_args1 = sizeof(args1) / sizeof(args1[0]);

    TEST_TRUE(test_new_args_block_p(args1, num_args1));

    const char *args2[] = {
        "aasfasffsd", "asdf", "aaaa", "dfdf", "a"
    };
    const size_t num_args2 = sizeof(args2) / sizeof(args2[0]);

    TEST_TRUE(test_new_args_block_p(args2, num_args2));

    // Null arguments should work fine.

    const void *block;
    size_t block_len;
    TEST_SUCCESS(new_da_args_block(NULL, 0, &block, &block_len));
    TEST_EQUAL_HEX(NULL, block);
    TEST_EQUAL_UINT(0, block_len);

    TEST_SUCCEED();
}

static bool test_bad_new_args_block(void) {
    const void *block;
    size_t block_len;
    TEST_FAILURE(new_da_args_block(NULL, 0, NULL, &block_len));
    TEST_FAILURE(new_da_args_block(NULL, 0, &block, NULL));

    const char *bad_args[] = {
        "a", NULL, "b"
    };
    const size_t num_bad_args = sizeof(bad_args) / sizeof(bad_args[0]);

    TEST_FAILURE(new_da_args_block(bad_args, num_bad_args, &block, &block_len));

    TEST_SUCCEED();
}

bool test_app(void) {
    BEGIN_SUITE("App");
    RUN_TEST(test_new_args_block);
    RUN_TEST(test_bad_new_args_block);
    return END_SUITE();
}
