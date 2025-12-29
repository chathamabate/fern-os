
#include "s_util/misc.h"
#include <stdarg.h>
#include "s_util/ansi.h"

void _dump_hex_pairs(void (*pf)(const char *fmt, ...), ...) {
    va_list va;
    va_start(va, pf);

    const char *name = va_arg(va, const char *);
    uint32_t val;

    while (name) {
        val = va_arg(va, uint32_t);

        pf(ANSI_LIGHT_GREY_FG "%s: "  ANSI_CYAN_FG "0x%X" ANSI_RESET, name, val);

        name = va_arg(va, const char *);

        if (name) {
            pf(", "); 
        }
    }

    va_end(va);
}

bool intervals_overlap(int32_t *pos, int32_t *len, int32_t window_len)  {
    int32_t p = *pos;  
    int32_t l = *len;

    if (l <= 0 || window_len <= 0) {
        return false;
    }

    if (p < 0) {
        // With p < 0, wrap is impossible here given `l` is positive.
        if (p + l <= 0) {
            return false;
        }
        
        // Chop off left side. `l` must be greater than zero after the chop.
        l += p;
        p = 0;
    } else if (p >= window_len) {
        return false; // Too far off to the right.
    } else { /* 0 <= p && p < window_len */
        // Since `p` is positive, now there is a chance for wrap when adding
        // another positive number.
        if (p + l < 0) { 
            // This makes the maximum possible value of `p + l` INT32_MAX.
            // WHICH IS OK! because the largest window interval describeable is
            // [0, INT32_MAX)... INT32_MAX will never be able to be in the overlapping
            // interval.
            l = INT32_MAX - p; 
        }

        if (p + l > window_len) {
            // Chop off right hanging side.
            l = window_len - p;
        }
    }

    *pos = p;
    *len = l;

    return true;
}

