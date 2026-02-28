
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
 * The alloc, free, and check functions are all kinda related from a testing perspective.
 */
static bool test_pt_alloc_and_free(void) {
    fernos_error_t err;

    enable_loss_check();

    /*
     * This test will just perform a series of allocs and frees on a page table
     * and confirm its contents.
     */

    typedef struct {
        bool alloc; // Is this an alloc or a free.
        uint32_t s, e; // Start and end.

        // User or shared (For allocs)
        bool user, shared;

        // The expected value for true_e.
        uint32_t true_e;
    } step_t;

    const step_t steps[] = {
        {true, 10, 100, true, false, 100},
        // [10, 100)

        {true, 0, 100, true, true, 10},
        // [0, 100)

        {false, 5, 20, false, false, 0},
        // [0, 5) [20, 100)

        {true, 100, 101, false, false, 101},
        // [0, 5) [20, 101)

        {true, 200, 1024, true, true, 1024},
        // [0, 5) [20, 101), [200, 1024)

        {false, 4, 300, false, false, 0},
        // [0, 4) [300, 1024)

        {true, 0, 0, false, false, 0},
        // [0, 4) [300, 1024)
        
        {true, 4, 500, false, false, 300}
        // [0, 1024)
    };
    const size_t num_steps = sizeof(steps) / sizeof(steps[0]);

    // This "Mock PT" will help us confirm the contents of the page table after each step!
    struct { bool present, user, shared; } mock_pt[1024];
    mem_set(mock_pt, 0, sizeof(mock_pt));

    phys_addr_t pt = new_page_table();
    TEST_TRUE(pt != NULL_PHYS_ADDR);

    phys_addr_t old = assign_free_page(0, pt);
    const pt_entry_t *ptv = (pt_entry_t *)(free_kernel_pages[0]);

    for (size_t i = 0; i < num_steps; i++) {
        step_t s = steps[i];

        if (s.alloc) {
            uint32_t true_e;
            err = pt_alloc_range(pt, s.user, s.shared, s.s, s.e, &true_e);
            TEST_EQUAL_HEX(s.true_e == s.e ? FOS_E_SUCCESS : FOS_E_ALREADY_ALLOCATED, err);
            TEST_EQUAL_UINT(s.true_e, true_e);

            for (uint32_t mi = s.s; mi < s.true_e; mi++) {
                mock_pt[mi].present = true;
                mock_pt[mi].user = s.user;
                mock_pt[mi].shared = s.shared;
            }
        } else {
            pt_free_range(pt, true, s.s, s.e);

            for (uint32_t mi = s.s; mi < s.e; mi++) {
                mock_pt[mi].present = false;
            }
        }

        // Now for the full pt check!
        for (uint32_t mi = 0; mi < 1024; mi++) {
            pt_entry_t pte = ptv[mi]; 

            TEST_EQUAL_UINT(mock_pt[mi].present, pte_get_present(pte));
            if (mock_pt[mi].present) {
                TEST_EQUAL_UINT(mock_pt[mi].user, pte_get_user(pte));
                TEST_EQUAL_UINT(mock_pt[mi].shared ? SHARED_ENTRY : UNIQUE_ENTRY, pte_get_avail(pte));
                TEST_TRUE(NULL_PHYS_ADDR != pte_get_base(pte));
            }
        }
    }

    assign_free_page(0, old);

    // Real fast just check some bad allocs.
    TEST_EQUAL_HEX(FOS_E_INVALID_RANGE, pt_alloc_range(pt, true, true, 1024, 1024, NULL));
    TEST_EQUAL_HEX(FOS_E_INVALID_RANGE, pt_alloc_range(pt, true, true, 10, 9, NULL));

    delete_page_table_force(pt, true);

    TEST_SUCCEED();
}

