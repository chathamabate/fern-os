
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
    list_t *l = gen_list(sizeof(uint8_t));
    TEST_TRUE(l != NULL);

    // push 0-9
    for (uint8_t i = 0; i < 10; i++) {
        TEST_EQUAL_HEX(FOS_SUCCESS, l_push(l, i, &i));
    }

    uint8_t val;

    // pop 1-3
    for (uint8_t i = 1; i < 4; i++) {
        
        TEST_EQUAL_HEX(FOS_SUCCESS, l_pop(l, 1, &val));
        TEST_EQUAL_UINT(i, val);
    }

    TEST_EQUAL_HEX(7, l_get_len(l));

    // pop 9
    TEST_EQUAL_HEX(FOS_SUCCESS, l_pop(l, l_get_len(l) - 1, &val));
    TEST_EQUAL_UINT(9, val);

    // pop 0
    TEST_EQUAL_HEX(FOS_SUCCESS, l_pop(l, 0, &val)); 
    TEST_EQUAL_UINT(0, val);

    // 4 - 8 is left.
    for (uint8_t i = 0; i < 5; i++) {
        uint8_t *ptr = l_get_ptr(l, i);
        TEST_TRUE(ptr != NULL);

        TEST_EQUAL_UINT(i + 4, *ptr);
    }

    delete_list(l);
     
    TEST_SUCCEED();
}

static bool test_big(void) {
    list_t *l = gen_list(sizeof(int));
    TEST_TRUE(l != NULL);

    // 0 => 999
    for (int i = 999; i >= 0; i--) {
        TEST_EQUAL_HEX(FOS_SUCCESS, l_push(l, 0, &i));
    }

    // Double every other element.
    for (int i = 0; i < 1000; i += 2) {
        int *ptr = l_get_ptr(l, (size_t)i);
        TEST_TRUE(ptr != NULL);

        *ptr *= 2;
    }

    // Remove even indexed elements, leaving just the doubled elements.
    for (int i = 999; i >= 0; i -= 2) {
        int val;
        TEST_EQUAL_HEX(FOS_SUCCESS, l_pop(l, (size_t)i, &val));
        TEST_EQUAL_INT(i, val);
    }

    for (int i = 0; i < 1000; i += 2) {
        int val;
        TEST_EQUAL_HEX(FOS_SUCCESS, l_pop(l, 0, &val));
        TEST_EQUAL_INT(i * 2, val);
    }

    TEST_EQUAL_HEX(0, l_get_len(l));

    delete_list(l);
    TEST_SUCCEED();
}

/* Now for iterator tests */

static bool test_basic_iter(void) {
    list_t *l = gen_list(sizeof(int));
    TEST_TRUE(l != NULL);

    // 0 => 9
    for (int i = 0; i < 10; i++) {
        TEST_EQUAL_HEX(FOS_SUCCESS, l_push(l, (size_t)i, &i));
    }

    int count = 0;
    int *ptr;

    l_reset_iter(l);
    while ((ptr = (int *)l_next_iter(l))) {
        TEST_EQUAL_INT(count, *ptr);
        count++;
    }

    TEST_TRUE(l_get_iter(l) == NULL);
    TEST_EQUAL_INT(10, count);

    delete_list(l);
    TEST_SUCCEED();
}

static bool test_mutate_iter(void) {
    list_t *l = gen_list(sizeof(int));
    TEST_TRUE(l != NULL);

    // 0 => 9
    for (int i = 0; i < 10; i++) {
        TEST_EQUAL_HEX(FOS_SUCCESS, l_push(l, (size_t)i, &i));
    }

    int *ptr;
    int val;

    l_reset_iter(l);
    
    ptr = (int *)l_get_iter(l);
    TEST_TRUE(ptr != NULL);
    TEST_EQUAL_INT(0, *ptr);

    // 1 => 9
    TEST_EQUAL_HEX(FOS_SUCCESS, l_pop_iter(l, &val));
    TEST_EQUAL_INT(0, val);

    ptr = (int *)l_get_iter(l);
    TEST_TRUE(ptr != NULL);
    TEST_EQUAL_INT(1, *ptr);

    ptr = (int *)l_next_iter(l);
    TEST_TRUE(ptr != NULL);
    TEST_EQUAL_INT(2, *ptr);

    val = 10;
    // 1, 2, 10, 3 => 9
    TEST_EQUAL_HEX(FOS_SUCCESS, l_push_after_iter(l, &val));

    ptr = (int *)l_get_iter(l);
    TEST_TRUE(ptr != NULL);
    TEST_EQUAL_INT(2, *ptr);

    ptr = (int *)l_next_iter(l);
    TEST_TRUE(ptr != NULL);
    TEST_EQUAL_INT(10, *ptr);

    l_next_iter(l);

    for (int i = 3; i < 10; i++) {
        TEST_EQUAL_HEX(FOS_SUCCESS, l_pop_iter(l, &val));
        TEST_EQUAL_INT(i, val);
    }

    // Both of these iter calls should fail once we've reached the end of the list.
    TEST_TRUE(FOS_SUCCESS != l_push_after_iter(l, &val));
    TEST_TRUE(FOS_SUCCESS != l_pop_iter(l, &val));

    delete_list(l);
    TEST_SUCCEED();
}

/* Some input fuzzing not covered in above tests */
static bool test_bad_calls(void) {
    list_t *l = gen_list(sizeof(int));
    TEST_TRUE(l != NULL);
    
    int val = 10;
    TEST_EQUAL_HEX(FOS_SUCCESS, l_push(l, 0, &val));

    TEST_TRUE(FOS_SUCCESS != l_push(l, 10, &val));
    TEST_TRUE(FOS_SUCCESS != l_push(l, 0, NULL));

    l_reset_iter(l); 
    TEST_TRUE(FOS_SUCCESS != l_push_after_iter(l, NULL));

    delete_list(l);
    TEST_SUCCEED();
}

bool test_list(const char *name, list_t *(*gen)(size_t cs)) {
    gen_list = gen;
    BEGIN_SUITE(name);
    RUN_TEST(test_cell_size);
    RUN_TEST(test_push);
    RUN_TEST(test_pop);
    RUN_TEST(test_big);
    RUN_TEST(test_basic_iter);
    RUN_TEST(test_mutate_iter);
    RUN_TEST(test_bad_calls);
    return END_SUITE();
}
