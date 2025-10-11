

#include "s_util/str.h"
#include <stddef.h>

// Alright, we are gonna need a linker script for this shit.

int main(void) {
    size_t i = str_len("Hello\n");
    return 0;
}
