
#include "s_data/binary_search_tree.h"
#include "s_data/test/binary_search_tree.h"
#include "k_startup/gfx.h"

static bool pretest(void);
static bool posttest(void);

#define PRETEST() pretest()
#define POSTTEST() posttest()

#define LOGF_METHOD(...) gfx_direct_put_fmt_s_rr(__VA_ARGS__)

#include "s_util/test.h"

typedef binary_search_tree_t *(*bst_gen_ft)(comparator_ft cmp, size_t cs);

static size_t _num_al_blocks;
static bst_gen_ft _bst_generator = NULL;


static bool pretest(void) {
    _num_al_blocks = al_num_user_blocks(get_default_allocator());

    TEST_SUCCEED();
}

static bool posttest(void) {
    TEST_EQUAL_HEX(_num_al_blocks, al_num_user_blocks(get_default_allocator()));

    TEST_SUCCEED();
}

/**
 * Return true if `node` is the root of a valid binary search tree!
 * `node` must be non-NULL.
 *
 * If `lb` is given, all nodes must be greater than `lb`.
 * If `ub` is given, all nodes must be less than `ub`.
 * (i.e. lower bound and upper bound)
 *
 * If `eles` is given, adds 1 for each element traversed.
 */
static bool test_confirm_bst(binary_search_tree_t *bst, const void *node, const void *lb, const void *ub, size_t *eles) {
    if (lb) {
        TEST_TRUE(bst->cmp(lb, node) < 0);
    }

    if (ub) {
        TEST_TRUE(bst->cmp(node, ub) < 0);
    }

    if (bst_left(bst, node)) {
        TEST_EQUAL_HEX(node, bst_parent(bst, bst_left(bst, node)));
        TEST_TRUE(test_confirm_bst(bst, bst_left(bst, node), lb, node, eles));
    }

    if (bst_right(bst, node)) {
        TEST_EQUAL_HEX(node, bst_parent(bst, bst_right(bst, node)));
        TEST_TRUE(test_confirm_bst(bst, bst_right(bst, node), node, ub, eles));
    }

    if (eles) {
        *eles += 1;
    }

    TEST_SUCCEED();
}

/**
 * Return true if and only if `ele` appears in the binary search tree rooted at `node`.
 */
static bool test_confirm_existence(binary_search_tree_t *bst, const void *node, const void *ele) {
    if (!node) {
        TEST_FAIL();
    }

    int32_t c = bst->cmp(ele, node);

    if (c < 0) { // ele < node, go left
        TEST_TRUE(test_confirm_existence(bst, bst_left(bst, node), ele));     
    } else if (c > 0) { // go right.
        TEST_TRUE(test_confirm_existence(bst, bst_right(bst, node), ele));     
    }

    TEST_SUCCEED();
}

static int32_t test_cmp_int32(const void *k0, const void *k1) {
    const int32_t i0 = *(const int32_t *)k0;
    const int32_t i1 = *(const int32_t *)k1;

    if (i0 == i1) {
        return 0;
    }

    return i0 < i1 ? -1 : 1;
}

static bool test_new_and_delete(void) {
    binary_search_tree_t *bst = _bst_generator(test_cmp_int32, sizeof(int32_t));
    TEST_TRUE(bst != NULL);
    delete_binary_search_tree(bst);

    TEST_SUCCEED();
}

static bool test_int_bst_simple(void) {
    binary_search_tree_t *bst = _bst_generator(test_cmp_int32, sizeof(int32_t));
    TEST_TRUE(bst != NULL);

    // Here we'll just test adding to a tree and confirming it's valid.

    const int32_t eles[] = {
        1, 6, 2, -1, 0 ,10, 5, 3, 11, 7, -3, -4,
        -50, -45, -44, 33
    };
    const size_t num_eles = sizeof(eles) / sizeof(eles[0]);

    for (size_t i = 0; i < num_eles; i++) {
        TEST_SUCCESS(bst_add(bst, eles + i));
    }

    TEST_TRUE(bst_root(bst) != NULL);

    size_t act_eles = 0;
    TEST_TRUE(test_confirm_bst(bst, bst_root(bst), NULL, NULL, &act_eles));
    TEST_EQUAL_UINT(num_eles, act_eles);

    delete_binary_search_tree(bst);

    TEST_SUCCEED();
}

static bool test_generic_bst(const char *suite_name, bst_gen_ft gen) {
    _bst_generator = gen;
    BEGIN_SUITE(suite_name);
    RUN_TEST(test_new_and_delete);
    RUN_TEST(test_int_bst_simple);
    return END_SUITE();
}

bool test_simple_bst(void) {
    return test_generic_bst("Simple BST", new_da_simple_bst);
}
