
#include "s_util/rand.h"

uint8_t next_rand_u1(rand_t *r) {
    // Taps: 64, 63, 61, 60 (1 indexed)

    uint64_t old_val = *r;

    uint8_t old_bit = old_val >> 63;
    uint8_t new_bit = ((old_val >> 63) ^ (old_val >> 62) ^ (old_val >> 60) ^ (old_val >> 59)) & 1U;

    *r = (old_val << 1) | new_bit;

    return old_bit;
}

uint8_t next_rand_u8(rand_t *r) {
    uint8_t val = 0;
    for (uint8_t i = 0; i < 8; i++) {
        val <<= 1;
        val |= next_rand_u1(r);
    }
    return val;
}

uint16_t next_rand_u16(rand_t *r) {
    uint16_t val = 0;
    for (uint8_t i = 0; i < 16; i++) {
        val <<= 1;
        val |= next_rand_u1(r);
    }
    return val;
}

uint32_t next_rand_u32(rand_t *r) {
    uint32_t val = 0;
    for (uint8_t i = 0; i < 32; i++) {
        val <<= 1;
        val |= next_rand_u1(r);
    }
    return val;
}

uint64_t next_rand_u64(rand_t *r) {
    uint64_t val = 0;
    for (uint8_t i = 0; i < 64; i++) {
        val <<= 1;
        val |= next_rand_u1(r);
    }
    return val;
}

