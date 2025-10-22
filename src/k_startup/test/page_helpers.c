
#include "k_startup/test/page_helpers.h"
#include "k_startup/page.h"
#include "k_startup/page_helpers.h"
#include "k_sys/page.h"

#include "k_startup/vga_cd.h"
#include "k_sys/debug.h"
#include "s_util/err.h"
#include "s_util/constraints.h"
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
    TEST_EQUAL_HEX(FOS_E_SUCCESS, pt_alloc_range(pt, user, s, e, &true_e));

    phys_addr_t old0 = assign_free_page(0, pt);
    
    pt_entry_t *vpt = (pt_entry_t *)(free_kernel_pages[0]);

    for (uint32_t i = s; i < e; i++) {
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

    pt_randomize_alloc(pt, true, 0, 10);
    pt_randomize_alloc(pt, false, 10, 20);

    // Now manually place some shared pages.
    phys_addr_t old0 = assign_free_page(0, pt);

    pt_entry_t *vpt = (pt_entry_t *)(free_kernel_pages[0]);
    for (uint32_t i = 20; i < 100; i++) {
        vpt[i] = fos_identity_pt_entry(i, false, false);
    }

    for (uint32_t i = 200; i < 250; i++) {
        vpt[i] = fos_identity_pt_entry(i, true, true);
    }

    assign_free_page(0, old0);

    // Finally some big alloc at the end.
    pt_randomize_alloc(pt, true, 400, 500);

    // Now copy and compare.

    phys_addr_t pt_copy = copy_page_table(pt);
    TEST_TRUE(NULL_PHYS_ADDR != pt_copy);

    TEST_TRUE(check_equiv_pt(pt, pt_copy));

    delete_page_table(pt);
    delete_page_table(pt_copy);

    TEST_SUCCEED();
}

/**
 * Confirm two page directories have equivelant structure!
 */
static bool check_equiv_pd(phys_addr_t pd0, phys_addr_t pd1) {
    phys_addr_t old0 = assign_free_page(0, pd0);
    phys_addr_t old1 = assign_free_page(1, pd1);

    pt_entry_t *vpd0 = (pt_entry_t *)(free_kernel_pages[0]);
    pt_entry_t *vpd1 = (pt_entry_t *)(free_kernel_pages[1]);

    for (uint32_t i = 0; i < 1024; i++) {
        pt_entry_t pde0 = vpd0[i];
        pt_entry_t pde1 = vpd1[i];

        TEST_EQUAL_UINT(pte_get_present(pde0), pte_get_present(pde1));

        if (pte_get_present(pde0)) {
            // Entries in page directories are always marked USER/UNIQUE
            TEST_EQUAL_UINT(1, pte_get_user(pde0)); 
            TEST_EQUAL_HEX(UNIQUE_ENTRY, pte_get_avail(pde0));

            TEST_EQUAL_UINT(1, pte_get_user(pde1)); 
            TEST_EQUAL_HEX(UNIQUE_ENTRY, pte_get_avail(pde1));

            phys_addr_t base0 = pte_get_base(pde0);
            phys_addr_t base1 = pte_get_base(pde1);

            TEST_TRUE(base0 != base1);

            TEST_TRUE(check_equiv_pt(base0, base1));
        }
    }

    assign_free_page(1, old1);
    assign_free_page(0, old0);

    TEST_SUCCEED();
}

static pt_entry_t copy_pt_entry(phys_addr_t pt) {
    phys_addr_t pt_copy = copy_page_table(pt);

    if (pt_copy == NULL_PHYS_ADDR) {
        return not_present_pt_entry();
    } 

    return fos_unique_pt_entry(pt_copy, true, true);
}

/**
 * Most of the work of copy page directory is done by copy page table.
 *
 * This test will be somewhat simple.
 */
