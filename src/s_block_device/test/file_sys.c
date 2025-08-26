
#include "s_block_device/test/file_sys.h"

#include "k_bios_term/term.h"
#include "s_mem/allocator.h"

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

static file_sys_t *(*generate_fs)(void) = NULL;
static file_sys_t *fs = NULL;
static size_t num_al_blocks = 0;

static bool pretest(void) {
    num_al_blocks = al_num_user_blocks(get_default_allocator());

    TEST_TRUE(generate_fs != NULL);
    
    fs = generate_fs();
    TEST_TRUE(fs != NULL);

    TEST_SUCCEED();
}

static bool posttest(void) {
    fernos_error_t err;

    err = fs_flush(fs, NULL);
    TEST_EQUAL_UINT(FOS_SUCCESS, err);

    delete_file_sys(fs);

    size_t curr_user_blocks = al_num_user_blocks(get_default_allocator());
    TEST_EQUAL_UINT(num_al_blocks, curr_user_blocks);

    TEST_SUCCEED();
}

static bool test_dummy(void) {

    TEST_SUCCEED();
}

bool test_file_sys(const char *name, file_sys_t *(*gen)(void)) {
    generate_fs = gen;

    BEGIN_SUITE(name);
    RUN_TEST(test_dummy);
    return END_SUITE();
}
