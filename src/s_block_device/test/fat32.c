
#include "s_block_device/test/fat32.h"

#include <stdbool.h>

#include "s_block_device/block_device.h"
#include "s_block_device/fat32.h"
#include "k_bios_term/term.h"

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

static bool pretest(void) {
    TEST_SUCCEED();
}

static bool posttest(void) {
    TEST_SUCCEED();
}

static bool dummy_test(void) {
    TEST_SUCCEED();
}

bool test_fat32_device(void) {
    BEGIN_SUITE("FAT32 Device");
    RUN_TEST(dummy_test);
    return END_SUITE();
}



