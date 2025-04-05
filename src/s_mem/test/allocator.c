

#include "s_mem/test/allocator.h"
#include "s_mem/allocator.h"
#include "k_bios_term/term.h"


static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

/**
 * The allocator generator used by each test.
 */
static allocator_t *(*gen_allocator)(void) = NULL;
static allocator_t *al = NULL;

static bool pretest(void) {
    al = gen_allocator();

    TEST_TRUE(al != NULL);
    TEST_EQUAL_UINT(0, al_num_user_blocks(al));

    TEST_SUCCEED();
}

static bool posttest(void) {

    TEST_EQUAL_UINT(0, al_num_user_blocks(al));
    delete_allocator(al);
    al = NULL;

    TEST_SUCCEED();
}

bool test_allocator(const char *name, allocator_t *(*gen)(void)) {
    gen_allocator = gen;

    BEGIN_SUITE(name);
    return END_SUITE();
}