static bool test_copy_page_directory(void) {
    enable_loss_check();

    // For this test, we are going to make two simple but different page tables,
    // then place them at random indeces in the page directory.

    phys_addr_t pd = new_page_directory();
    TEST_TRUE(pd != NULL_PHYS_ADDR);

    phys_addr_t old = assign_free_page(0, pd);
    pt_entry_t *vpd = (pt_entry_t *)(free_kernel_pages[0]);

    phys_addr_t pt_a = new_page_table();
    TEST_TRUE(pt_a != NULL_PHYS_ADDR);
    pt_randomize_alloc(pt_a, true, 0, 1);

    phys_addr_t pt_b = new_page_table();
    TEST_TRUE(pt_b != NULL_PHYS_ADDR);
    pt_randomize_alloc(pt_b, true, 0, 1);

    vpd[0] = copy_pt_entry(pt_a);
    vpd[1] = copy_pt_entry(pt_b);

    for (uint32_t i = 10; i < 20; i++) {
        vpd[i] = copy_pt_entry(pt_a);
    }

    for (uint32_t i = 100; i < 200; i++) {
        vpd[i] = copy_pt_entry((i & 1) ? pt_a : pt_b);
    }

    vpd[1023] = copy_pt_entry(pt_b);

    // These are no longer needed after this point.
    delete_page_table(pt_a);
    delete_page_table(pt_b);

    assign_free_page(0, old);

    phys_addr_t pd_copy = copy_page_directory(pd);

    TEST_TRUE(check_equiv_pd(pd, pd_copy));

    delete_page_directory(pd_copy);
    delete_page_directory(pd);

    TEST_SUCCEED();
}

/**
 * NOTE: This test copies the current page directory!
 */
#define MEM_TEST_AREA_SIZE  (4 * M_4K)
#define MEM_TEST_AREA_START ((uint8_t *)FOS_FREE_AREA_START)
#define MEM_TEST_AREA_END   (MEM_TEST_AREA_START + MEM_TEST_AREA_SIZE)
static bool test_mem_cpy_user(void) {
    const void *true_e;

    phys_addr_t kpd = get_page_directory();
    TEST_EQUAL_HEX(FOS_E_SUCCESS, 
            pd_alloc_pages(kpd, true, MEM_TEST_AREA_START, MEM_TEST_AREA_END, &true_e));

    phys_addr_t upd = copy_page_directory(kpd);
    TEST_TRUE(upd != NULL_PHYS_ADDR);

    // Kernel pages will have lower case abc's.
    for (uint32_t i = 0; i < MEM_TEST_AREA_SIZE; i++) {
        MEM_TEST_AREA_START[i] = 'a' + (i % 26);
    }

    // User pages will have upper case abc's.
    set_page_directory(upd);
    for (uint32_t i = 0; i < MEM_TEST_AREA_SIZE; i++) {
        MEM_TEST_AREA_START[i] = 'A' + (i % 26);
    }

    set_page_directory(kpd);

    typedef struct {
        uint32_t ki; 
        uint32_t ui;
        uint32_t bytes;
    } case_t;

    const case_t CASES[] = {
        (case_t) {
            .ui = 0,
            .ki = 0,
            .bytes = 100
        },
        (case_t) {
            .ui = 0,
            .ki = 10,
            .bytes = 100
        },
        (case_t) {
            .ui = 10,
            .ki = 0,
            .bytes = 100
        },
        (case_t) {
            .ui = M_4K,
            .ki = 0,
            .bytes = M_4K 
        },
        (case_t) {
            .ui = 100,
            .ki = 50,
            .bytes = 3*M_4K 
        },
        (case_t) {
            .ui = 100,
            .ki = 50,
            .bytes = MEM_TEST_AREA_SIZE - 100
        },
        (case_t) {
            .ui = 0,
            .ki = 0,
            .bytes = MEM_TEST_AREA_SIZE
        }
    };
    const uint32_t NUM_CASES = sizeof(CASES) / sizeof(case_t);

    for (uint32_t ci = 0; ci < NUM_CASES; ci++) {
        case_t c = CASES[ci];

        // First we copy in the to user direction.
        
        uint32_t copied;

        TEST_EQUAL_HEX(FOS_E_SUCCESS, mem_cpy_to_user(upd, MEM_TEST_AREA_START + c.ui, 
                    MEM_TEST_AREA_START + c.ki, c.bytes, &copied));
        TEST_EQUAL_UINT(c.bytes, copied);

        // Ok, so was the copy successful. (We look over the whole area btw!)
        set_page_directory(upd);
        for (uint32_t i = 0; i < MEM_TEST_AREA_SIZE; i++) {
            if (c.ui <= i && i < c.ui + c.bytes) {
                uint32_t ki = c.ki + (i - c.ui);
                TEST_EQUAL_UINT('a' + (ki % 26), MEM_TEST_AREA_START[i]);
                MEM_TEST_AREA_START[i] = 'A' + (i % 26);
            } else {
                TEST_EQUAL_UINT('A' + (i % 26), MEM_TEST_AREA_START[i]);
            }
        }

        // Now we copy in the from user direciton!

        set_page_directory(kpd);
        TEST_EQUAL_HEX(FOS_E_SUCCESS, mem_cpy_from_user(MEM_TEST_AREA_START + c.ki, 
                    upd, MEM_TEST_AREA_START + c.ui, c.bytes, &copied));
        TEST_EQUAL_UINT(c.bytes, copied);

        for (uint32_t i = 0; i < MEM_TEST_AREA_SIZE; i++) {
            if (c.ki <= i && i < c.ki + c.bytes) {
                uint32_t ui = c.ui + (i - c.ki);
                TEST_EQUAL_UINT('A' + (ui % 26), MEM_TEST_AREA_START[i]);
                MEM_TEST_AREA_START[i] = 'a' + (i % 26);
            } else {
                TEST_EQUAL_UINT('a' + (i % 26), MEM_TEST_AREA_START[i]);
            }
        }
    }

    pd_free_pages(kpd, MEM_TEST_AREA_START, MEM_TEST_AREA_END);
    delete_page_directory(upd);

    TEST_SUCCEED();
}

