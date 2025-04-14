
#pragma once

#include "s_data/test/list.h"
#include "s_data/list.h"

#include <stdbool.h>
#include "s_data/list.h"

#include "s_mem/allocator.h"

#include "k_bios_term/term.h"
#include "s_util/err.h"

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

/**
 * The list generator to be used by each test.
 */

static list_t *(*gen_list)(size_t cs) = NULL;
static size_t num_al_blocks;

static bool pretest(void) {
    num_al_blocks = al_num_user_blocks(get_default_allocator());

    TEST_SUCCEED();
}

static bool posttest(void) {
    TEST_EQUAL_HEX(num_al_blocks, al_num_user_blocks(get_default_allocator()));

    TEST_SUCCEED();
}

static bool test_cell_size(void) {
    list_t *l = gen_list(sizeof(int));
    TEST_TRUE(l != NULL);

    TEST_EQUAL_HEX(sizeof(int), l_get_cell_size(l));

    delete_list(l);
    
    TEST_SUCCEED();
}

static bool test_push(void) {
    list_t *l = gen_list(sizeof(int));
    TEST_TRUE(l != NULL);

    int *ptr;
    int num;

    TEST_EQUAL_HEX(0, l_get_len(l));
    TEST_EQUAL_HEX(NULL, l_get_ptr(l, 0));
    TEST_EQUAL_HEX(NULL, l_get_ptr(l, 10));

    num = 6;
    TEST_EQUAL_HEX(FOS_SUCCESS, l_push(l, 0, &num));
    TEST_EQUAL_HEX(1, l_get_len(l));

    ptr = (int *)l_get_ptr(l, 0);

    TEST_TRUE(ptr != NULL);
    TEST_EQUAL_INT(6, *ptr);
    TEST_EQUAL_HEX(NULL, l_get_ptr(l, 1));

    num = 8;
    TEST_EQUAL_HEX(FOS_SUCCESS, l_push(l, 0, &num));

    ptr = (int *)l_get_ptr(l, 0);
    TEST_TRUE(ptr != NULL);
    TEST_EQUAL_INT(8, *ptr);
     
    ptr = (int *)l_get_ptr(l, 1);
    TEST_TRUE(ptr != NULL);
    TEST_EQUAL_INT(6, *ptr);

    num = 10;
    TEST_EQUAL_HEX(FOS_SUCCESS, l_push(l, 1, &num));

    num = 12;
    TEST_EQUAL_HEX(FOS_SUCCESS, l_push(l, 3, &num));

    ptr = (int *)l_get_ptr(l, 1);
    TEST_TRUE(ptr != NULL);
    TEST_EQUAL_INT(10, *ptr);

    ptr = (int *)l_get_ptr(l, 3);
    TEST_TRUE(ptr != NULL);
    TEST_EQUAL_INT(12, *ptr);

    TEST_EQUAL_HEX(4, l_get_len(l));
    TEST_EQUAL_HEX(NULL, l_get_ptr(l, 4));

    delete_list(l);

    TEST_SUCCEED();
}

static bool test_pop(void) {
     
    TEST_SUCCEED();
}

bool test_list(const char *name, list_t *(*gen)(size_t cs)) {
    gen_list = gen;
    BEGIN_SUITE(name);
    RUN_TEST(test_cell_size);
    RUN_TEST(test_push);
    RUN_TEST(test_pop);
    return END_SUITE();
}
