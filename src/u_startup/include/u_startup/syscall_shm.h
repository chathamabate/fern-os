
#pragma once

#include "s_util/err.h"
#include "s_bridge/shared_defs.h"

/**
 * Create a shared memory region of at least `bytes` bytes.
 *
 * A "shared memory region" is a piece of memory that is mapped at the same position
 * in both kernel and userspace. Additionally, while mapped, forked child process will also
 * inherit this region. Its purpose is to allow for faster communication across memory spaces.
 *
 * If `out` is NULL or `bytes` is 0, FOS_E_BAD_ARGS is returned.
 * If there is an error with allocation FOS_E_NO_MEM is returned.
 *
 * On success, FOS_E_SUCCESS is returned, the start of the region is written to `*out`.
 */
fernos_error_t sc_shm_alloc(void **out, size_t bytes);

/**
 * If `shm` points to any byte within a shared memory region mapped in this process, 
 * the entire referenced shared memory region is unmapped in the process.
 *
 * Said region has its reference count decremented. When the count reaches zero, the kernel
 * will free the region's underlying pages.
 *
 * NOTE: When a process exits, it releases all of its mapped shared memory regions.
 */
void sc_shm_release(void *shm);