/**
 * This tests out mem copying when the source/dest areas may not be fully available!
 */
static bool test_bad_mem_cpy(void) {
    const void *true_e;

    fernos_error_t err;
    uint32_t copied;

    phys_addr_t kpd = get_page_directory();
    TEST_EQUAL_HEX(FOS_E_SUCCESS, 
            pd_alloc_pages(kpd, true, MEM_TEST_AREA_START, MEM_TEST_AREA_END, &true_e));

    phys_addr_t upd = copy_page_directory(kpd);
    TEST_TRUE(upd != NULL_PHYS_ADDR);

    for (uint32_t i = 0; i < MEM_TEST_AREA_SIZE; i++) {
        MEM_TEST_AREA_START[i] = 0xAA;
    }

    set_page_directory(upd);
    for (uint32_t i = 0; i < MEM_TEST_AREA_SIZE; i++) {
        MEM_TEST_AREA_START[i] = 0xBB;
    }
    set_page_directory(kpd);

    err = mem_cpy_to_user(upd, MEM_TEST_AREA_END - M_4K, 
            MEM_TEST_AREA_START, 2 * M_4K, &copied);

    TEST_TRUE(err != FOS_E_SUCCESS);
    TEST_EQUAL_HEX(M_4K, copied);

    set_page_directory(upd);
    for (uint32_t i = MEM_TEST_AREA_SIZE - M_4K; i < MEM_TEST_AREA_SIZE; i++) {
        // CHeck isn't really as rigorous, but whatevs.
        TEST_EQUAL_HEX(0xAA, MEM_TEST_AREA_START[i]);
        MEM_TEST_AREA_START[i] = 0xBB;
    }
    set_page_directory(kpd);

    // Now in the from direction.
    err = mem_cpy_from_user(MEM_TEST_AREA_START, upd, 
            MEM_TEST_AREA_END - M_4K, 2*M_4K, &copied);
    TEST_TRUE(err != FOS_E_SUCCESS);
    TEST_EQUAL_HEX(M_4K, copied);

    for (uint32_t i = 0; i < M_4K; i++) {
        // CHeck isn't really as rigorous, but whatevs.
        TEST_EQUAL_HEX(0xBB, MEM_TEST_AREA_START[i]);
        MEM_TEST_AREA_START[i] = 0xAA;
    }

    delete_page_directory(upd);
    pd_free_pages(kpd, MEM_TEST_AREA_START, MEM_TEST_AREA_END);

    TEST_SUCCEED();
}

