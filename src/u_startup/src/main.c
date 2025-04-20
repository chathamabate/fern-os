
#include "u_startup/main.h"
#include "s_bridge/intr.h"
#include "s_util/misc.h"

static char str_data[16 * M_4K+2];

void user_main(void) {

    uint32_t len = sizeof(str_data);
    for (uint32_t i = 0; i < len; i++) {
        str_data[i] = 'a' + (i % 26);
    }
    str_data[len - 2] = '$';
    str_data[len - 1] = '\0';

    // Seems to work I guess??

    trigger_syscall(0, (uint32_t)str_data);

    while (1) {
    }
}
