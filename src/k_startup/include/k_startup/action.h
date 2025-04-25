
#pragma once

#include "s_bridge/ctx.h"

/*
 * This file stores all the fernos "actions".
 *
 * These are really just the important interrupt handlers.
 */

/**
 * What should be done in the case of a general protection fault.
 */
void fos_gpf_action(user_ctx_t *ctx);

/**
 * What should be done during a timer interrupt.
 */
void fos_timer_action(user_ctx_t *ctx);

/**
 * What should be done in the case of a syscall.
 */
void fos_syscall_action(user_ctx_t *ctx, uint32_t id, void *arg);
