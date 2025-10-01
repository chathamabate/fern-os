
#pragma once

#include "s_util/err.h"

/*
 * The character display syscall tests are NOT unit tests.
 *
 * The print things out to the VGA Character display which should confirm the kernel side
 * generic character display handle is implemented correctly.
 */

/**
 * This will print a colored border around the VGA CD to confirm the get dimmensions call works.
 */
fernos_error_t test_syscall_cd_dimmensions(void);

/**
 * This should print out rows with alternating colors.
 */
fernos_error_t test_syscall_cd_big(void);
