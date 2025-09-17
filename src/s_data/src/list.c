
#include "s_data/list.h"
#include "s_util/err.h"
#include "s_util/str.h"

/*
 * Default function implementations
 */

fernos_error_t l_push_back_all(list_t *l, list_t *s) {
    fernos_error_t err;

    if (!s) {
        return FOS_E_BAD_ARGS;
    }

    l_reset_iter(s);
    for (void *iter = l_get_iter(s); iter; iter = l_next_iter(s)) {
        err = l_push_back(l, iter);
        if (err != FOS_E_SUCCESS) {
            return err;
        }
    }

    l_clear(s);
    
    return FOS_E_SUCCESS;
}

bool l_pop_ele(list_t *l, const void *ele, bool (*cmp)(const void *, const void *), bool all) {
    if (!ele || !cmp) {
        return false;
    }

    bool removed = false;

    l_reset_iter(l);
    void *iter = l_get_iter(l);
    while (iter) {
        if (cmp(ele, iter)) {
            l_pop_iter(l, NULL);  // should never error since iter is non-null.
            removed = true;

            if (!all) {
                break;
            }

            iter = l_get_iter(l);
        } else {
            iter = l_next_iter(l);
        }
    }

    return removed;
}

static void delete_linked_list(list_t *l);
static void ll_clear(list_t *l);
static size_t ll_get_cell_size(list_t *l);
static size_t ll_get_len(list_t *l);
static void *ll_get_ptr(list_t *l, size_t i);
static fernos_error_t ll_push(list_t *l, size_t i, const void *src);
static fernos_error_t ll_pop(list_t *l, size_t i, void *dest);
static void ll_reset_iter(list_t *l);
static void *ll_get_iter(list_t *l);
static void *ll_next_iter(list_t *l);
static fernos_error_t ll_push_before_iter(list_t *l, const void *src);
static fernos_error_t ll_push_after_iter(list_t *l, const void *src);
static fernos_error_t ll_pop_iter(list_t *l, void *dest);
static void ll_dump(list_t *l, void (*pf)(const char *fmt, ...));

static const list_impl_t LL_IMPL = {
    .delete_list = delete_linked_list,
    .l_clear = ll_clear,
    .l_get_cell_size = ll_get_cell_size,
    .l_get_len = ll_get_len,
    .l_get_ptr = ll_get_ptr,
    .l_push = ll_push,
    .l_pop = ll_pop,
    .l_reset_iter = ll_reset_iter,
    .l_get_iter = ll_get_iter,
    .l_next_iter = ll_next_iter,
    .l_push_before_iter = ll_push_before_iter,
    .l_push_after_iter = ll_push_after_iter,
    .l_pop_iter = ll_pop_iter,
    .l_dump = ll_dump,
};

list_t *new_linked_list(allocator_t *al, size_t cs) {
    if (!al || cs == 0) {
        return NULL;
    }

    linked_list_t *ll = al_malloc(al, sizeof(linked_list_t));

    if (!ll) {
        return NULL;
    }

    *(const list_impl_t **)&(ll->super.impl) = &LL_IMPL;

    ll->al = al;
    ll->len = 0;
    ll->cs = cs;

    ll->first = NULL;
    ll->last = NULL;

    ll->iter = NULL;

    return (list_t *)ll;
}

static void delete_linked_list(list_t *l) {
    ll_clear(l);

    linked_list_t *ll = (linked_list_t *)l;
    al_free(ll->al, ll);
}

static void ll_clear(list_t *l) {
    linked_list_t *ll = (linked_list_t *)l;
    linked_list_node_t *node = ll->first; 

    while (node) {
        linked_list_node_t *next = node->next;
        al_free(ll->al, node);
        node = next;
    }

    ll->len = 0;
    ll->first = NULL;
    ll->last = NULL;
    ll->iter = NULL;
}

static size_t ll_get_cell_size(list_t *l) {
    linked_list_t *ll = (linked_list_t *)l;
    return ll->cs;
}

static size_t ll_get_len(list_t *l) {
    linked_list_t *ll = (linked_list_t *)l;
    return ll->len;
}

/**
 * This assumes i is valid!
 */
static linked_list_node_t *ll_get_node(linked_list_t *ll, size_t i) {
    // If we are close to the front, we will search forwards.
    // Otherwise we will search backwards.
    bool forward = i <= (ll->len / 2);

    size_t ind;
    linked_list_node_t *node;

    if (forward) {
        node = ll->first;
        ind = 0;
        while (ind < i) {
            node = node->next;
            ind++;
        }
    } else {
        node = ll->last;
        ind = ll->len - 1;
        while (ind > i) {
            node = node->prev;
            ind--;
        }
    }

    return node;
}

