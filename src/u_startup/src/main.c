
#include "u_startup/main.h"
#include "k_bios_term/term.h"
#include "k_sys/intr.h"
#include "s_util/misc.h"
#include "s_bridge/ctx.h"
#include "s_bridge/syscall.h"
#include "s_util/str.h"

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

    uint32_t threads = 10;
    thread_id_t tid;
    fernos_error_t err;

    for (uint32_t i = 0; i < threads; i++) {
        err = sc_thread_spawn(NULL, other_main, (void *)i);
        if (err != FOS_SUCCESS) {
            sc_term_put_s("Failed to spawn thread\n");
            while (1);
        } 
    }

    for (uint32_t i = 0; i < threads; i++) {
        void *retval;
        err = sc_thread_join(full_join_vector(), &tid, &retval);
        if (err != FOS_SUCCESS) {
            sc_term_put_s("Failed to join thread\n");
            while (1);
        } 

        uprintf("Retval received %u\n", retval);
    }

    uprintf("Done\n");

    while (1) {
        sc_thread_sleep(3);
    }
}

void *other_main(void *arg) {
    uint32_t worker_num = (uint32_t)arg;

    for (uint32_t i = 0; i < 4; i++) {
        uprintf("[%u] Job %u of %u Done\n", worker_num, i + 1, 4);
        sc_thread_sleep(2);
    }

    uint32_t retval = worker_num * 2;

    uprintf("[%u] Returning %u\n", worker_num, retval);

    return (void *)retval;
}


