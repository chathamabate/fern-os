
#include "s_data/binary_search_tree.h"

void init_binary_search_tree(binary_search_tree_t *bst, const binary_search_tree_impl_t *impl,
        comparator_ft cmp, size_t cs) {
    *(const binary_search_tree_impl_t **)&(bst->impl) = impl;
    *(comparator_ft *)&(bst->cmp) = cmp;
    *(size_t *)&(bst->cell_size) = cs;
}

static void delete_sbst(binary_search_tree_t *bst);
static fernos_error_t sbst_add(binary_search_tree_t *bst, const void *val);
static void sbst_remove_node(binary_search_tree_t *bst, void *node);
static void *sbst_root(binary_search_tree_t *bst);
static void *sbst_left(binary_search_tree_t *bst, const void *node);
static void *sbst_right(binary_search_tree_t *bst, const void *node);
static void *sbst_parent(binary_search_tree_t *bst, const void *node);

static const binary_search_tree_impl_t SBST_IMPL = {
    .delete_binary_search_tree = delete_sbst,
    .bst_add = sbst_add,
    .bst_remove_node = sbst_remove_node,
    .bst_root = sbst_root,
    .bst_right = sbst_right,
    .bst_left = sbst_left,
    .bst_parent = sbst_parent
};

binary_search_tree_t *new_simple_bst(allocator_t *al, comparator_ft cmp, size_t cs) {
    if (!al || !cmp || cs == 0) {
        return NULL;
    }

    simple_bst_t *sbst = al_malloc(al, sizeof(simple_bst_t));
    if (!sbst) {
        return NULL;
    }

    init_binary_search_tree((binary_search_tree_t *)sbst, &SBST_IMPL, cmp, cs);
    *(allocator_t **)&(sbst->al) = al;
    sbst->root = NULL;

    return (binary_search_tree_t *)sbst;
}

static void *sbst_hdr_to_val(const simple_bst_header_t *hdr) {
    return (void *)(hdr + 1);
}

static simple_bst_header_t *val_to_sbst_hdr(const void *val) {
    return (simple_bst_header_t *)val - 1;
}

static void delete_sbst(binary_search_tree_t *bst) {
    simple_bst_t *sbst = (simple_bst_t *)bst;

    // Really doubling down on no recursion here.

    simple_bst_header_t *iter = sbst->root;

    while (iter) {
        if (!(iter->left) && !(iter->right)) { // Leaf, delete!
            if (iter->parent) {
                if (iter->parent->left == iter) {
                    iter->parent->left = NULL;
                } else {
                    iter->parent->right = NULL;
                }
            } else {
                sbst->root = NULL;
            }

            iter = iter->parent;
            
            al_free(sbst->al, iter);
        } else if (iter->left) { // left or right, just go left.
            iter = iter->left;
        } else { // just right, go right.
            iter = iter->right;
        }
    }

    al_free(sbst->al, sbst);
}

/**
 * Helper for creating a node in a simple bst.
 *
 * `p` will be modified to have this new node as a child!
 * Use `left_child` to mark which child to become.
 *
 * If `p` is NULL, this new node will become the root of the `bst`
 *
 * Returns FOS_E_STATE_MISMATCH if the tree is in an unexpected state.
 * Returns FOS_E_SUCCESS on success.
 * Returns FOS_E_NO_MEM if allocation fails.
 */
static fernos_error_t new_sbst_leaf_node(simple_bst_t *sbst, const void *val, simple_bst_header_t *p, bool left_child) {
    if (p) {
        if (left_child && p->left) { // Already a left child.
            return FOS_E_STATE_MISMATCH;
        }

        if (!left_child && p->right) { // Already a right child.
            return FOS_E_STATE_MISMATCH;
        }
    } else if (sbst->root) { // Already a root.
        return FOS_E_STATE_MISMATCH;
    }

    simple_bst_header_t *hdr = al_malloc(sbst->al, sizeof(simple_bst_header_t) + sbst->super.cell_size);
    if (!hdr) {
        return FOS_E_NO_MEM;
    }
    
    void *node_val = sbst_hdr_to_val(hdr);
    mem_cpy(node_val, val, sbst->super.cell_size);

    hdr->parent = p;
    hdr->left = NULL;
    hdr->right = NULL;

    if (p) {
        if (left_child) {
            p->left = hdr;
        } else {
            p->right = hdr;
        }
    } else {
        sbst->root = p;
    }

    return FOS_E_SUCCESS;
}

