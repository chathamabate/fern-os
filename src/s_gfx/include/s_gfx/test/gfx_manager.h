
#pragma once

#include "s_gfx/gfx_manager.h"

/**
 * This is a generic test suite for graphics managers.
 *
 * This suite should work fine whether the manager is single or double buffered.
 * 
 * This will do only small resizes to accomodate for managers which have some sort
 * of fixed size limit.
 */
bool test_gfx_manager(const char *name, gfx_manager_t *(*gm_gen)(uint16_t, uint16_t), void (*l)(const char *fmt, ...));

bool test_dynamic_gfx_manager(void (*l)(const char *fmt, ...));
bool test_dynamic_gfx_manager_single(void (*l)(const char *fmt, ...));
