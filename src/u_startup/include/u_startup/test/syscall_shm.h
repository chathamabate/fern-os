
#pragma once

#include <stdbool.h>

/**
 * This tests the shared memory plugin's semaphore capabilities explicitly.
 *
 * To isolate this test from shared memory stuff, this will use pipes as a way of
 * sharing data between processes.
 */
bool test_syscall_shm_sem(void);
