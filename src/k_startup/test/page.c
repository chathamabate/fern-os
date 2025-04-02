
#include "k_startup/test/page.h"
#include "k_startup/page.h"
#include "k_sys/page.h"

#include "k_bios_term/term.h"
#include "k_sys/debug.h"

static void pretest(void);
static void posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)
#define FAILURE_ACTION() lock_up()

#include "s_util/test.h"

static uint32_t initial_num_free_pages;
static void pretest(void) {
    initial_num_free_pages = get_num_free_pages();
}

static void posttest(void) {
    uint32_t actual_num_free_pages = get_num_free_pages();
    TEST_EQUAL_HEX(initial_num_free_pages, actual_num_free_pages);
}

static bool test_push_and_pop(void) {
    phys_addr_t free_page = pop_free_page();
    TEST_FALSE(free_page == NULL_PHYS_ADDR);

    TEST_EQUAL_HEX(initial_num_free_pages - 1, get_num_free_pages());

    push_free_page(free_page);

    TEST_SUCCEED();
}

void test_page(void) {
    RUN_TEST(test_push_and_pop);
}
