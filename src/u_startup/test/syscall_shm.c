
#include "u_startup/test/syscall_shm.h"
#include "u_startup/syscall_shm.h"
#include "u_startup/syscall.h"

#define LOGF_METHOD(...) sc_out_write_fmt_s(__VA_ARGS__)
#define FAILURE_ACTION() while (1)

#include "s_util/test.h"

static bool test_sem_new_and_close(void) {
    sem_id_t sem;

    TEST_EQUAL_HEX(FOS_E_BAD_ARGS, sc_shm_new_semaphore(NULL, 10));
    TEST_EQUAL_HEX(FOS_E_BAD_ARGS, sc_shm_new_semaphore(&sem, 0));

    TEST_SUCCESS(sc_shm_new_semaphore(&sem, 2));
    sc_shm_close_semaphore(sem);

    TEST_SUCCEED();
}

bool test_syscall_shm_sem(void) {
    BEGIN_SUITE("Shared Memory: Semaphores");
    RUN_TEST(test_sem_new_and_close);
    return END_SUITE();
}
