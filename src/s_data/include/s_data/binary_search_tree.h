
#pragma once

#include "s_mem/allocator.h"
#include "s_util/hash.h"

/*
 * I am making an abstract BST interface since I believe there are a few different BST
 * implementations that could fit here.
 *
 * For example: Standard BST, AVL Tree, Red-Black Tree
 *
 * I think these all could use the below interface.
 */

typedef struct _binary_search_tree_t binary_search_tree_t;
typedef struct _binary_search_tree_impl_t binary_search_tree_impl_t;

struct _binary_search_tree_impl_t {
    /*
     * ALL REQUIRED!
     */

    void (*delete_binary_search_tree)(binary_search_tree_t *bst);

    fernos_error_t (*bst_add)(binary_search_tree_t *bst, const void *val);

    void (*bst_remove_node)(binary_search_tree_t *bst, void *node);

    void *(*bst_root)(binary_search_tree_t *bst);

    void *(*bst_left)(binary_search_tree_t *bst, const void *node);
    void *(*bst_right)(binary_search_tree_t *bst, const void *node);
    void *(*bst_parent)(binary_search_tree_t *bst, const void *node);
};

struct _binary_search_tree_t {
    const binary_search_tree_impl_t * const impl;

    const comparator_ft cmp;

    const size_t cell_size;
};

/**
 * Initialize binary search tree super class.
 */
void init_binary_search_tree(binary_search_tree_t *bst, const binary_search_tree_impl_t *impl,
        comparator_ft cmp, size_t cs);

static inline void delete_binary_search_tree(binary_search_tree_t *bst) {
    if (bst) {
        bst->impl->delete_binary_search_tree(bst);
    }
}

/**
 * Copy a given value into a correctly placed new node!
 *
 * Returns FOS_E_SUCCESS on success.
 * Returns FOS_E_ALREADY_ALLOCATED if `val` already exists in the bst.
 * Returns FOS_E_BAD_ARGS if `val` is NULL.
 * Returns FOS_E_UNKNOWN_ERROR if something else goes wrong.
 */
static inline fernos_error_t bst_add(binary_search_tree_t *bst, const void *val) {
    return bst->impl->bst_add(bst, val);
}

/*
 * NOTE: all binary search trees are expected to have stable node pointers.
 *
 * The intention (although not required) is for each value to occupy a node like this:
 *
 *          *---------------*
 *          |  Node Header  |
 *          *---------------*   <--- Node Ptr
 *          |               |
 *          |  Node Value   |
 *          |               |
 *          *---------------*
 *
 * What is in the node header depends on how your binary search tree works, and is up to you!
 * The below functions which return `void *`'s should return node pointers as illustrated above.
 * When I say "stable" I mean, this pointer should be valid and point to the same value until
 * either the bst is deleted or the value is removed.
 *
 * When given a node pointer, you should never modify any part of the node which is used
 * to determine its order! This would basically render the tree useless!
 *
 * For examples about what I mean, you should probably check the tests.
 */

/**
 * Remove the given node from the `bst`.
 *
 * NOTE: After this function call `node` will no longer be useable!!
 * This is a destructor style function.
 */
static inline void bst_remove_node(binary_search_tree_t *bst, void *node) {
    if (node) {
        bst->impl->bst_remove_node(bst, node);
    }
}

/**
 * Returns the node pointer of the trees root.
 *
 * Returns NULL if `bst` is empty.
 */
static inline void *bst_root(binary_search_tree_t *bst) {
    return bst->impl->bst_root(bst);
}

/**
 * Returns the left child of `node`, NULL if there is no left child.
 */
static inline void *bst_left(binary_search_tree_t *bst, const void *node) {
    return bst->impl->bst_left(bst, node);
}

/**
 * Returns the right child of `node`, NULL if there is no right child.
 */
static inline void *bst_right(binary_search_tree_t *bst, const void *node) {
    return bst->impl->bst_right(bst, node);
}

/**
 * Returns the parent of `node`, NULL if there is no parent. (i.e. `node` is the root)
 */
static inline void *bst_parent(binary_search_tree_t *bst, const void *node) {
    return bst->impl->bst_parent(bst, node);
}

/*
 * Simple binary search tree with no automatic balancing.
 */

typedef struct _simple_bst_t simple_bst_t;
typedef struct _simple_bst_header_t simple_bst_header_t;

struct _simple_bst_t {
    binary_search_tree_t super;

    allocator_t * const al;

    simple_bst_header_t *root;
};

struct _simple_bst_header_t {
    simple_bst_header_t *parent;
    simple_bst_header_t *left;
    simple_bst_header_t *right;
};

/**
 * Create a new simple binary search tree.
 *
 * Returns NULL on error.
 */
binary_search_tree_t *new_simple_bst(allocator_t *al, comparator_ft cmp, size_t cs);

static inline binary_search_tree_t *new_da_simple_bst(comparator_ft cmp, size_t cs) {
    return new_simple_bst(get_default_allocator(), cmp, cs);
}

