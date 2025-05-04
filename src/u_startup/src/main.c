
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


void *user_main(void *arg) {
    (void)arg;

    for (uint32_t i = 0; i < 10; i++) {
        fernos_error_t err = sc_thread_spawn(NULL, other_main, (void *)i);
        if (err != FOS_SUCCESS) {
            uprintf("Failed to spawn thread %u\n", i);
            while (1);
        }
    }

    sc_thread_sleep(4);

    for (uint32_t i = 0; i < 10; i++) {
        fernos_error_t err = sc_thread_join(full_join_vector(), NULL, NULL);
        if (err != FOS_SUCCESS) {
            uprintf("Failed to join threads\n", i);
            while (1);
        }
    }

    uprintf("All Joined\n");

    while (1);
}

void *other_main(void *arg) {
    uprintf("[%u] Starting Task\n", arg);
    sc_thread_sleep(5);
    uprintf("[%u] Ending Task\n", arg);
    return (void *)((uint32_t)arg * 2);
}