static bool test_mem_set_user(void) {
    typedef struct {
        void *start;
        const void *end;
        uint8_t val;
    } case_t;

    const case_t CASES[] = {
        {
            (void *)(MEM_TEST_AREA_START),
            (void *)(MEM_TEST_AREA_START + M_4K),
            0xAB
        },
        {
            (void *)(MEM_TEST_AREA_START + 13),
            (void *)(MEM_TEST_AREA_START + M_4K),
            0xAC
        },
        {
            (void *)(MEM_TEST_AREA_START + 13),
            (void *)(MEM_TEST_AREA_START + M_4K + 15),
            0xAD
        },
        {
            (void *)(MEM_TEST_AREA_START + 13),
            (void *)(MEM_TEST_AREA_START + (2*M_4K) + 15),
            0xCC
        },
        {
            (void *)(MEM_TEST_AREA_START),
            (void *)(MEM_TEST_AREA_START + (2*M_4K)),
            0xCB
        },
        {
            (void *)(MEM_TEST_AREA_START + 24),
            (void *)(MEM_TEST_AREA_START + (2*M_4K)),
            0xA0
        },
        {
            (void *)(MEM_TEST_AREA_START),
            (void *)(MEM_TEST_AREA_START),
            0xA1
        }
    };
    const size_t NUM_CASES = sizeof(CASES) / sizeof(CASES[0]);

    const void *true_e;

    // Like the mem_cpy tests above, this test will switch between memory spaces to confirm
    // mem_set's work as expected. This type of test only works when the kernel stack is shared
    // between the two spaces.

    phys_addr_t kpd = get_page_directory();

    phys_addr_t upd = copy_page_directory(kpd);
    TEST_TRUE(upd != NULL_PHYS_ADDR);

    TEST_SUCCESS(pd_alloc_pages(upd, true, (void *)MEM_TEST_AREA_START, (const void *)MEM_TEST_AREA_END,
                &true_e));


    for (size_t i = 0; i < NUM_CASES; i++) {
        case_t c = CASES[i];

        TEST_TRUE(c.start <= c.end);
        const size_t bytes = (uintptr_t)c.end - (uintptr_t)c.start;

        uint32_t bytes_set;
        TEST_SUCCESS(mem_set_to_user(upd, c.start, c.val, bytes, &bytes_set));

        set_page_directory(upd);
        TEST_TRUE(mem_chk(c.start, c.val, bytes));
        set_page_directory(kpd);

        TEST_EQUAL_UINT(bytes, bytes_set);
    }

    delete_page_directory(upd);

    TEST_SUCCEED();
}

static bool test_bad_mem_set(void) {
    typedef struct {
        // The start and end of our mem_set.
        void *start;
        const void *end;

        // Where we expect the mem_set'ing to stop.
        const void *real_end;
        uint8_t val;
    } case_t;

    const case_t CASES[] = {
        {
            (void *)(MEM_TEST_AREA_END),
            (const void *)(MEM_TEST_AREA_END + 10),

            (const void *)(MEM_TEST_AREA_END),
            0xAA
        },
        {
            (void *)(MEM_TEST_AREA_END - 10),
            (const void *)(MEM_TEST_AREA_END + 10),

            (const void *)(MEM_TEST_AREA_END),
            0xAB
        },
        {
            (void *)(MEM_TEST_AREA_END - M_4K),
            (const void *)(MEM_TEST_AREA_END + M_4K),

            (const void *)(MEM_TEST_AREA_END),
            0xAC
        },
        {
            (void *)(MEM_TEST_AREA_END - M_4K - 10),
            (const void *)(MEM_TEST_AREA_END + M_4K),

            (const void *)(MEM_TEST_AREA_END),
            0xAD
        }
    };
    const size_t NUM_CASES = sizeof(CASES) / sizeof(CASES[0]);

    phys_addr_t kpd = get_page_directory();

    phys_addr_t upd = copy_page_directory(kpd);
    TEST_TRUE(upd != NULL_PHYS_ADDR);

    const void *true_e;
    TEST_SUCCESS(pd_alloc_pages(upd, true, (void *)MEM_TEST_AREA_START, (const void *)MEM_TEST_AREA_END,
                &true_e));

    for (size_t i = 0; i < NUM_CASES; i++) {
        case_t c = CASES[i];

        TEST_TRUE(c.start <= c.end);
        TEST_TRUE(c.start <= c.real_end);

        const size_t bytes_to_set = (uint32_t)c.end - (uint32_t)c.start;
        const size_t exp_bytes_set = (uint32_t)c.real_end - (uint32_t)c.start;

        uint32_t bytes_set;
        TEST_FAILURE(mem_set_to_user(upd, c.start, c.val, bytes_to_set, &bytes_set));
        TEST_EQUAL_UINT(exp_bytes_set, bytes_set);

        set_page_directory(upd);
        TEST_TRUE(mem_chk(c.start, c.val, bytes_set));
        set_page_directory(kpd);
    }

    delete_page_directory(upd);

    TEST_SUCCEED();
}

