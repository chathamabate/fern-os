
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
