
#include "s_data/binary_search_tree.h"
#include "s_data/test/binary_search_tree.h"
#include "s_util/rand.h"
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

static bool test_int32_bst_add_and_find(void) {
    binary_search_tree_t *bst = _bst_generator(test_cmp_int32, sizeof(int32_t));
    TEST_TRUE(bst != NULL);

    // Here we add some static elements, confirm the tree is valid,
    // also confirm that all values can be found.

    const int32_t eles[] = {
        1, 6, 2, -1, 0 ,10, 5, 3, 11, 7, -3, -4,
        -50, -45, -44, 33
    };
    const size_t num_eles = sizeof(eles) / sizeof(eles[0]);

    for (size_t i = 0; i < num_eles; i++) {
        TEST_SUCCESS(bst_add(bst, eles + i));
    }

    TEST_TRUE(bst_root(bst) != NULL);

    // Let's confirm our BST is correctly 
    size_t act_eles = 0;
    TEST_TRUE(test_confirm_bst(bst, bst_root(bst), NULL, NULL, &act_eles));
    TEST_EQUAL_UINT(num_eles, act_eles);

    // Now try find to find all elements.
    for (size_t i = 0; i < num_eles; i++) {
        TEST_TRUE(bst_find(bst, eles + i) != NULL);
    }

    // FInally, confirm readding fails.
    for (size_t i = 0; i < num_eles; i++) {
        TEST_EQUAL_HEX(FOS_E_ALREADY_ALLOCATED, bst_add(bst, eles + i));
    }

    delete_binary_search_tree(bst);

    TEST_SUCCEED();
}

static int32_t test_cmp_uint64(const void *k0, const void *k1) {
    const uint64_t i0 = *(const uint64_t *)k0;
    const uint64_t i1 = *(const uint64_t *)k1;

    if (i0 == i1) {
        return 0;
    }

    return i0 < i1 ? -1 : 1;
}

static bool test_uint64_bst_add_and_remove(void) {
    binary_search_tree_t *bst = _bst_generator(test_cmp_uint64, sizeof(uint64_t));
    TEST_TRUE(bst != NULL);

    uint64_t eles[] = {
        456, 123, 3999, UINT64_MAX, 444, 1, 0, 
        12, 13, 65, 100, 10, 9, 8
    };
    const size_t num_eles = sizeof(eles) / sizeof(eles[0]);

    // Add all elements.
    for (size_t i = 0; i < num_eles; i++) {
        TEST_SUCCESS(bst_add(bst, eles + i));
    }

    // Remember node pointers should be stable!
    uint64_t *nodes[sizeof(eles) / sizeof(eles[0])];
    for (size_t i = 0; i < num_eles; i++) {
        nodes[i] = bst_find(bst, eles + i);
        TEST_TRUE(nodes[i] != NULL);
    }

    // Remove every other element.
    for (size_t i = 0; i < num_eles; i += 2) {
        TEST_TRUE(bst_remove(bst, eles + i));
        nodes[i] = NULL;
    }

    // Confirm our tree is still valid.
    TEST_TRUE(test_confirm_bst(bst, bst_root(bst), NULL, NULL, NULL));

    // Let's find is still working. (removed elements should not be findable!
    for (size_t i = 0; i < num_eles; i += 2) {
        TEST_EQUAL_HEX(NULL, bst_find(bst, eles + i));
    }

    // Now try adding back in some more elements.
    for (size_t i = 0; i < num_eles; i += 2) {
        eles[i] = 4000 + i; // Shouldn't be any duplicates.
        TEST_SUCCESS(bst_add(bst, eles + i));

        // Store new nodes in the pointer array.
        nodes[i] = bst_find(bst, eles + i);
        TEST_TRUE(nodes[i] != NULL);
    }

    // Confirm our tree is still valid.
    TEST_TRUE(test_confirm_bst(bst, bst_root(bst), NULL, NULL, NULL));

    // Finally, confirm values and stable pointers!
    for (size_t i = 0; i < num_eles; i++) {
        uint64_t *node = bst_find(bst, eles + i);
        TEST_EQUAL_HEX(nodes[i], node);
    }

    delete_binary_search_tree(bst);
    TEST_SUCCEED();
}

static int32_t test_cmp_uint8(const void *k0, const void *k1) {
    const uint8_t i0 = *(const uint8_t *)k0;
    const uint8_t i1 = *(const uint8_t *)k1;

    if (i0 == i1) {
        return 0;
    }

    return i0 < i1 ? -1 : 1;
}