static bool test_new_user_app_pd(void) {
    enable_loss_check();

    // Because the page directory being created here is intended for a user process,
    // we won't switch to it in this test like we do in the above tests. In theory, such
    // a move could still work as the kernel stack is in all page directoreis, but probs not
    // worth the risk.

    // NOTE: because these tests assume no heap, everything for this test is allocated on the
    // kernel stack. We'll just loosely require the kernel stack to be pretty large before
    // running this test.

    TEST_TRUE(FOS_KERNEL_STACK_SIZE >= (M_4K * 24));

    user_app_t ua = {
        .al = NULL, // These tests cannot depend on a heap being set up.
        .entry = (const void *)FOS_APP_AREA_START,
        .areas = {
            (user_app_area_entry_t) {
                .occupied = true,
                .load_position = (void *)FOS_APP_AREA_START,
                .area_size = M_4K,
                .given = NULL,
                .given_size = 0,
                .writeable = false
            },
            (user_app_area_entry_t) {
                .occupied = true,
                .load_position = (void *)(FOS_APP_AREA_START + M_4K),
                .area_size = 16,
                .given = "hello",
                .given_size = 6,
                .writeable = false
            },
            (user_app_area_entry_t) {
                .occupied = true,
                .load_position = (void *)(FOS_APP_AREA_START + (3 * M_4K)),
                .area_size = 8,
                .given = "goodbye",
                .given_size = 8,
                .writeable = false
            } ,
            (user_app_area_entry_t) {
                .occupied = true,
                .load_position = (void *)(FOS_APP_AREA_START + (5 * M_4K)),
                .area_size = (2 * M_4K) + 10,
                .given = NULL,
                .given_size = 0,
                .writeable = false
            } 
        }
    };

    // Remember, the args block is just copied directly into the page directory.
    // For this test, it doesn't need actual string args.
    uint8_t mock_args_block[(2 * M_4K) + 5];
    const size_t mock_args_block_size = sizeof(mock_args_block);
    for (size_t i = 0; i < mock_args_block_size; i++) {
        mock_args_block[i] = (uint8_t)i;
    }

    phys_addr_t upd;
    TEST_SUCCESS(new_user_app_pd(&ua, mock_args_block, mock_args_block_size, &upd));


    // This is hard on the kernel stack
    // The size of this array MUST be larger than any single area described in the user app above!
    uint8_t dummy_pages[8 * M_4K];

    for (size_t i = 0; i < FOS_MAX_APP_AREAS; i++) {
        const user_app_area_entry_t *uae = ua.areas + i;
        if (!(uae->occupied)) {
            continue;
        }

        TEST_SUCCESS(mem_cpy_from_user(dummy_pages, upd, uae->load_position, 
                    uae->area_size, NULL));

        TEST_TRUE(mem_cmp(dummy_pages, uae->given, uae->given_size));
        TEST_TRUE(mem_chk(dummy_pages + uae->given_size, 0, 
                    uae->area_size - uae->given_size));
    }

    // Now check args block!
    TEST_SUCCESS(mem_cpy_from_user(dummy_pages, upd, (const void *)FOS_APP_ARGS_AREA_START, 
                mock_args_block_size, NULL));
    TEST_TRUE(mem_cmp(mock_args_block, dummy_pages, mock_args_block_size));

    delete_page_directory(upd);

    // Oh, let's make sure a NULL args block works.
    TEST_SUCCESS(new_user_app_pd(&ua, NULL, 0, &upd));
    delete_page_directory(upd);

    // Ok, now let's try fucking up the original user app a bit.
    // expecting errors!

    // Bad entry point.
    const void *og_entry = ua.entry;
    ua.entry = (const void *)(FOS_APP_AREA_START + (10 * M_4K));  
    TEST_FAILURE(new_user_app_pd(&ua, mock_args_block, mock_args_block_size, &upd));
    ua.entry = og_entry;

    // Areas too large.
    uint32_t og_size = ua.areas[0].area_size;
    ua.areas[0].area_size = FOS_APP_AREA_SIZE + M_4K;
    TEST_FAILURE(new_user_app_pd(&ua, mock_args_block, mock_args_block_size, &upd));
    ua.areas[0].area_size = og_size;

    // Overlapping areas.
    og_size = ua.areas[0].area_size;
    ua.areas[0].area_size = 10 * M_4K;
    TEST_FAILURE(new_user_app_pd(&ua, mock_args_block, mock_args_block_size, &upd));
    ua.areas[0].area_size = og_size;

    // Args block too large!
    TEST_FAILURE(new_user_app_pd(&ua, mock_args_block, FOS_APP_ARGS_AREA_SIZE + M_4K, &upd));

    TEST_SUCCEED();
}

bool test_page_helpers(void) {
    BEGIN_SUITE("Page Helpers");

    RUN_TEST(test_page_copy);
    RUN_TEST(test_copy_page_table);
    RUN_TEST(test_copy_page_directory);
    RUN_TEST(test_mem_cpy_user);
    RUN_TEST(test_bad_mem_cpy);
    RUN_TEST(test_mem_set_user);
    RUN_TEST(test_bad_mem_set);
    RUN_TEST(test_new_user_app_pd);

    return END_SUITE();
}
