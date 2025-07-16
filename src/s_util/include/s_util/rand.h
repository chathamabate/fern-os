
#pragma once

#include <stdint.h>

#define RAND_MAX (0x7FFFFFFFU)

typedef uint32_t rand_t;

static inline rand_t rand(uint32_t seed) {
    return seed % (RAND_MAX + 1);
}

static inline uint32_t next_rand(rand_t *r) {
    // This PRNG was taken from ChatGPT. Apparnetly, this is the 
    // glibc implementation.
    *r = ((1103515245 * *r) + 12345) % (RAND_MAX + 1);

    return *r;
}
