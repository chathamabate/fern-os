
#pragma once

#include "s_data/test/list.h"
#include "s_data/list.h"

#include <stdbool.h>
#include "s_data/list.h"

#include "s_mem/allocator.h"

#include "k_bios_term/term.h"

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)

#include "s_util/test.h"

/**
 * The list generator to be used by each test.
 */

static list_t *(*gen_list)(void) = NULL;
static list_t *l = NULL;
static size_t num_al_blocks;

static bool pretest(void) {
    num_al_blocks = al_num_user_blocks(get_default_allocator());

    l = gen_list();
    TEST_TRUE(l != NULL);

    TEST_SUCCEED();
}

static bool posttest(void) {
    delete_list(l);

    TEST_EQUAL_HEX(num_al_blocks, al_num_user_blocks(get_default_allocator()));

    TEST_SUCCEED();
}

bool test_list(const char *name, list_t *(*gen)(void)) {
    gen_list = gen;
    BEGIN_SUITE(name);
    //... Run tests....
    return END_SUITE();
}
