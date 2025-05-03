
#include "s_data/test/map.h"
#include "s_data/map.h"

#include "k_bios_term/term.h"
#include "s_mem/allocator.h"

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

static map_t *(*gen_map)(size_t ks, size_t vs) = NULL;
static size_t num_al_blocks;

static bool pretest(void) {
    num_al_blocks = al_num_user_blocks(get_default_allocator());

    TEST_SUCCEED();
}

static bool posttest(void) {
    TEST_EQUAL_HEX(num_al_blocks, al_num_user_blocks(get_default_allocator()));

    TEST_SUCCEED();
}

bool test_map(const char *name, map_t *(*gen)(size_t ks, size_t vs)) {
    gen_map = gen;

    BEGIN_SUITE(name);
    return END_SUITE();
}
