
#include "k_startup/test/page.h"
#include "k_startup/page.h"
#include "k_sys/page.h"

#include "k_bios_term/term.h"
#include "k_sys/debug.h"
#include "s_util/err.h"

static void pretest(void);
static void posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) term_put_fmt_s(__VA_ARGS__)
#define FAILURE_ACTION() lock_up()

#include "s_util/test.h"

/*
 * NOTE: These tests assume that virtual addresses after _static_area_end are available.
 * 
 * Basically, we are testing just basic paging setup operations.
 */

/**
 * When true, the test will confirm the number of free pages before and after the test are equal.
 */
static bool expect_no_loss;
static uint32_t initial_num_free_pages;

static void enable_loss_check(void) {
    expect_no_loss = true;
}

static void pretest(void) {
    expect_no_loss = false;
    initial_num_free_pages = get_num_free_pages();
}

static void posttest(void) {
    uint32_t actual_num_free_pages = get_num_free_pages();

    if (expect_no_loss) {
        TEST_EQUAL_HEX(initial_num_free_pages, actual_num_free_pages);
    }
}

static bool test_push_and_pop(void) {
    enable_loss_check();

    phys_addr_t free_page = pop_free_page();
    TEST_FALSE(free_page == NULL_PHYS_ADDR);

    TEST_EQUAL_HEX(initial_num_free_pages - 1, get_num_free_pages());

    push_free_page(free_page);

    TEST_SUCCEED();
}

static bool test_pd_alloc(void) {
    uint8_t * const S = (uint8_t *)_static_area_end;

    typedef struct {
        uint8_t *s;
        uint8_t *e;
    } case_t;

    const case_t CASES[] = {
        {.s = S, .e = S + M_4K},
        {.s = S, .e = S + (4 * M_4K)},
        {.s = S, .e = S + M_4M},
        {.s = S + (13 * M_4K), .e = S + M_4M},
        {.s = S + (132 * M_4K), .e = S + M_4M - (12 * M_4K)},
        {.s = S, .e = S + (3 * M_4M)},
        {.s = S + (13 * M_4K), .e = S + (6 * M_4M)},
        {.s = S + (132 * M_4K), .e = S + (4 * M_4M) - (12 * M_4K)},
    };
    const size_t NUM_CASES = sizeof(CASES) / sizeof(CASES[0]);

    phys_addr_t pd = get_page_directory();

    uint32_t init_pages;

    for (size_t i = 0; i < NUM_CASES; i++) {
        case_t c = CASES[i];

        const uint32_t num_range_pages = ((uint32_t)c.e - (uint32_t)c.s) / M_4K;

        init_pages = get_num_free_pages();

        void *true_e;
        fernos_error_t err = pd_alloc_pages(pd, c.s, c.e, &true_e);
        TEST_EQUAL_HEX(FOS_SUCCESS, err);

        // Always assume a full allocation.
        TEST_EQUAL_HEX(c.e, true_e);
            
        // We know at least num_pages must've been allocated!
        TEST_TRUE(get_num_free_pages() <= init_pages - num_range_pages);

        // Now just test we can read and write data form the range.
        for (uint8_t *bi = c.s; bi < c.e; bi += 11) {
            *bi =  ((uint32_t)bi * 1329121) & 0xFF;
        }

        for (uint8_t *bi = c.s; bi < c.e; bi += 11) {
            TEST_EQUAL_UINT(((uint32_t)bi * 1329121) & 0xFF, *bi);
        }

        init_pages = get_num_free_pages();
        // Finally, free the range!
        pd_free_pages(pd, c.s, c.e);

        // We know at least num_range_pages were added back to the free list.
        TEST_TRUE(get_num_free_pages() >= init_pages + num_range_pages);
    }

    TEST_SUCCEED();
}

static bool test_pd_overlapping_alloc(void) {
    uint8_t * const S = (uint8_t *)_static_area_end;
    phys_addr_t pd = get_page_directory();

    fernos_error_t err;
    void *true_e;

    err = pd_alloc_pages(pd, S + (2*M_4K), S + (8*M_4K), &true_e);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);
    TEST_EQUAL_HEX(S + (8*M_4K), true_e);

    pd_free_pages(pd, S, S + (6*M_4K));

    err = pd_alloc_pages(pd, S, S + (3*M_4K), &true_e);
    TEST_EQUAL_HEX(FOS_SUCCESS, err);
    TEST_EQUAL_HEX(S + (3*M_4K), true_e);

    // 3, 4, and 5 should all be free.
    err = pd_alloc_pages(pd, S + (3*M_4K), S + (7*M_4K), &true_e);
    TEST_EQUAL_HEX(FOS_ALREADY_ALLOCATED, err);
    TEST_EQUAL_HEX(S + (6*M_4K), true_e);

    err = pd_alloc_pages(pd, S + (3*M_4K), S + (7*M_4K), &true_e);
    TEST_EQUAL_HEX(FOS_ALREADY_ALLOCATED, err);
    TEST_EQUAL_HEX(S + (3*M_4K), true_e);

    pd_free_pages(pd, S, S + (8*M_4K));

    TEST_SUCCEED();
}

// Ok what else?
// Can we confirm that certain areas actually work????
// Like what exactly??

void test_page(void) {
    RUN_TEST(test_push_and_pop);
    RUN_TEST(test_pd_alloc);
    RUN_TEST(test_pd_overlapping_alloc);
}
