
#pragma once

#include "s_bridge/ctx.h"

/*
 * This file stores all the fernos "actions".
 *
 * These are really just the important interrupt handlers.
 */

/**
 * This is a simple action which prints out a message and locks up the kernel.
 * It's purpose is to be used in the earlier stages of kernel setup to catch extremely fatal 
 * errors.
 */
void fos_lock_up_action(user_ctx_t *ctx);

/**
 * What should be done in the case of a general protection fault.
 */
void fos_gpf_action(user_ctx_t *ctx);

/**
 * What should be done in case of a page fault.
 */
void fos_pf_action(user_ctx_t *ctx);

/**
 * What should be done during a timer interrupt.
 */
void fos_timer_action(user_ctx_t *ctx);

/**
 * What should be done in the case of a syscall.
 */
void fos_syscall_action(user_ctx_t *ctx, uint32_t id, uint32_t arg0, uint32_t arg1, 
        uint32_t arg2, uint32_t arg3);

/**
 * IRQ1 interrupt action (This is usually for the keyboard/mouse)
 */
void fos_irq1_action(user_ctx_t *ctx);