/**
 * Create a new node with contents from src. Place the node just before the node which is given.
 * If pos == NULL, push to the end of the list.
 *
 * NOTE: Assumes all args are valid.
 */
static fernos_error_t ll_push_node_at(linked_list_t *ll, linked_list_node_t *pos, const void *src) {
    linked_list_node_t *new_node = al_malloc(ll->al, sizeof(linked_list_node_t) + ll->cs);
    if (!new_node) {
        return FOS_E_NO_MEM;
    }

    mem_cpy(new_node + 1, src, ll->cs);

    if (!pos) {
        // Push back case!
        
        new_node->prev = ll->last;
        new_node->next = NULL;
    } else {
        new_node->prev = pos->prev;
        new_node->next = pos;
    }

    if (new_node->next) {
        new_node->next->prev = new_node;
    } else {
        ll->last = new_node;
    }

    if (new_node->prev) {
        new_node->prev->next = new_node;
    } else {
        ll->first = new_node;
    }

    ll->len++;

    return FOS_E_SUCCESS;
}

/**
 * Assumes all args are valid (dest can be NULL)
 */
static void ll_pop_node(linked_list_t *ll, linked_list_node_t *node, void *dest) {
    if (node->next) {
        node->next->prev = node->prev;
    } else {
        ll->last = node->prev;
    }

    if (node->prev) {
        node->prev->next = node->next;
    } else {
        ll->first = node->next;
    }

    if (dest) {
        mem_cpy(dest, (node + 1), ll->cs);
    }

    al_free(ll->al, node);

    ll->len--;
}

static void *ll_get_ptr(list_t *l, size_t i) {
    linked_list_t *ll = (linked_list_t *)l;
    if (i >= ll->len) {
        return NULL;
    }

    linked_list_node_t *node = ll_get_node(ll, i);
    return node + 1; // skip over the header.
}

static fernos_error_t ll_push(list_t *l, size_t i, const void *src) {
    linked_list_t *ll = (linked_list_t *)l;
    if (i > ll->len) {
        return FOS_E_INVALID_INDEX;
    }

    if (!src) {
        return FOS_E_BAD_ARGS;
    }

    linked_list_node_t *pos = i < ll->len ? ll_get_node(ll, i) : NULL;
    return ll_push_node_at(ll, pos, src);
}


static fernos_error_t ll_pop(list_t *l, size_t i, void *dest) {
    linked_list_t *ll = (linked_list_t *)l;
    if (i >= ll->len) {
        return FOS_E_INVALID_INDEX;
    }

    linked_list_node_t *node = ll_get_node(ll, i);
    ll_pop_node(ll, node, dest);

    return FOS_E_SUCCESS;
}

static void ll_reset_iter(list_t *l) {
    linked_list_t *ll = (linked_list_t *)l;

    ll->iter = ll->first;
}

static void *ll_get_iter(list_t *l) {
    linked_list_t *ll = (linked_list_t *)l;
    return ll->iter ? ll->iter + 1 : NULL;
}

static void *ll_next_iter(list_t *l) {
    linked_list_t *ll = (linked_list_t *)l;

    if (ll->iter) {
        ll->iter = ll->iter->next;
    }

    return ll->iter ? ll->iter + 1 : NULL;
}

static fernos_error_t ll_push_before_iter(list_t *l, const void *src) {
    linked_list_t *ll = (linked_list_t *)l;

    if (!src) {
        return FOS_E_BAD_ARGS;
    }

    if (!(ll->iter)) {
        return FOS_E_INVALID_INDEX;
    }

    linked_list_node_t *pos = ll->iter;

    return ll_push_node_at(ll, pos, src);
}

static fernos_error_t ll_push_after_iter(list_t *l, const void *src) {
    linked_list_t *ll = (linked_list_t *)l;

    if (!src) {
        return FOS_E_BAD_ARGS;
    }

    if (!(ll->iter)) {
        return FOS_E_INVALID_INDEX;
    }

    linked_list_node_t *pos = ll->iter->next;

    // Remember this call will work even if pos is NULL!
    return ll_push_node_at(ll, pos, src);
}

static fernos_error_t ll_pop_iter(list_t *l, void *dest) {
    linked_list_t *ll = (linked_list_t *)l;
    // Unlike for push after
    if (!(ll->iter)) {
        return FOS_E_INVALID_INDEX;
    }

    linked_list_node_t *node = ll->iter;

    // Advance iter premptively.
    ll->iter = ll->iter->next;

    ll_pop_node(ll, node, dest);

    return FOS_E_SUCCESS;
}

static void ll_dump(list_t *l, void (*pf)(const char *fmt, ...)) {
    (void)l;
    (void)pf;

    // TODO: Maybe implement this at some point?
}

