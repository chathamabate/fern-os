
#include "k_startup/test/page.h"
#include "k_startup/page.h"
#include "k_sys/page.h"

#include "k_sys/debug.h"
#include "s_util/err.h"
#include "s_util/constraints.h"
#include "s_util/str.h"
#include <stdint.h>
#include "k_startup/gfx.h"

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) gfx_direct_put_fmt_s_rr(__VA_ARGS__)
#define FAILURE_ACTION() lock_up()

#include "s_util/test.h"

/*
 * NOTE: These tests assume that virtual addresses after FOS_FREE_AREA_START are available.
 * If you have already set up some sort of heap in the FOS Free Area, these tests will fail!
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

static bool pretest(void) {
    expect_no_loss = false;
    initial_num_free_pages = get_num_free_pages();

    TEST_SUCCEED();
}

static bool posttest(void) {
    uint32_t actual_num_free_pages = get_num_free_pages();

    if (expect_no_loss) {
        TEST_EQUAL_HEX(initial_num_free_pages, actual_num_free_pages);
    }

    TEST_SUCCEED();
}


static bool test_assign_free_page(void) {
    // Ok, there are 2 free pages.

    enable_loss_check();

    phys_addr_t p0 = pop_free_page();
    TEST_TRUE(p0 != NULL_PHYS_ADDR);

    phys_addr_t p1 = pop_free_page();
    TEST_TRUE(p1 != NULL_PHYS_ADDR);

    phys_addr_t old0 = assign_free_page(0, p0);
    // 0 -> 0
    free_kernel_pages[0][0] = 0xAB;

    phys_addr_t old0p = assign_free_page(0, p1);
    // 0 -> 1
    
    free_kernel_pages[0][0] = 0xFE;

    assign_free_page(0, old0p);
    // 0 -> 0

    phys_addr_t old1 = assign_free_page(1, p0);
    // 1 -> 0

    TEST_EQUAL_HEX(0xAB, free_kernel_pages[1][0]);
    free_kernel_pages[1][0] = 0xCC;

    assign_free_page(1, old1);
    // 1 -> *

    TEST_EQUAL_HEX(0xCC, free_kernel_pages[0][0]);

    old1 = assign_free_page(1, p1);
    // 1 -> 1
    
    TEST_EQUAL_HEX(0xFE, free_kernel_pages[1][0]);

    assign_free_page(1, old1);
    // 1 -> *

    assign_free_page(0, old0);
    // 0 -> *
    
    push_free_page(p0);
    push_free_page(p1);
    
    TEST_SUCCEED();
}

static bool test_push_and_pop(void) {
    enable_loss_check();

    phys_addr_t free_page = pop_free_page();
    TEST_FALSE(free_page == NULL_PHYS_ADDR);

    TEST_EQUAL_HEX(initial_num_free_pages - 1, get_num_free_pages());

    push_free_page(free_page);

    TEST_SUCCEED();
}

static bool test_new_pt(void) {
    enable_loss_check();

    phys_addr_t pt = new_page_table();

    phys_addr_t old = assign_free_page(0, pt);

    pt_entry_t *pt_arr = (pt_entry_t *)(free_kernel_pages[0]);

    for (size_t i = 0; i < 1024; i++) {
        TEST_EQUAL_HEX(0x0, pte_get_present(pt_arr[i]));
    }

    assign_free_page(0, old);

    delete_page_table(pt);

    TEST_SUCCEED();
}

/**
 * Helper subtest. 
 *
 * Confirm the given range matches the given properties.
 *
 * present - The range must contain all present and allocated entries.
 * user - if present, the range must contain all user entries.
 */
static bool check_pt_range(pt_entry_t *pt, uint32_t s, uint32_t e, bool present, bool user) {
    for (uint32_t i = s; i < e; i++) {
        pt_entry_t pte = pt[i];
        
        if (present) {
            TEST_EQUAL_UINT(1, pte_get_present(pte));
            TEST_EQUAL_UINT(user ? 1 : 0, pte_get_user(pte));
            TEST_TRUE(NULL_PHYS_ADDR != pte_get_base(pte));
        } else {
            TEST_EQUAL_UINT(0, pte_get_present(pte));
            TEST_EQUAL_HEX(NULL_PHYS_ADDR, pte_get_base(pte));
        }
    }

    TEST_SUCCEED();
}

/**
 * I added in this very simple pt test, probably could be expanded on.
 */
static bool test_pt_alloc(void) {
    enable_loss_check();
    uint32_t true_e;

    phys_addr_t pt = new_page_table();

    phys_addr_t old = assign_free_page(0, pt);

    pt_entry_t *vpt = (pt_entry_t *)(free_kernel_pages[0]);

    // Should start as all free.
    check_pt_range(vpt, 0, 1024, false, false);

    TEST_EQUAL_HEX(FOS_E_SUCCESS, pt_alloc_range(pt, true, false, 0, 100, &true_e));
    TEST_EQUAL_UINT(100, true_e);
    check_pt_range(vpt, 0, 100, true, true);

    pt_free_range(pt, false, 10, 20);
    check_pt_range(vpt, 0, 10, true, true);
    check_pt_range(vpt, 10, 20, false, false);
    check_pt_range(vpt, 20, 100, true, true);

    // Overlapping alloc.
    TEST_EQUAL_HEX(FOS_E_ALREADY_ALLOCATED, pt_alloc_range(pt, false, false, 15, 25, &true_e));
    TEST_EQUAL_UINT(20, true_e);
    check_pt_range(vpt, 15, 20, true, false);
    check_pt_range(vpt, 20, 100, true, true);

    // Test some bad ranges.
    TEST_TRUE(FOS_E_SUCCESS != pt_alloc_range(pt, false, false, 200, 1025, &true_e));
    TEST_TRUE(FOS_E_SUCCESS != pt_alloc_range(pt, false, false, 1024, 1024, &true_e));
    TEST_TRUE(FOS_E_SUCCESS != pt_alloc_range(pt, false, false, 1020, 1000, &true_e));

    assign_free_page(0, old);

    delete_page_table(pt);

    TEST_SUCCEED();
}

