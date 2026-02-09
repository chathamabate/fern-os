
#include "s_data/fixed_queue.h"
#include "s_data/test/fixed_queue.h"

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

static bool test_fq_new_and_delete(void) {
    fixed_queue_t *fq = new_da_fixed_queue(sizeof(uint32_t), 10);
    TEST_TRUE(fq != NULL);
    delete_fixed_queue(fq);

    TEST_SUCCEED();
}

static bool test_fq_simple_usage(void) {
    fixed_queue_t *fq = new_da_fixed_queue(sizeof(uint32_t), 4);
    TEST_TRUE(fq != NULL);

    uint32_t val;

    val = 4;
    TEST_TRUE(fq_enqueue(fq, &val, false));

    // [4]

    val = 3;
    TEST_TRUE(fq_enqueue(fq, &val, false));

    // [4, 3]

    TEST_TRUE(fq_poll(fq, &val));
    TEST_EQUAL_UINT(4, val);

    // [3]

    for (uint32_t i = 4; i <= 6; i++) {
        TEST_TRUE(fq_enqueue(fq, &i, false));
    }

    // [3, 4, 5, 6]
    
    TEST_FALSE(fq_enqueue(fq, &val, false));

    val = 7;
    TEST_TRUE(fq_enqueue(fq, &val, true));

    // [4, 5, 6, 7]

    for (uint32_t i = 4; i <= 7; i++) {
        TEST_TRUE(fq_poll(fq, &val));
        TEST_EQUAL_UINT(i, val);
    }

    // []

    TEST_TRUE(fq_is_empty(fq));

    delete_fixed_queue(fq);

    TEST_SUCCEED();
}

static bool test_fq_complex_usage(void) {
    fixed_queue_t *fq = new_da_fixed_queue(sizeof(uint16_t), 6);
    TEST_TRUE(fq != NULL);

    typedef struct _case_t {
        bool enqueue;
        bool writeover;

        size_t num_vals;
        uint16_t vals[16];
    } case_t;

    const case_t CASES[] = {
        {
            true, false,
            3,
            {1, 2, 3}
        }, // [1, 2, 3]
        {
            false, false,
            1,
            {1} 
        }, // [2, 3]
        {
            true, false,
            5,
            {UINT16_MAX, 4, 5, 5, 6}
        }, // [2, 3, U16_MAX, 4, 5, 5]
        {
            false, false,
            3,
            {2, 3, UINT16_MAX}
        }, // [4, 5, 5]
        {
            true, true,
            5,
            {2, 3, 9, 10, 12}
        }, // [5, 2, 3, 9, 10, 12]
        {
            false, false,
            6,
            {5, 2, 3, 9, 10, 12}
        }, // []
        {
            true, false,
            2,
            {1, 2}
        },
        {
            false, false,
            2,
            {1, 2}
        }
    };
    const size_t NUM_CASES = sizeof(CASES) / sizeof(CASES[0]);

    for (size_t ci = 0; ci < NUM_CASES; ci++) {
        const case_t * const c = CASES + ci;
        
        for (size_t i = 0; i < c->num_vals; i++) {
            if (c->enqueue) {
                fq_enqueue(fq, c->vals + i, c->writeover);
            } else {
                uint16_t val;

                TEST_TRUE(fq_poll(fq, &val));
                TEST_EQUAL_UINT(c->vals[i], val);
            }
        }
    }

    delete_fixed_queue(fq);

    TEST_SUCCEED();
}

static bool test_fixed_queue_big(void) {
    const size_t cap = 1000;

    fixed_queue_t *fq = new_da_fixed_queue(sizeof(uint64_t), cap);
    TEST_TRUE(fq != NULL);

    uint64_t val;

    for (uint64_t i = 0; i < cap + 100; i++) {
        TEST_TRUE(fq_enqueue(fq, &i, true));
    }

    for (uint64_t i = 100; i < cap + 100; i++) {
        TEST_TRUE(fq_poll(fq, &val));
        TEST_EQUAL_UINT(i, val);
    }

    TEST_TRUE(fq_is_empty(fq));

    for (uint64_t i = 0; i < 2 * cap; i++) {
        TEST_TRUE(fq_enqueue(fq, &i, false));
        TEST_TRUE(fq_poll(fq, &val));
        TEST_EQUAL_UINT(i, val);
    }

    delete_fixed_queue(fq);

    TEST_SUCCEED();
}

static bool test_fixed_queue_small(void) {
    fixed_queue_t *fq = new_da_fixed_queue(sizeof(uint8_t), 1);
    TEST_TRUE(fq != NULL);

    uint8_t val;

    val = 10;
    TEST_TRUE(fq_enqueue(fq, &val, true));

    val = 5;
    TEST_TRUE(fq_enqueue(fq, &val, true));

    TEST_TRUE(fq_poll(fq, &val));
    TEST_EQUAL_UINT(5, val);

    val = 3;
    TEST_TRUE(fq_enqueue(fq, &val, false));

    val = 2;
    TEST_FALSE(fq_enqueue(fq, &val, false));

    TEST_TRUE(fq_poll(fq, &val));
    TEST_EQUAL_UINT(3, val);

    TEST_FALSE(fq_poll(fq, &val));

    delete_fixed_queue(fq);

    TEST_SUCCEED();
}

bool test_fixed_queue(void) {
    BEGIN_SUITE("Fixed Queue");
    RUN_TEST(test_fq_new_and_delete);
    RUN_TEST(test_fq_simple_usage);
    RUN_TEST(test_fq_complex_usage);
    RUN_TEST(test_fixed_queue_big);
    RUN_TEST(test_fixed_queue_small);
    return END_SUITE();
}
