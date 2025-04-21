
#include "u_startup/main.h"
#include "s_bridge/intr.h"
#include "s_util/misc.h"

static char str_data[36];

void user_main(void) {

    trigger_syscall(0, 0);

    while (1) {
    }
}
