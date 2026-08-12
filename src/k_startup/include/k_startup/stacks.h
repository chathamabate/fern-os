#pragma once

#include "c_config.h"

/*
 * I have replaced the old "constraints.h" file with fernconf.
 *
 * Although, I used to have a nice macro for determining the location of a user stack
 * of a given indexed thread. 
 *
 * So, this file contains these nice stack macros which don't fit into the configuration
 * schema very well!
 */

/*
 * NOTE: When I define a "stack area size", this will always include the redzone page.
 *
 * Every stack area will have at least one unmapped page, this way a page fault will trigger 
 * during a stack overflow. Thus, the true area useable by the stack will be "stack size" - 4K.
 */

/**
 * NOT EDITABLE!
 *
 * The kernel depends on the kernel stack spanning the final pages of the stack area!
 * (These pages will be identity mapped)
 */
#define FC_CORE_KSTACK_START    (FC_CORE_VMEM_STACK_END - FC_CORE_KSTACK_SIZE)
#define FC_CORE_KSTACK_END      (FC_CORE_VMEM_STACK_END)

/**
 * i is the thread index, (i < FOS_MAX_THREADS_PER_PROC)
 *
 * The indexes go down. i.e. thread stack 0, will be just below the kernel stack.
 */
#define FC_CORE_TSTACK_END(i)     (FC_CORE_KSTACK_START - ((i) * FC_CORE_TSTACK_SIZE))
#define FC_CORE_TSTACK_START(i)   (FC_CORE_TSTACK_END(i) - FC_CORE_TSTACK_SIZE)
