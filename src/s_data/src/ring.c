
#include "s_data/ring.h"

void init_ring(ring_t *r) {
    r->len = 0;
    r->head = NULL;
}

void init_ring_element(ring_element_t *re) {
    re->r = NULL;
    re->next = NULL;
    re->prev = NULL;
}

void ring_element_detach(ring_element_t *re) {
    // Only do anything if we are currently attached.
    if (re->r) {
        if (re->r->len == 1) { // re is the only element
            re->r->head = NULL;
        } else { // Must be at least 2 elements in the ring!
            if (re->r->head == re) {
                re->r->head = re->next;
            }

            re->next->prev = re->prev;
            re->prev->next = re->next;
        }

        re->r = NULL;
        re->next = NULL;
        re->prev = NULL;
        
        re->r->len--;
    }
}

void ring_element_attach(ring_element_t *re, ring_t *r) {
    if (re->r) {
        ring_element_detach(re);
    }

    if (r->len == 0) {
        r->head = re;

        re->next = re;
        re->prev = re;
    } else { // At least 1 element in the ring!
        re->next = r->head; 
        re->prev = r->head->prev;

        r->head->prev->next = re;
        r->head->prev = re;
    }

    re->r = r;
    r->len++;
}
