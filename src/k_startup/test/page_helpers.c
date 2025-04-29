
#include "k_startup/test/page_helpers.h"
#include "k_startup/page.h"
#include "k_startup/page_helpers.h"
#include "k_sys/page.h"

#include "k_bios_term/term.h"
#include "k_sys/debug.h"
#include "s_util/err.h"
#include "s_util/str.h"
#include <stdint.h>

static bool pretest(void);
static bool posttest(void);

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

static bool test_page_copy(void) {
    enable_loss_check();

    phys_addr_t p0 = pop_free_page();
    TEST_TRUE(p0 != NULL_PHYS_ADDR);
    assign_free_page(0, p0);

    for (uint32_t i = 0; i < M_4K; i++) {
        free_kernel_pages[0][i] = (i + 13) * (i + 3);
    }

    phys_addr_t p1 = pop_free_page();
    TEST_TRUE(p1 != NULL_PHYS_ADDR);

    page_copy(p1, p0);
    assign_free_page(1, p1);

    TEST_TRUE(mem_cmp(free_kernel_pages[0], free_kernel_pages[1], M_4K));

    push_free_page(p1);
    push_free_page(p0);

    TEST_SUCCEED();
}

/**
 * Take a physical page and "randomize" its contents.
 */
static void randomize_page(phys_addr_t p, uint32_t seed) {
    phys_addr_t old = assign_free_page(0, p);

    uint8_t *vp = (uint8_t *)(free_kernel_pages[0]);

    for (uint32_t i = 0; i < M_4K; i++) {
        // This isn't really a "random" formula, just made it up on the spot.
        vp[i] = (seed + i + 11) * (seed + i + 123);
    }

    assign_free_page(0, old);
}

/**
 * Allocate a range of pages within a page table, randomize the contents of all
 * allocated pages.
 */
static bool pt_randomize_alloc(phys_addr_t pt, bool user, uint32_t s, uint32_t e) {
    uint32_t true_e;
    TEST_EQUAL_HEX(FOS_SUCCESS, pt_alloc_range(pt, user, s, e, &true_e));

    phys_addr_t old0 = assign_free_page(0, pt);
    
    pt_entry_t *vpt = (pt_entry_t *)(free_kernel_pages[0]);

    for (uint32_t i = 0; i < s; i++) {
        phys_addr_t base = pte_get_base(vpt[i]);
        randomize_page(base, base / M_4K);
    }

    assign_free_page(0, old0);
    
    TEST_SUCCEED();
}

/**
 * Check if two physical pages have equal contents.
 */
static bool check_equal_page(phys_addr_t p0, phys_addr_t p1) {
    phys_addr_t old0 = assign_free_page(0, p0);
    phys_addr_t old1 = assign_free_page(1, p1);

    const uint8_t *vp0 = (uint8_t *)(free_kernel_pages[0]);
    const uint8_t *vp1 = (uint8_t *)(free_kernel_pages[1]);

    TEST_TRUE(mem_cmp(vp0, vp1, M_4K));

    assign_free_page(1, old1);
    assign_free_page(0, old0);

    TEST_SUCCEED();
}

/**
 * This function confirms that the two page tables are more or less equal.
 *
 * I say more or less, because if two page table have unique entries in the same slot,
 * those entries should both point to different physical pages!
 */
static bool check_equiv_pt(phys_addr_t pt0, phys_addr_t pt1) {
    phys_addr_t old0 = assign_free_page(0, pt0);
    phys_addr_t old1 = assign_free_page(1, pt1);

    pt_entry_t *vpt0 = (pt_entry_t *)(free_kernel_pages[0]);
    pt_entry_t *vpt1 = (pt_entry_t *)(free_kernel_pages[1]);

    for (uint32_t i = 0; i < 1024; i++) {
        pt_entry_t pte0 = vpt0[i];
        pt_entry_t pte1 = vpt1[i];

        TEST_EQUAL_UINT(pte_get_present(pte0), pte_get_present(pte1));

        if (pte_get_present(pte0)) {
            TEST_EQUAL_UINT(pte_get_user(pte0), pte_get_user(pte1));
            TEST_EQUAL_UINT(pte_get_writable(pte0), pte_get_writable(pte1));
            TEST_EQUAL_UINT(pte_get_avail(pte0), pte_get_avail(pte1));

            if (pte_get_avail(pte1) == UNIQUE_ENTRY) {
                phys_addr_t b0 = pte_get_base(pte0);
                phys_addr_t b1 = pte_get_base(pte1);

                TEST_TRUE(b0 != b1);
                TEST_TRUE(check_equal_page(b0, b1));
            } else {
                TEST_EQUAL_HEX(pte_get_base(pte0), pte_get_base(pte1));
            }
        }
    }

    assign_free_page(1, old1);
    assign_free_page(0, old0);

    TEST_SUCCEED();
}

/**
 * Pretty simple test, just make a funky page table, copy it, make sure the
 * copy and the original are equivelant.
 */
static bool test_copy_page_table(void) {
    enable_loss_check();

    phys_addr_t pt = new_page_table();
    TEST_TRUE(NULL_PHYS_ADDR != pt);

    pt_randomize_alloc(pt, true, 0, 1024);

    phys_addr_t pt_copy = copy_page_table(pt);
    TEST_TRUE(NULL_PHYS_ADDR != pt_copy);

    TEST_TRUE(check_equiv_pt(pt, pt_copy));

    delete_page_table(pt);
    delete_page_table(pt_copy);
    TEST_SUCCEED();
}

bool test_page_helpers(void) {
    BEGIN_SUITE("Page Helpers");

    RUN_TEST(test_page_copy);
    RUN_TEST(test_copy_page_table);

    return END_SUITE();
}