static bool test_uint8_bst_nav(void) {
    binary_search_tree_t *bst = _bst_generator(test_cmp_uint8, sizeof(uint8_t));
    TEST_TRUE(bst != NULL);

    // Here we will test min, max, next, and prev helpers.

    TEST_EQUAL_HEX(NULL, bst_min(bst));
    TEST_EQUAL_HEX(NULL, bst_max(bst));
    TEST_EQUAL_HEX(NULL, bst_root(bst));

    uint8_t eles[] = { 
        6, 5, 4, 0, 12, 11, 40, 35,
        66, 77, 43, 1, 3, 90, 101, 17 
    };
    const size_t num_eles = sizeof(eles) / sizeof(eles[0]);

    // Add all elements.
    for (size_t i = 0; i < num_eles; i++) {
        TEST_SUCCESS(bst_add(bst, eles + i));
    }

    // Now, sneaky bubble sort for the boys back home.
    for (size_t i = 0; i < num_eles; i++) {
        uint8_t min_ind = i;
        for (size_t j = i; j < num_eles; j++) {
            if (eles[j] < eles[min_ind]) {
                min_ind = j;
            }
        }
        
        uint8_t temp = eles[i];
        eles[i] = eles[min_ind];
        eles[min_ind] = temp;
    }

    uint8_t *min_node = bst_min(bst);
    TEST_TRUE(min_node != NULL);
    TEST_EQUAL_HEX(NULL, bst_prev(bst, min_node));

    uint8_t *max_node = bst_max(bst);
    TEST_TRUE(max_node != NULL);
    TEST_EQUAL_HEX(NULL, bst_next(bst, max_node));

    // Left to right please!

    uint8_t *node = min_node;
    size_t node_i = 0;

    do {
        TEST_EQUAL_UINT(eles[node_i], *node);
        node = bst_next(bst, node);
        node_i++;
    } while (node && node_i < num_eles);
    TEST_EQUAL_HEX(NULL, node);
    TEST_EQUAL_UINT(num_eles, node_i);

    // Right to left!

    node = max_node;
    node_i = num_eles - 1;
    do {
        TEST_EQUAL_UINT(eles[node_i], *node);
        node = bst_prev(bst, node);
        node_i--;
    } while (node && node_i < num_eles); // `node_i < num_eles` still works here!
    TEST_EQUAL_HEX(NULL, node);
    TEST_EQUAL_UINT(SIZE_MAX, node_i);

    delete_binary_search_tree(bst);
    TEST_SUCCEED();
}

static int32_t test_cmp_uint32(const void *k0, const void *k1) {
    const uint32_t v0 = *(uint32_t *)k0; 
    const uint32_t v1 = *(uint32_t *)k1;

    if (v0 < v1) {
        return -1;
    }

    if (v0 > v1) {
        return 1;
    }

    return 0;
}

static bool test_uint32_bst_big_random(void) {
    binary_search_tree_t *bst = _bst_generator(test_cmp_uint32, sizeof(uint32_t));
    TEST_TRUE(bst != NULL);

    bool found[100];
    const size_t found_size = sizeof(found) / sizeof(found[0]);
    for (size_t i = 0; i < found_size; i++) {
        found[i] = false; // Kinda OCD false setting, but whatever.
    }

    rand_t r = rand(0);
    for (size_t c = 0; c < 100 * found_size; c++) {
        uint8_t remove = next_rand_u1(&r);
        uint32_t val = next_rand_u32(&r) % found_size;

        if (remove) {
            if (found[val]) {
                TEST_TRUE(bst_remove(bst, &val));
                found[val] = false;
            } else {
                TEST_FALSE(bst_remove(bst, &val));
            }
        } else {
            if (found[val]) {
                TEST_EQUAL_HEX(FOS_E_ALREADY_ALLOCATED, bst_add(bst, &val));
            } else {
                TEST_SUCCESS(bst_add(bst, &val));
                found[val] = true;
            }
        }

        if (c % 10 == 0 && bst_root(bst)) {
            test_confirm_bst(bst, bst_root(bst), NULL, NULL, NULL);
        }
    }

    // I guess test navigation one final time?
    const uint32_t *node = bst_min(bst);

    for (uint32_t i = 0; i < found_size; i++) {
        if (found[i]) {
            TEST_TRUE(node != NULL);
            TEST_EQUAL_UINT(i, *node);
            node = bst_next(bst, node);
        }
    }

    TEST_EQUAL_HEX(NULL, node);

    delete_binary_search_tree(bst);
    TEST_SUCCEED();
}


/**
 * When I originally designed this, I thought there would be one suite which just tests the 
 * virtual functions of a BST, then another suite which tests just the default helpers.
 *
 * The idea being that the virtual function suite would confirm a BST concrete subclass is valid.
 * The helper suite would confirm the BST helpers were written correctly given a valid BST.
 *
 * Anyway, this design would be cool, but ultimately was kinda annoying as the virtual functions
 * are pretty bare bones.
 *
 * So, just one test suite!
 */
static bool test_generic_bst(const char *suite_name, bst_gen_ft gen) {
    _bst_generator = gen;
    BEGIN_SUITE(suite_name);
    RUN_TEST(test_new_and_delete);
    RUN_TEST(test_int32_bst_add_and_find);
    RUN_TEST(test_uint64_bst_add_and_remove);
    RUN_TEST(test_uint8_bst_nav);
    RUN_TEST(test_uint32_bst_big_random);
    return END_SUITE();
}

bool test_simple_bst(void) {
    return test_generic_bst("Simple BST", new_da_simple_bst);
}
