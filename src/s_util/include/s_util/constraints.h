
#pragma once

#include "s_util/misc.h"

/*
 * Some definitions which specify FernOS Limitations.
 */

#define FOS_MAX_THREADS_PER_PROC (16)
#define FOS_MAX_PROCS (256)

/**
 * NOTE: Each thread's stack size will be slightly less than this value.
 * There will always exist a 1 page redzone between thread stacks.
 */
#define FOS_THREAD_STACK_SIZE (M_4M)

#define FOS_STACK_AREA_SIZE   (FOS_MAX_THREADS_PER_PROC * FOS_THREAD_STACK_SIZE)

/**
 * If the stack area size exceeds this value, the kernel should fail to boot!
 */
#define FOS_STACK_AREA_MAX_SIZE (M_256M)
