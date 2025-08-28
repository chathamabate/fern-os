
#pragma once

#include <stdbool.h>

/**
 * Run generic file system test suite on the FAT32 file system
 * implementation.
 */
bool test_fat32_file_sys(void);

/**
 * There are some implementation specific aspects of how the fat32_fs
 * interacts with a fat32 device.
 *
 * That is what this suite tests.
 */
bool test_fat32_file_sys_impl(void);
