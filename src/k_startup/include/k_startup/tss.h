
#pragma once

#include "s_util/err.h"

extern uint8_t _tss_start[];
extern uint8_t _tss_end[];

/**
 * For now there will only ever be one tss.
 *
 * It'll be used for interrupt handling!
 */
fernos_error_t init_global_tss(void);
