
#include "u_startup/main.h"
#include "s_util/misc.h"
#include "s_util/str.h"
#include "u_concur/mutex.h"
#include "s_util/constraints.h"
#include "u_startup/syscall.h"
#include "u_startup/test/syscall.h"
#include "u_concur/test/mutex.h"

proc_exit_status_t user_main(void) {
    test_syscall();

    return PROC_ES_SUCCESS;
}
