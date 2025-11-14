
#pragma once
#include <stdbool.h>

/**
 * This suite tests that the `sc_proc_exec` syscall works as expected.
 *
 * This suite is separate from the normal syscall suite because it depends on the
 * file system plugin and the test app.
 */
bool test_syscall_exec(void);
