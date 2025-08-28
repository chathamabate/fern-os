
#pragma once

#include <stdbool.h>
#include "s_block_device/file_sys.h"

/**
 * Run generic file system tests.
 *
 * The file system returned by `gen` should contain just the root directory
 * with just the entry ".".
 *
 * These tests use the default heap. (They also only check for memory leaks on
 * the default heap.)
 */
bool test_file_sys(const char *name, file_sys_t *(*gen)(void));
