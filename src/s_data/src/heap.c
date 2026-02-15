
#include "s_data/heap.h"

/**
 * MUST BE NON-ZERO
 */
#define HP_STARTING_CAP 1

heap_t *new_heap(allocator_t *al, size_t cs, comparator_ft cmp) {
    if (!al || cs == 0 || !cmp) {
        return NULL;
    }

    heap_t *hp = al_malloc(al, sizeof(heap_t));
    uint8_t *buf = al_malloc(al, HP_STARTING_CAP * cs);

    if (!hp || !buf) {
        return NULL;
    }

    *(allocator_t **)&(hp->al) = al;
    hp->buf = buf;
    *(size_t *)&(hp->cell_size) = cs;
    hp->buf_cap = HP_STARTING_CAP;
    hp->tree_len = 0;
    *(comparator_ft *)&(hp->cmp) = cmp;

    return hp;
}

void delete_heap(heap_t *hp) {
    al_free(hp->al, hp->buf);
    al_free(hp->al, hp);
}

bool hp_pop_max(heap_t *hp, void *out) {
    if (hp->tree_len == 0) {
        return false;
    }

    if (out) {
        mem_cpy(out, hp->buf + (hp->cell_size * 0), hp->cell_size);
    }

    if (hp->tree_len > 1) {
        // If there is more than one element, we must do a shift down!
        // Essentially swap the last node with the root, then push the last node down. 
     
        uint8_t * const sub = hp->buf + (hp->cell_size * (hp->tree_len - 1));

        size_t curr_pos = 0;
        uint8_t *curr = hp->buf + (hp->cell_size * curr_pos);

        // NOTE: During this loop, we consider the cell at position `tree_len - 1` as
        // invalid! (Conceptually, this is the last cell of the heap, which we are now pushing
        // into the root position)
        while (true) {
            const size_t left_pos = (curr_pos * 2) + 1;
            uint8_t * const left = left_pos < hp->tree_len - 1 
                ? hp->buf + (hp->cell_size * left_pos) : NULL;

            const size_t right_pos = left_pos + 1;
            uint8_t * const right = right_pos < hp->tree_len - 1 
                ? hp->buf + (hp->cell_size * right_pos) : NULL;
            
            size_t next_pos;
            uint8_t *next;

            if (right) {
                // If we have a right child, we must have a left.
                if (hp->cmp(left, right) < 0) {
                    next_pos = right_pos;
                    next = right;
                } else {
                    next_pos = left_pos;
                    next = left;
                }
            } else if (left) {
                // we just have a left.
                next_pos = left_pos;
                next = left;
            } else {
                // No left and no right :,(
                break;
            }

            // Ok, so we are gauranteed to have a next position lined up.
            if (hp->cmp(sub, next) >= 0) {
                // If our substitute >= the next node, we can stop here!
                break;
            }

            // We are advancing down heap, must shift next up. 
            mem_cpy(curr, next, hp->cell_size);

            curr_pos = next_pos;
            curr = next;
        }

        // Finally, but "final" element into its new found position.
        mem_cpy(curr, sub, hp->cell_size);
    }

    hp->tree_len--;
    return true;
}

fernos_error_t hp_push(heap_t *hp, const void *val) {
    if (!val) {
        return FOS_E_BAD_ARGS;
    }

    if (hp->tree_len == hp->buf_cap) {
        const size_t new_cap = hp->buf_cap * 2; // simple double.

        void *new_buf = al_realloc(hp->al, hp->buf, new_cap * hp->cell_size);
        if (!new_buf) {
            return FOS_E_NO_MEM;
        }

        hp->buf = new_buf;
        hp->buf_cap = new_cap;
    }

    // Ok, so we traverse up the heap starting from the ending position
    // until we find a node which is greater than or equal to `*val`.
    //
    // Swapping as we go!

    size_t curr_pos = hp->tree_len; 
    void *curr =  hp->buf + (hp->cell_size * curr_pos);

    while (curr_pos > 0) {
        const size_t parent_pos = (curr_pos - 1) / 2;
        void * const parent = hp->buf + (hp->cell_size * parent_pos);

        if (hp->cmp(parent, val) >= 0) {
            // The parent is >= `val` break!
            break;
        }

        // the parent < `val` copy parent down to make space!
        mem_cpy(curr, parent, hp->cell_size);

        // advance!
        curr_pos = parent_pos;
        curr = parent;
    }

    // Here `curr` points to a free cell who's direct parent is >= `*val`,
    // and who's children are both < `*val`. Woo!

    mem_cpy(curr, val, hp->cell_size);

    hp->tree_len++;

    return FOS_E_SUCCESS;
}
