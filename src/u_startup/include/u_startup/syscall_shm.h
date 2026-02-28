
#pragma once

#include "s_util/err.h"
#include "s_bridge/shared_defs.h"

/**
 * Create a shared memory region of at least `bytes` bytes.
 * Returns NULL if allocation fails for some reason.
 *
 * Shared memory regions are mapped in the kernel.
 * AND, when forking, shared memory regions are mapped in child processes.
 *
 * Shared memory regions are reference counted. 
 */
void *sc_shm_alloc(size_t bytes);

/**
 * If `shm` points to any byte within a shared memory region mapped in this process, 
 * the entire referenced shared memory region is unmapped in the process.
 *
 * If no other process's reference the region, it will be freed.
 *
 * NOTE: When a process exits, it frees all of its mapped shared memory regions.
 */
void sc_shm_release(void *shm);

// Now semaphor stuff too I gues??

