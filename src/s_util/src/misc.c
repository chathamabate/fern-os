
#include "s_util/misc.h"
#include <stdarg.h>
#include "s_util/ansii.h"

void _dump_hex_pairs(void (*pf)(const char *fmt, ...), ...) {
    va_list va;
    va_start(va, pf);

    const char *name = va_arg(va, const char *);
    uint32_t val;

    while (name) {
        val = va_arg(va, uint32_t);

        pf(ANSII_LIGHT_GREY_FG "%s: "  ANSII_CYAN_FG "0x%X" ANSII_RESET, name, val);

        name = va_arg(va, const char *);

        if (name) {
            pf(", "); 
        }
    }

    va_end(va);
}
