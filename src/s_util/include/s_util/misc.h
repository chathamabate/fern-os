
#pragma once

#include "s_util/err.h"

#define NULL ((void *)0)

#define M_4K       (0x1000)
#define M_64K     (0x10000)
#define M_1M     (0x100000)
#define M_4M     (0x400000)
#define M_16M   (0x1000000)
#define M_256M (0x10000000)

// Converts a number n to a 64-bit constant with n 1 lsbs
#define TO_MASK64(wid) ((1LL << wid) - 1)

// Only work for powers of 2 alignments.
#define IS_ALIGNED(val, align) (((unsigned int)(val) & ((align) - 1)) == 0)
#define ALIGN(val, align) ((unsigned int)(val) & ~((align) - 1))

#define CHECK_ALIGN(val, align) \
    if (!IS_ALIGNED(val, align)) { \
        return FOS_ALIGN_ERROR; \
    }

#define PROP_ERR(expr) \
    do { \
        fernos_error_t __err = expr; \
        if (__err != FOS_SUCCESS) { \
            return __err; \
        } \
    } while (0);


