
#pragma once

#include <stdbool.h>
#include "s_bridge/shared_defs.h"

/**
 * This suite tests that the default input and output system calls work as expected.
 *
 * NOTE: These tests are separate from `syscall.h` tests because they depend on the 
 * `syscall_fs.h` endpoints to create file handles.
 *
 * NOTE: These tests will overwrite the default in and out handles of the calling process.
 *
 * Since this tests works with the default IO calls, it cannot rely on them for printing
 * test output. `cd` is where test results will be output.
 */
bool test_syscall_default_io(handle_t cd);
