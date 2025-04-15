
#include "s_data/list.h"
#include "s_util/err.h"
#include "s_util/str.h"

static void delete_linked_list(list_t *l);
static size_t ll_get_cell_size(list_t *l);
static size_t ll_get_len(list_t *l);
static void *ll_get_ptr(list_t *l, size_t i);
static fernos_error_t ll_push(list_t *l, size_t i, const void *src);
static fernos_error_t ll_pop(list_t *l, size_t i, void *dest);
static void ll_reset_iter(list_t *l);
static void *ll_get_iter(list_t *l);
static void *ll_next_iter(list_t *l);
static fernos_error_t ll_push_after_iter(list_t *l, const void *src);
static fernos_error_t ll_pop_iter(list_t *l, void *dest);
static void ll_dump(list_t *l, void (*pf)(const char *fmt, ...));

static const list_impl_t LL_IMPL = {
    .delete_list = delete_linked_list,
    .l_get_cell_size = ll_get_cell_size,
    .l_get_len = ll_get_len,
    .l_get_ptr = ll_get_ptr,
    .l_push = ll_push,
    .l_pop = ll_pop,
    .l_reset_iter = ll_reset_iter,
    .l_get_iter = ll_get_iter,
    .l_next_iter = ll_next_iter,
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

    ll->reached_end = true;
    ll->iter = NULL;

    return (list_t *)ll;
}

static void delete_linked_list(list_t *l) {
    linked_list_t *ll = (linked_list_t *)l;
    linked_list_node_t *node = ll->first; 

    while (node) {
        linked_list_node_t *next = node->next;
        al_free(ll->al, node);
        node = next;
    }

    al_free(ll->al, ll);
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
        return FOS_NO_MEM;
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

    return FOS_SUCCESS;
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
        return FOS_INVALID_INDEX;
    }

    if (!src) {
        return FOS_BAD_ARGS;
    }

    linked_list_node_t *pos = i < ll->len ? ll_get_node(ll, i) : NULL;
    return ll_push_node_at(ll, pos, src);
}


static fernos_error_t ll_pop(list_t *l, size_t i, void *dest) {
    linked_list_t *ll = (linked_list_t *)l;
    if (i >= ll->len) {
        return FOS_INVALID_INDEX;
    }

    linked_list_node_t *node = ll_get_node(ll, i);
    ll_pop_node(ll, node, dest);

    return FOS_SUCCESS;
}

static void ll_reset_iter(list_t *l) {
    linked_list_t *ll = (linked_list_t *)l;

    // reached_end is kinda confusing, but used so we can have the iterator behavoir we want.
    ll->reached_end = ll->len == 0;
    ll->iter = NULL;
}

static void *ll_get_iter(list_t *l) {
    linked_list_t *ll = (linked_list_t *)l;
    return ll->iter;
}

static void *ll_next_iter(list_t *l) {
    linked_list_t *ll = (linked_list_t *)l;

    if (!(ll->reached_end)) {
        if (ll->iter) {
            ll->iter = ll->iter->next;
        } else {
            // Starting case!
            ll->iter = ll->first;
        }

        if (!(ll->iter)) {
            ll->reached_end = true;
        }
    }

    return ll->iter;
}

static fernos_error_t ll_push_after_iter(list_t *l, const void *src) {
    linked_list_t *ll = (linked_list_t *)l;

    if (!src) {
        return FOS_BAD_ARGS;
    }

    linked_list_node_t *pos;

    if (ll->iter) {
        pos = ll->iter->next;
    } else {
        // Here we are working with either the front or the back!
        if (ll->reached_end) {
            pos = NULL;
        } else {
            pos = ll->first;
        }
    }

    return ll_push_node_at(ll, pos, src);
}

static fernos_error_t ll_pop_iter(list_t *l, void *dest) {
    linked_list_t *ll = (linked_list_t *)l;
    // Unlike for push after
    if (!(ll->iter)) {
        return FOS_INVALID_INDEX;
    }

    linked_list_node_t *node = ll->iter;

    // Advance iter premptively.
    ll->iter = ll->iter->next;
    if (!(ll->iter)) {
        ll->reached_end = true;
    }

    ll_pop_node(ll, node, dest);

    return FOS_SUCCESS;
}

static void ll_dump(list_t *l, void (*pf)(const char *fmt, ...)) {
    (void)l;
    (void)pf;
}

