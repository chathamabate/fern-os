
#pragma once

typedef struct _destructable_t destructable_t;

typedef struct _destructable_impl_t {
    void (*delete_me)(destructable_t *me);   
} destructable_impl_t;

/**
 * A very simple but useful structure!
 *
 * It's purpose is to hold an arbitrary object with a destructor, that's it!
 */
struct _destructable_t {
    const destructable_impl_t * const impl;
};

static inline void delete_destructable(destructable_t *d) {
    d->impl->delete_me(d);
}

