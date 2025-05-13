
#include "u_startup/main.h"
#include "k_bios_term/term.h"
#include "k_sys/intr.h"
#include "s_util/misc.h"
#include "s_bridge/ctx.h"
#include "s_bridge/syscall.h"
#include "s_util/str.h"
#include "s_util/atomic.h"

static void uprintf(const char *fmt, ...) {
    char buf[256];

    va_list va; 
    va_start(va, fmt);
    str_vfmt(buf, fmt, va);
    va_end(va);

    sc_term_put_s(buf);
}

void *other_main(void *arg);

futex_t fut;

void *user_main(void *arg) {
    (void)arg;

    fernos_error_t err;

    fut = 0;
    err = sc_futex_register(&fut);
    if (err != FOS_SUCCESS) {
        uprintf("Failed to create futex\n");
        while (1);
    }

    for (uint32_t i = 0; i < 4; i++) {
        err = sc_thread_spawn(NULL, other_main, (void *)i); 
        if (err != FOS_SUCCESS) {
            uprintf("Failed to spawn thread\n");
            while (1);
        }
    }


    sc_thread_sleep(30);

    err = sc_futex_wake(&fut, true);
    if (err != FOS_SUCCESS) {
        uprintf("Failed to wake\n");
        while (1);
    }

    while (1);
}

void *other_main(void *arg) {
    fernos_error_t err;

    uprintf("[%u] Starting Task\n", arg);

    err = sc_futex_wait(&fut, 0);
    if (err != FOS_SUCCESS) {
        uprintf("Error waiting on futex\n");
    }

    uprintf("[%u] Ending Task\n", arg);
    return (void *)((uint32_t)arg * 2);
}


