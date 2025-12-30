
#include "u_startup/main.h"

#include "u_startup/syscall.h"
#include "u_startup/syscall_vga_cd.h"

#include <stdarg.h>

#include "u_startup/test/syscall.h"
#include "u_startup/test/syscall_fs.h"
#include "s_mem/simple_heap.h"

proc_exit_status_t user_main(void) {
    while (1);

    return PROC_ES_SUCCESS;
}
