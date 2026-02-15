
#include "s_data/test/heap.h"
#include "s_data/heap.h"

#include "k_startup/gfx.h"

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) gfx_direct_put_fmt_s_rr(__VA_ARGS__)

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

static int32_t int_cmp(const void *k0, const void *k1) {
    const int32_t i0 = *(const int32_t *)k0;
    const int32_t i1 = *(const int32_t *)k1;

    if (i0 < i1) {
        return -1;
    } 

    if (i0 > i1) {
        return 1;
    }

    return 0;
}

static bool test_new_heap(void) {
    heap_t *hp = new_da_heap(sizeof(int32_t), int_cmp);
    TEST_TRUE(hp != NULL);
    delete_heap(hp);

    TEST_SUCCEED();
}

typedef struct _int_heap_check_t {
    /** 
     * Whether or not to push or pop
     */
    bool push;

    size_t num_eles;
    int32_t eles[16];
} int_heap_check_t;

static bool int_heap_test(const int_heap_check_t *checks, size_t num_checks) {
    heap_t *hp = new_da_heap(sizeof(int32_t), int_cmp);
    TEST_TRUE(hp != NULL);

    for (size_t i = 0; i < num_checks; i++) {
        const int_heap_check_t *check = checks + i;

        if (check->push) {
            for (size_t j = 0; j < check->num_eles; j++) {
                TEST_SUCCESS(hp_push(hp, check->eles + j));
            }
        } else {
            for (size_t j = 0; j < check->num_eles; j++) {
                int32_t val;
                TEST_TRUE(hp_pop_max(hp, &val)); 
                TEST_EQUAL_INT(check->eles[j], val);
            }
        }
    }

    delete_heap(hp);

    TEST_SUCCEED();
}

static bool test_int_heap0(void) {
    const int_heap_check_t checks[] = {
        { true, 2, {4, 1} },      // [4, 1]
        { false, 1, {4} },            // [1]
        { true,  4, {-1, 3, 1, 2} }, // [3, 2, 1, 1, -1]
        { false,  3, {3, 2, 1} }, // [1, -1]
        { true, 4, {INT32_MIN, INT32_MAX, 0, -1} }, // [MAX, 1, 0, -1, -1, MIN]
        { false, 6, {INT32_MAX, 1, 0, -1,  -1, INT32_MIN} }, // []
    };
    const size_t num_checks = sizeof(checks) / sizeof(checks[0]);

    TEST_TRUE(int_heap_test(checks, num_checks));
    TEST_SUCCEED();
}

static bool test_int_heap1(void) {
    const int_heap_check_t checks[] = {
        { true, 6, {0, 2, 3, 1, 5, 6} },      // [6, 5, 3, 2, 1, 0]
        { false, 2, {6, 5} }, // [3, 2, 1, 0]
        { true, 4, {10, 2, 0, 0} }, // [10, 3, 2, 2, 1, 0, 0}
        { false, 7, {10, 3, 2, 2, 1, 0, 0} } // { }
    };
    const size_t num_checks = sizeof(checks) / sizeof(checks[0]);

    TEST_TRUE(int_heap_test(checks, num_checks));
    TEST_SUCCEED();
}

static bool test_heap_misc(void) {
    heap_t *hp = new_da_heap(sizeof(int32_t), int_cmp);
    TEST_TRUE(hp != NULL);

    TEST_FALSE(hp_pop_max(hp, NULL));

    int32_t val = 10;
    TEST_SUCCESS(hp_push(hp, &val));
    TEST_TRUE(hp_pop_max(hp, NULL));

    TEST_EQUAL_HEX(FOS_E_BAD_ARGS, hp_push(hp, NULL));

    TEST_TRUE(hp_is_empty(hp));

    delete_heap(hp);

    TEST_SUCCEED();
}

typedef struct _test_range_t {
    int32_t s;
    int32_t e;
} test_range_t;

static int32_t range_cmp(const void *k0, const void *k1) {
    const test_range_t r0 = *(const test_range_t *)k0;
    const test_range_t r1 = *(const test_range_t *)k1;

    if (r0.s < r1.s) {
        return -1;
    }

    if (r0.s > r1.s) {
        return 1;
    }

    // r0.s == r1.s

    if (r0.e < r1.e) {
        return -1;
    }

    if (r0.e > r1.e) {
        return 1;
    }

    return 0;
}

static bool test_big_heap(void) {
    heap_t *hp = new_da_heap(sizeof(test_range_t), range_cmp);
    TEST_TRUE(hp != NULL);

    for (int32_t i = 0; i < 100; i++) {
        const test_range_t r = {
            .s = i,
            .e = i + 1
        };
        TEST_SUCCESS(hp_push(hp, &r));
    }

    for (int32_t i = 0; i < 100; i += 2) {
        const test_range_t r = {
            .s = i,
            .e = i + 2
        };
        TEST_SUCCESS(hp_push(hp, &r));
    }

    for (int32_t i = 100; i > 0; i -= 2) {
        const test_range_t exp_r[3] = {
            {i - 1, i},
            {i - 2, i},
            {i - 2, i - 1}
        };
        for (size_t i = 0; i < 3; i++) {
            test_range_t act_r;
            TEST_TRUE(hp_pop_max(hp, &act_r));
            TEST_EQUAL_INT(0, range_cmp(exp_r + i, &act_r));
        }
    }

    TEST_TRUE(hp_is_empty(hp));

    delete_heap(hp);

    TEST_SUCCEED();
}

bool test_heap(void) {
    BEGIN_SUITE("Heap");
    RUN_TEST(test_new_heap);
    RUN_TEST(test_int_heap0);
    RUN_TEST(test_int_heap1);
    RUN_TEST(test_heap_misc);
    RUN_TEST(test_big_heap);
    return END_SUITE();
}
