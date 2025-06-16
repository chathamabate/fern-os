
#pragma once

#include <stdbool.h>
#include "s_block_device/block_device.h"

#define TEST_BLOCK_DEVICE_MIN_SECTORS (0x40U)

/**
 * Run the generic Block Device test suite.
 *
 * These tests assume a default heap is set up.
 * It also assumes that the given block device has at least TEST_BLOCK_DEVICE_MIN_SECTORS
 * available.
 */
bool test_block_device(const char *name, block_device_t *(*gen)(void));
