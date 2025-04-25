
#pragma once

#include "s_util/err.h"
#include "s_util/misc.h"
#include "os_defs.h"

/*
 * Some definitions which specify FernOS Limitations.
 *
 *  The fernOS memory space will have 3 areas:
 *
 *  Code and Data Area 
 *  Free Area
 *  Stack Area
 *
 *  NOTE: These areas are not necessarily snug. There will likely be unmapped pages between them.
 *  The code and data area is populate with the contents of the kernel elf file.
 *
 *  The free area is an area of the memory space which is gauranteed to be unused and mappable.
 *  (Where you should place your heap)
 *
 *  The stack area will be where all stacks are placed.
 *
 *  Other virtual areas may be free, but really shouldn't be used!
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
 * EDITABLE
 *
 * If the kernel elf contents exceed this end, the kernel should fail to boot.
 */
#define FOS_CODE_AND_DATA_AREA_END (0x8000000U)

/**
 * EDITABLE
 *
 * The free area!
 */
#define FOS_FREE_AREA_START (0x10000000U)
#define FOS_FREE_AREA_END   (0x30000000U)
#define FOS_FREE_AREA_SIZE  (FOS_FREE_AREA_END - FOS_FREE_AREA_START)

/**
 * EDITABLE
 *
 * NOTE: The kernel stack should be completely allocated at boot time.
 * (Excluding the redzone page of course)
 */
#define FOS_KERNEL_STACK_SIZE  (8 * M_4K)

/**
 * NOT EDITABLE!
 *
 * The kernel depends on the kernel stack spanning the final pages of the FERNOS memory space.
 */
#define FOS_KERNEL_STACK_END   (FERNOS_END + 1)
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
#define FOS_THREAD_STACK_START(i)   (FOS_THREAD_STACK_END(i) - M_4K) 

/**
 * The "stack area" includes all thread stacks and the kernel stack.
 */
#define FOS_STACK_AREA_SIZE   ((FOS_MAX_THREADS_PER_PROC * FOS_THREAD_STACK_SIZE) + FOS_KERNEL_STACK_SIZE)
#define FOS_STACK_AREA_END    (FOS_KERNEL_STACK_END)
#define FOS_STACK_AREA_START  (FOS_STACK_AREA_END - FOS_STACK_AREA_SIZE)

/**
 * EDITABLE
 *
 * If the stack area size exceeds this value, the kernel should fail to boot!
 *
 * The "stack area" includes all thread stacks of a proceess, and the kernel stack.
 */
#define FOS_STACK_AREA_MAX_SIZE (M_256M)

/**
 * Confirm values listed above make sense.
 */
fernos_error_t validate_constraints(void);

