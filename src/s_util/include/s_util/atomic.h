
#pragma once

#include <stdint.h>

/**
 * This function compares *dest with expected.
 *
 * If *dest = expected, dest will be set to desired, and expected is returned.
 * Otherwise, the value of *dest is returned.
 *
 * This is atomic and meant to be used for synchronization primitives.
 *
 * Since this is a single core 32-bit system, I don't think I need to worry about the 
 * alignment of dest.
 */
int32_t cmp_xchg(int32_t expected, int32_t desired, int32_t *dest);

/**
 * A spinlock with value 1 is locked, with value 0 is unlocked.
 *
 * Extremely simple primitive!
 *
 * Extremely dangerous to use, starvation is almost likely!
 */
typedef int32_t spinlock_t;

static inline void init_spinlock(int32_t *spl) {
    *spl = 0;
}

static inline void spl_lock(spinlock_t *spl) {
    while (cmp_xchg(0, 1, spl) == 1);
}

static inline void spl_unlock(spinlock_t *spl) {
    // Set the lock back down to 0.
    cmp_xchg(1, 0, spl);
}
