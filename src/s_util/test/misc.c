
#include "k_startup/vga_cd.h"

// NOTE: This is a cyclic dependency, but we'll allow it.
#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"
#include "s_util/test/misc.h"
#include "s_util/misc.h"

static bool test_intervals_overlap(void) {
    /**
     * A case here should describe overlapping interals.
     */
    typedef struct {
        int32_t pos;
        int32_t len;

        int32_t window_len;

        /*
         * Expected outputs.
         */

        int32_t e_pos;
        int32_t e_len;
    } case_t;

    const case_t CASES[] = {
        {0, 1, 1, 0, 1},
        {1, 1, 2, 1, 1},
        {1, 1, 3, 1, 1},
        {2, 5, 4, 2, 2},

        {-2, 5, 5, 0, 3},
        {-2, 7, 5, 0, 5},
        {-3, 100, 20, 0, 20},
        {-5, INT32_MAX, INT32_MAX, 0, INT32_MAX - 5},

        {10, INT32_MAX, 20, 10, 10},
        {10, INT32_MAX, INT32_MAX, 10, INT32_MAX - 10},
        {INT32_MAX - 1, 1, INT32_MAX, INT32_MAX - 1, 1},
        {INT32_MAX - 1, 2, INT32_MAX, INT32_MAX - 1, 1},
    };
    const size_t NUM_CASES = sizeof(CASES) / sizeof(CASES[0]);

    for (size_t i = 0; i < NUM_CASES; i++) {
        const case_t *c = CASES + i;
        
        int32_t pos = c->pos;
        int32_t len = c->len;

        TEST_TRUE(intervals_overlap(&pos, &len, c->window_len));
        TEST_EQUAL_INT(c->e_pos, pos);
        TEST_EQUAL_INT(c->e_len, len);
    }

    TEST_SUCCEED();
}

static bool test_intervals_dont_overlap(void) {
    // Each case should describe a non-overlapping interval.
    // (Or an invalid interval which should always return false)
    typedef struct {
        int32_t pos;
        int32_t len;

        int32_t window_len;
    } case_t;

    const case_t CASES[] = {
        // invalid args.
        {0, 0, 1},
        {0, 1, 0},
        {0, 1, -1},
        {0, -1, 1},

        // valid, but non-overlapping intervals.
       
        // To the left.
        {-1, 1, 10},
        {-5, 3, 10},
        {INT32_MIN, INT32_MAX, 10},

        // To the right.
        {10, 4, 10},
        {10, INT32_MAX, 10},
        {15, 100, 1},
        {INT32_MAX, INT32_MAX, INT32_MAX},
        {INT32_MAX, 1, INT32_MAX},
    };
    const size_t NUM_CASES =  sizeof(CASES) / sizeof(CASES[0]);

    for (size_t i = 0; i < NUM_CASES; i++) {
        const case_t *c = CASES + i;
        int32_t pos = c->pos;
        int32_t len = c->len;

        TEST_FALSE(intervals_overlap(&pos, &len, CASES[i].window_len));
    }

    TEST_SUCCEED();
}

bool test_misc(void) {
    BEGIN_SUITE("Misc");
    RUN_TEST(test_intervals_overlap);
    RUN_TEST(test_intervals_dont_overlap);
    return END_SUITE();
}