static bool test_pd_alloc_and_free_p(void) {
    fernos_error_t err;
    enable_loss_check();

    // Each bit will equal 1 page! (1 = allocate, 0 = free)
    // Using bit vectors here because otherwise this would be 1M on the stack :,(
    uint8_t mock_pd[1024 * (1024 / 8)];
    mem_set(mock_pd, 0, sizeof(mock_pd));

    // This test will just check if allocating/freeing uses the correct ranges.
    // It DOES NOT test if the ranges have the correct modifiers (i.e. user or shared)

    typedef struct {
        bool alloc;
        uint32_t pi_s;
        uint32_t pi_e;

        // These are only used if alloc is true!
        bool shared;
        uint32_t true_e;
    } step_t;

    // I could've done a random test, but this should suffice.

    const step_t steps[] = {
        {true, 0, 100, false, 100},
        // [0, 100)

        {true, 1024, 1024 * 2, true, 1024 * 2},
        // [0, 100), [1024, 1024 * 2)
        
        {true, 1024 * 4 + 124, 1024 * 10 + 13, true, 1024 * 10 + 13},
        // [0, 100), [1024, 1024 * 2), [1024 * 4 + 124, 1024 * 10 + 13)

        {false, 1024 + 10, 1024 * 2 - 10, false, 0},
        // [0, 100), [1024, 1024 + 10), [1024 * 2 - 10, 1024 * 2), [1024 * 4 + 124, 1024 * 10 + 13)

        {true, 1024 + 10, 1024 * 2, true, 1024 * 2 - 10},
        // [0, 100) [1024, 1024 * 2) [1024 * 4 + 124, 1024 * 10 + 13)

        {false, 1024, 1024 * 2, false, 0},
        // [0, 100), [1024 * 4 + 124, 1024 * 10 + 13)

        {true, 1024 * 3, 1024 * 4 + 124, false, 1024 * 4 + 124},
        // [0, 100), [1024 * 3, 1024 * 10 + 13)

        // This is kinda a special case, we assume an empty allocation succeeds!
        {true, 1024 * 10 + 12, 1024 * 10 + 12, true, 1024 * 10 + 12},
        // [0, 100), [1024 * 3, 1024 * 10 + 13)

        // Here we try a single page which already allocated.
        {true, 1024 * 10 + 12, 1024 * 10 + 13, false, 1024 * 10 + 12},
        // [0, 100), [1024 * 3, 1024 * 10 + 13)
        
        // Here a single page which should succeed.
        {true, 1024 * 10 + 13, 1024 * 10 + 14, false, 1024 * 10 + 14},
        // [0, 100), [1024 * 3, 1024 * 10 + 14)

        {false, 10, 20, false, 0},
        // [0, 10), [20, 100), [1024 * 3, 1024 * 10 + 14)
        
        {true, 15,17, false, 17},
        // [0, 10), [15, 17), [20, 100), [1024 * 3, 1024 * 10 + 14)

        {false, 0, 1024 * 1024, false, 0},
        // empty

        {true, 1024 * 1022 + 5, 1024 * 1024, true,  1024 * 1024},
        // [1024 * 1022 + 5, 1024 * 1024)

        {true, 1024 * 1024 - 1, 1024 * 1024, true, 1024 * 1024 - 1}
    };

    const size_t num_steps = sizeof(steps) / sizeof(steps[0]);

    phys_addr_t pd = new_page_directory();
    TEST_TRUE(pd != NULL_PHYS_ADDR);

    phys_addr_t old0 = assign_free_page(0, pd);
    const pt_entry_t *pdv = (pt_entry_t *)(free_kernel_pages[0]);

    for (size_t i = 0; i < num_steps; i++) {
        step_t s = steps[i];        

        if (s.alloc) {
            uint32_t true_e;
            err = pd_alloc_pages_p(pd, true, s.shared, s.pi_s, s.pi_e, &true_e);
            TEST_EQUAL_HEX(s.pi_e == s.true_e ? FOS_E_SUCCESS : FOS_E_ALREADY_ALLOCATED, err);
            TEST_EQUAL_UINT(s.true_e, true_e);

            // Set 1's in the bitmap.
            for (uint32_t pi = s.pi_s; pi < s.pi_e; pi++) {
                mock_pd[pi / 8] |= (1 << (pi % 8));
            }
        } else {
            pd_free_pages_p(pd, true, s.pi_s, s.pi_e);

            // Set 0's in the bitmap.
            for (uint32_t pi = s.pi_s; pi < s.pi_e; pi++) {
                mock_pd[pi / 8] &= ~(1 << (pi % 8));
            }
        }

        // Checking for consistency with the bitmap will likely be very slow.
        // We'll only do it every few steps (Or on the final step)
        if (i % 4 == 0 || i == num_steps - 1) {
            for (uint32_t pdi = 0; pdi < 1024; pdi++) {
                pt_entry_t pde = pdv[pdi];

                if (pte_get_present(pde)) {
                    // If present, we actually need to check if bits match!
                    // We'll do this by creating a bit vector for page table and comparing
                    // it to what's in `mock_pd`.

                    phys_addr_t pt = pte_get_base(pde);

                    // Confirm pde is setup correctly in the first place for a page table!
                    TEST_TRUE(pt != NULL_PHYS_ADDR);
                    TEST_EQUAL_UINT(pte_get_avail(pde), UNIQUE_ENTRY);
                    TEST_EQUAL_UINT(pte_get_user(pde), 1);
                    TEST_EQUAL_UINT(pte_get_writable(pde), 1);

                    uint8_t pt_bv[1024 / 8];
                    mem_set(pt_bv, 0, sizeof(pt_bv));

                    // Next iterate over the page table!
                    phys_addr_t old1 = assign_free_page(1, pt);
                    const pt_entry_t *ptv = (pt_entry_t *)(free_kernel_pages[1]);
                    for (uint32_t pti = 0; pti < 1024; pti++) {
                        if (pte_get_present(ptv[pti])) {
                            pt_bv[pti / 8] |= (1 << (pti % 8));
                        } 
                    }
                    assign_free_page(1, old1);

                    // Finally, compare with mock_pd!
                    TEST_TRUE(mem_cmp(mock_pd + (pdi * (1024 / 8)), pt_bv, 1024 / 8));
                } else {
                    // If no pde, confirm that bit map is all 0's for its range.
                    TEST_TRUE(mem_chk(mock_pd + (pdi * (1024 / 8)), 0, (1024 / 8)));
                }
            }
        }
    }

    assign_free_page(0, old0);
    delete_page_directory_force(pd, true);

    TEST_SUCCEED();
}

