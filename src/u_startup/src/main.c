
#include "u_startup/main.h"
#include "s_bridge/intr.h"
#include "s_util/misc.h"

static char str_data[36];

void user_main(void) {
    str_data[32] = '#';

    uint32_t len = sizeof(str_data);
    for (uint32_t i = 0; i < 32; i++) {
        str_data[i] = 'a' + (i % 26);
    }
    str_data[len - 2] = '$';
    str_data[len - 1] = '\0';

    // Seems to work I guess??

    trigger_syscall(0, (uint32_t)str_data);

    for (uint32_t i = 0; i < 32; i++) {
        str_data[i] = '\0';
    }

    trigger_syscall(1, (uint32_t)str_data);

    for (uint32_t i = 0; i < len; i++) {
        char c = str_data[i];
        if ('a' <= c && c <= 'z') {
            str_data[i] -= 0x20;
        }
    }


    trigger_syscall(0, (uint32_t)str_data);

    if (str_data[32] != '#') {
        lock_up();
    }

    while (1) {
    }
}
