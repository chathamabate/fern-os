
#pragma once

#include "s_util/err.h"
#include "s_util/misc.h"
#include "os_defs.h"

/*
 * Some definitions which specify FernOS Limitations.
 *
 *  The fernOS memory space will have 4 areas.
 *  The boundaries of these area are defined in the top level Makefile. (Which generates os_defs.h)
 * 
 *  Kernel Area - Where kernel code and data lives.
 *  App Area - Where application code and data will live.
 *  Free Area - Where the free area will live for the kernel and user processes.
 *  Stack Area - Where the kernel and user stacks will live.
 *
 *  NOTE: These areas are not necessarily snug. There will likely be unmapped pages between them.
 *  The code and data area is populate with the contents of the kernel elf file.
 *
 *  The free area is an area of the memory space which is gauranteed to be unused and mappable.
 *  (Where you should place your heap)
 *
 *  NOTE: If any area definitons below lead to overlapping areas, the kernel should fail to boot.
 */

/*
 * NOTE: When I define a "stack area size", this will always include the redzone page.
 *
 * Every stack area will have at least one unmapped page, this way a page fault will trigger 
 * during a stack overflow. Thus, the true area useable by the stack will be "stack size" - 4K.
 *
 * NOTE: All sizes and addresses relating to stacks MUST be 4K aligned.
 */

/**
 * The entire FernOS memory space! (Likely not the full 4GB of mem)
 */
#define FOS_AREA_START (FERNOS_START)
#define FOS_AREA_END   (FERNOS_END + 1)
#define FOS_AREA_SIZE  (FOS_AREA_END - FOS_AREA_START)

/**
 * Where Kernel stuff is placed.
 *
 * NOTE: The linker script exposes much more fine grained boundaries for finding
 * where things like the bss and text sections live exactly.
 */
#define FOS_KERNEL_AREA_START (FERNOS_KERNEL_START)
#define FOS_KERNEL_AREA_END   (FERNOS_KERNEL_END + 1)
#define FOS_KERNEL_AREA_SIZE  (FOS_KERNEL_AREA_END - FOS_KERNEL_AREA_START)

/**
 * Where application binaries will be placed.
 */
#define FOS_APP_AREA_START (FERNOS_APP_START)
#define FOS_APP_AREA_END   (FERNOS_APP_END + 1)
#define FOS_APP_AREA_SIZE  (FOS_APP_AREA_END - FOS_APP_AREA_START)

/**
 * Where application arguments are placed.
 */
#define FOS_APP_ARGS_AREA_START (FERNOS_APP_ARGS_START)
#define FOS_APP_ARGS_AREA_END   (FERNOS_APP_ARGS_END + 1)
#define FOS_APP_ARGS_AREA_SIZE  (FOS_APP_ARGS_AREA_END - FOS_APP_ARGS_AREA_START)

/**
 * Where the free area will live.
 */
#define FOS_FREE_AREA_START (FERNOS_FREE_START)
#define FOS_FREE_AREA_END   (FERNOS_FREE_END + 1)
#define FOS_FREE_AREA_SIZE  (FOS_FREE_AREA_END - FOS_FREE_AREA_START)

/**
 * Where the shared memory pages will be mapped.
 */
#define FOS_SHARED_AREA_START (FERNOS_SHARED_START)
#define FOS_SHARED_AREA_END   (FERNOS_SHARED_END + 1)
#define FOS_SHARED_AREA_SIZE  (FOS_SHARED_AREA_END - FOS_SHARED_AREA_START)

/**
 * Where the stacks will live.
 */
#define FOS_STACK_AREA_START (FERNOS_STACK_START)
#define FOS_STACK_AREA_END   (FERNOS_STACK_END + 1)
#define FOS_STACK_AREA_SIZE  (FOS_STACK_AREA_END - FOS_STACK_AREA_START)

/**
 * EDITABLE
 *
 * NOTE: The kernel stack should be completely allocated at boot time.
 * (Excluding the redzone page of course)
 */
#define FOS_KERNEL_STACK_SIZE  (64 * M_4K)

/**
 * NOT EDITABLE!
 *
 * The kernel depends on the kernel stack spanning the final pages of the FERNOS memory space.
 */
#define FOS_KERNEL_STACK_END   (FOS_STACK_AREA_END)
#define FOS_KERNEL_STACK_START (FOS_KERNEL_STACK_END - FOS_KERNEL_STACK_SIZE) 

/**
 * EDITABLE, Can never be larger than 32.
 */
#define FOS_MAX_THREADS_PER_PROC (16)

/**
 * EDITABLE
 */
#define FOS_MAX_PROCS (256)

/**
 * EDITABLE
 */
#define FOS_THREAD_STACK_SIZE (M_4M)

/**
 * i is the thread index, (i < FOS_MAX_THREADS_PER_PROC)
 *
 * The indexes go down. i.e. thread stack 0, will be just below the kernel stack.
 */
#define FOS_THREAD_STACK_END(i)     (FOS_KERNEL_STACK_START - ((i) * FOS_THREAD_STACK_SIZE))
#define FOS_THREAD_STACK_START(i)   (FOS_THREAD_STACK_END(i) - FOS_THREAD_STACK_SIZE) 

/**
 * Given Threads Per Proc, Thread Stack Size, and Kernel Stack Size, this is the minimum required
 * size of the stack area. Anything smaller, and the kernel will fail to boot.
 */
#define FOS_MIN_STACK_AREA_SIZE   ((FOS_MAX_THREADS_PER_PROC * FOS_THREAD_STACK_SIZE) + FOS_KERNEL_STACK_SIZE)

/**
 * EDITABLE
 *
 * Maximum number of handles which can be open by one process at once.
 * MUST be <= 256.
 */
#define FOS_MAX_HANDLES_PER_PROC (32U)

/**
 * EDITABLE
 *
 * The maximum number of plugins holdable by the kernel.
 * 
 * Must be <= 256.
 */
#define FOS_MAX_PLUGINS (16U)

/**
 * Confirm values listed above make sense.
 */
fernos_error_t validate_constraints(void);