static bool test_pd_alloc_entries(void) {
    // The above tests doesn't actually check if individual entries have the correct attributes.
    // This just quickly checks that.

    enable_loss_check();

    phys_addr_t pd = new_page_directory();

    TEST_SUCCESS(pd_alloc_pages_p(pd, true, true, 0, 10, NULL));
    TEST_SUCCESS(pd_alloc_pages_p(pd, true, false, 20, 30, NULL));
    TEST_SUCCESS(pd_alloc_pages_p(pd, false, false, 30, 40, NULL));

    phys_addr_t old = assign_free_page(0, pd);
    const pt_entry_t *pdv = (pt_entry_t *)(free_kernel_pages[0]);

    phys_addr_t pt = pte_get_base(pdv[0]);
    TEST_TRUE(pt != NULL_PHYS_ADDR);

    assign_free_page(0, pt);
    const pt_entry_t *ptv = (pt_entry_t *)(free_kernel_pages[0]);

    // Confirm each of the 3 above ranges.

    for (uint32_t pi = 0; pi < 10; pi++) {
        pt_entry_t pte = ptv[pi];

        TEST_TRUE(pte_get_present(pte));
        TEST_TRUE(pte_get_user(pte));
        TEST_EQUAL_UINT(SHARED_ENTRY, pte_get_avail(pte));
        TEST_TRUE(pte_get_base(pte) != NULL_PHYS_ADDR);
    }

    for (uint32_t pi = 20; pi < 30; pi++) {
        pt_entry_t pte = ptv[pi];

        TEST_TRUE(pte_get_present(pte));
        TEST_TRUE(pte_get_user(pte));
        TEST_EQUAL_UINT(UNIQUE_ENTRY, pte_get_avail(pte));
        TEST_TRUE(pte_get_base(pte) != NULL_PHYS_ADDR);
    }

    for (uint32_t pi = 30; pi < 40; pi++) {
        pt_entry_t pte = ptv[pi];

        TEST_TRUE(pte_get_present(pte));
        TEST_FALSE(pte_get_user(pte));
        TEST_EQUAL_UINT(UNIQUE_ENTRY, pte_get_avail(pte));
        TEST_TRUE(pte_get_base(pte) != NULL_PHYS_ADDR);
    }

    assign_free_page(0, old);

    delete_page_directory_force(pd, true);

    TEST_SUCCEED();
}

static bool test_kernel_pd_alloc(void) {
    uint8_t * const S = (uint8_t *)FOS_FREE_AREA_START;
    // Here we will test that mapping stuff into the kernel free area actually works!
    // Also, this kinda tests the addressed version of `pd_alloc_p`.

    // Can't use loss check here becuase a new page table may be allocated and not freed!

    const void *true_e;
    TEST_SUCCESS(alloc_pages(S, S + M_4K, &true_e));
    TEST_EQUAL_HEX(S + M_4K, true_e);

    // We should be able to write to every byte in this page just fine!
    for (size_t i = 0; i < M_4K; i++) {
        S[i] = i;
    }

    free_pages(S, S + M_4K);

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


bool test_page(void) {
    BEGIN_SUITE("Paging");

    RUN_TEST(test_assign_free_page);
    RUN_TEST(test_push_and_pop);
    RUN_TEST(test_new_pt);
    RUN_TEST(test_pt_alloc_and_free);
    RUN_TEST(test_pd_alloc_and_free_p);
    RUN_TEST(test_pd_alloc_entries);
    RUN_TEST(test_kernel_pd_alloc);

    // Remember this halts the cpu.
    //RUN_TEST(test_pd_free_dangerous);
    (void)test_pd_free_dangerous;

    return END_SUITE();
}

