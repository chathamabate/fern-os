
#include "s_data/wait_queue.h"
#include "s_data/test/wait_queue.h"

#include "k_bios_term/term.h"


static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

static size_t num_al_blocks;

static bool pretest(void) {
    num_al_blocks = al_num_user_blocks(get_default_allocator());

    TEST_SUCCEED();
}

static bool posttest(void) {
    TEST_EQUAL_HEX(num_al_blocks, al_num_user_blocks(get_default_allocator()));

    TEST_SUCCEED();
}

bool test_basic_wait_queue(void) {
    BEGIN_SUITE("Basic Wait Queue");

    return END_SUITE();
}

