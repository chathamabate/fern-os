
#include "u_startup/main.h"
#include "k_bios_term/term.h"
#include "k_sys/intr.h"
#include "s_util/misc.h"
#include "s_bridge/ctx.h"
#include "s_util/str.h"
#include "s_util/atomic.h"
#include "u_concur/mutex.h"
#include "s_util/constraints.h"
#include "u_startup/syscall.h"
#include "u_startup/test/syscall.h"


void *user_main(void *arg) {
    (void)arg;

    test_syscall();

    return (void *)PROC_ES_SUCCESS;
}
