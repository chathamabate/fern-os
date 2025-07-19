
#pragma once

#include <stdint.h>

typedef uint64_t rand_t;

static inline rand_t rand(uint64_t seed) {
    // Provide a fallback seed.
    return seed == 0 ? 0xAABB1234FFEEBACDULL : seed;
}

uint8_t next_rand_u1(rand_t *r);
uint8_t next_rand_u8(rand_t *r);
uint16_t next_rand_u16(rand_t *r);
uint32_t next_rand_u32(rand_t *r);