static bool test_pd_alloc(void) {
    uint8_t * const S = (uint8_t *)FOS_FREE_AREA_START;

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

    phys_addr_t pd = get_kernel_pd();

    uint32_t init_pages;

    for (size_t i = 0; i < NUM_CASES; i++) {
        case_t c = CASES[i];

        const uint32_t num_range_pages = ((uint32_t)c.e - (uint32_t)c.s) / M_4K;

        init_pages = get_num_free_pages();

        const void *true_e;
        fernos_error_t err = pd_alloc_pages(pd, false, false, c.s, c.e, &true_e);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

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
        pd_free_pages(pd, false, c.s, c.e);

        // We know at least num_range_pages were added back to the free list.
        TEST_TRUE(get_num_free_pages() >= init_pages + num_range_pages);
    }

    TEST_SUCCEED();
}

static bool test_pd_overlapping_alloc(void) {
    uint8_t * const S = (uint8_t *)FOS_FREE_AREA_START;
    phys_addr_t pd = get_kernel_pd();

    fernos_error_t err;
    const void *true_e;

    err = pd_alloc_pages(pd, false, false, S + (2*M_4K), S + (8*M_4K), &true_e);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    TEST_EQUAL_HEX(S + (8*M_4K), true_e);

    pd_free_pages(pd, false, S, S + (6*M_4K));

    err = pd_alloc_pages(pd, false, false, S, S + (3*M_4K), &true_e);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);
    TEST_EQUAL_HEX(S + (3*M_4K), true_e);

    // 3, 4, and 5 should all be free.
    err = pd_alloc_pages(pd, false, false, S + (3*M_4K), S + (7*M_4K), &true_e);
    TEST_EQUAL_HEX(FOS_E_ALREADY_ALLOCATED, err);
    TEST_EQUAL_HEX(S + (6*M_4K), true_e);

    err = pd_alloc_pages(pd, false, false, S + (3*M_4K), S + (7*M_4K), &true_e);
    TEST_EQUAL_HEX(FOS_E_ALREADY_ALLOCATED, err);
    TEST_EQUAL_HEX(S + (3*M_4K), true_e);

    pd_free_pages(pd, false, S, S + (8*M_4K));

    TEST_SUCCEED();
}

static bool test_pd_free(void) {
    uint8_t * const S = (uint8_t *)FOS_FREE_AREA_START;
    phys_addr_t pd = get_kernel_pd();

    fernos_error_t err;
    const void *true_e;

    uint32_t num_fps;

    /* This test isn't really that rigorous tbh */

    err = pd_alloc_pages(pd, false, false, S, S + (5 * M_4K), &true_e);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    num_fps = get_num_free_pages();

    pd_free_pages(pd, false, S, S + (5 * M_4K));
    TEST_TRUE(get_num_free_pages() >= num_fps + 5);

    /* Test a larger and weirder range */

    err = pd_alloc_pages(pd, false, false, S + (7 * M_4K), S + (3012 * M_4K), &true_e);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    num_fps = get_num_free_pages();

    pd_free_pages(pd, false, S + (7 * M_4K), S + (3012 * M_4K));
    TEST_TRUE(get_num_free_pages() >= num_fps + (3012 - 7));

    TEST_SUCCEED();
}

/**
 * You should expect this test to halt the processor.
 * (Given an interrupt handler is setup for page faults)
 */
static bool test_pd_free_dangerous(void) {
    LOGF_PREFIXED("Running Dangerous Test\n");

    uint8_t * const S = (uint8_t *)FOS_FREE_AREA_START;
    phys_addr_t pd = get_kernel_pd();

    fernos_error_t err;
    const void *true_e;

    err = pd_alloc_pages(pd, false, false, S, S + (4 * M_4K), &true_e);
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    *(uint32_t *)S = 1234;
    LOGF_PREFIXED("We have written to S\n");

    pd_free_pages(pd, false, S, S + (2 * M_4K));
    *(uint32_t *)S = 1234;
    LOGF_PREFIXED("We have written again to S\n");

    TEST_SUCCEED();
}

static bool test_delete_page_directory(void) {
    enable_loss_check();
    fernos_error_t err;

    phys_addr_t pd = new_page_directory();
    TEST_TRUE(pd != NULL_PHYS_ADDR);

    const void *true_e;

    err = pd_alloc_pages(pd, false, false, (void *)M_4K, (void *)M_4M, &true_e); 
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    err = pd_alloc_pages(pd, false, false, (void *)(4 * M_4M), (void *)(6*M_4M + 12*M_4K), &true_e); 
    TEST_EQUAL_HEX(FOS_E_SUCCESS, err);

    delete_page_directory(pd);

    TEST_SUCCEED();
}


bool test_page(void) {
    BEGIN_SUITE("Paging");

    RUN_TEST(test_assign_free_page);
    RUN_TEST(test_push_and_pop);
    RUN_TEST(test_new_pt);
    RUN_TEST(test_pt_alloc);
    RUN_TEST(test_pd_alloc);
    RUN_TEST(test_pd_overlapping_alloc);
    RUN_TEST(test_pd_free);

    // Remember this halts the cpu.
    //RUN_TEST(test_pd_free_dangerous);
    (void)test_pd_free_dangerous;

    RUN_TEST(test_delete_page_directory);

    return END_SUITE();
}

