
#include "k_startup/vga_cd.h"

// NOTE: This is a cyclic dependency, but we'll allow it.
#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"
#include "s_util/test/misc.h"
#include "s_util/misc.h"

static bool test_intervals_overlap(void) {

    TEST_SUCCEED();
}

static bool test_intervals_dont_overlap(void) {
    // Each case should describe a non-overlapping interval.
    // (Or an invalid interval which should always return false)
    const struct {
        int32_t pos;
        int32_t len;

        int32_t window_len;
    } CASES[] = {
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
        int32_t pos = CASES[i].pos;
        int32_t len = CASES[i].len;

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