static fernos_error_t sbst_add(binary_search_tree_t *bst, const void *val) {
    fernos_error_t err;
    simple_bst_t *sbst = (simple_bst_t *)bst;

    if (!val) {
        return FOS_E_BAD_ARGS;
    }

    if (!(sbst->root)) { // If tree is empty, make a new node, set it as root.
        err = new_sbst_leaf_node(sbst, val, NULL, false);
        if (err != FOS_E_SUCCESS) {
            return FOS_E_UNKNWON_ERROR;
        }

        return FOS_E_SUCCESS;
    }

    // Tree is not empty, we go until we find NULL or a match.

    simple_bst_header_t *iter = sbst->root;
    while (true) {
        int32_t cval =  sbst->super.cmp(val, sbst_hdr_to_val(iter));
        if (cval == 0) { // Already in the tree!
            return FOS_E_ALREADY_ALLOCATED;
        }

        if (cval < 0) {
            if (!(iter->left)) {
                err = new_sbst_leaf_node(sbst, val, iter, true);
                if (err != FOS_E_SUCCESS) {
                    return FOS_E_UNKNWON_ERROR;
                }

                return FOS_E_SUCCESS;
            }

            iter = iter->left;
        } else { // go to the right.
            if (!(iter->right)) {
                err = new_sbst_leaf_node(sbst, val, iter, false);
                if (err != FOS_E_SUCCESS) {
                    return FOS_E_UNKNWON_ERROR;
                }

                return FOS_E_SUCCESS;
            }

            iter = iter->right;
        }
    }
}

/**
 * `root` should be a detatched non-empty subtree inside of some larger sbst.
 * (Meaning, `root->parent` should be set to NULL before calling this function!)
 *
 * All pointers below are assumed to be non-null.
 *
 * On completion, the minimum (or leftmost) node is take removed from it's position, then 
 * placed as the root of the given subtree. with all other nodes as right children.
 *
 * For example:
 *
 *          10                    5
 *       5      11      ===>          10
 *         6       12               6     11
 *                                            12
 *
 * It's gauranteed that what is returned and has no parent and no left sub children!
 */
static simple_bst_header_t *sbst_pick_minimum(simple_bst_header_t *root) {
    // If the given subtree already has the minimum rooted, just exit early.
    if (!(root->left)) {
        return root;
    }

    // First, let's find leftmost.  
    simple_bst_header_t *leftmost = root;
    while (leftmost->left) {
        leftmost = leftmost->left;
    }

    // Leftmost != root gauranteed

    // Next detatch leftmost.
    simple_bst_header_t *parent = leftmost->parent;
    simple_bst_header_t *right = leftmost->right;

    leftmost->parent = NULL;
    leftmost->right = root;
    root->parent = leftmost; // root->parent should be NULL when entering this function.

    // Patch hole.

    // Parent gauranteed non-NULL.
    parent->left = right;
    if (right) {
        right->parent = parent;
    }

    return leftmost;
}

static void sbst_remove_node(binary_search_tree_t *bst, void *node) {
    simple_bst_t *sbst = (simple_bst_t *)bst;

    simple_bst_header_t *node_hdr = val_to_sbst_hdr(node);

    // This is the node which will be put in `node`'s place.
    simple_bst_header_t *sub = NULL;

    // If there is a right child, we always search the right sub tree for a replacement.
    // (i.e. the leftmost node in the right subtree)
    if (node_hdr->right) { 
        node_hdr->right->parent = NULL; // Detach right subtree and pick its minimum.
        simple_bst_header_t *sub = sbst_pick_minimum(node_hdr->right);

        // Stich in old left subtree.
        sub->left = node_hdr->left;
        if (sub->left) {
            sub->left->parent = sub;
        }
    } else if (node_hdr->left) {
        // left, but no right, just detach from the parent for now.
        node_hdr->left->parent = NULL;
        sub = node_hdr->left;
    }  // otherwise no kids at all!

    // A this point, it is gauranteed that sub->parent = NULL.
    // (Or `sub` itself is NULL)

    if (node_hdr->parent) {
        sub->parent = node_hdr->parent;
        if (node_hdr->parent->left == node_hdr) {
            node_hdr->parent->left = sub;
        } else {
            node_hdr->parent->right = sub;
        }
    } else {
        sbst->root = sub; 
    }

    // Finally, `node_hdr` is not referenced at all in the tree!
    // It can be deleted!

    al_free(sbst->al, node_hdr); 
}

static void *sbst_root(binary_search_tree_t *bst) {
    simple_bst_t *sbst = (simple_bst_t *)bst;

    if (sbst->root) {
        return sbst_hdr_to_val(sbst->root);
    }

    return NULL;
}

static void *sbst_left(binary_search_tree_t *bst, const void *node) {
    (void)bst;

    simple_bst_header_t *hdr = val_to_sbst_hdr(node);

    if (hdr->left) {
        return sbst_hdr_to_val(hdr->left);
    }

    return NULL;
}

static void *sbst_right(binary_search_tree_t *bst, const void *node) {
    (void)bst;

    simple_bst_header_t *hdr = val_to_sbst_hdr(node);

    if (hdr->right) {
        return sbst_hdr_to_val(hdr->right);
    }

    return NULL;
}

static void *sbst_parent(binary_search_tree_t *bst, const void *node) {
    (void)bst;

    simple_bst_header_t *hdr = val_to_sbst_hdr(node);

    if (hdr->parent) {
        return sbst_hdr_to_val(hdr->parent);
    }

    return NULL;
}
