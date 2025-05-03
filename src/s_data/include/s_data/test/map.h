
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "s_data/map.h"

/**
 * Run the map test suite.
 * 
 * name is the name of the test suite.
 * gen_map should generate a map with key size ks and val size vs.
 *
 * These tests assume that two keys are equal if and only if all their bytes are equal.
 *
 * It's very possible this is not the case in practice, but alas, not easy to make generic tests.
 */
bool test_map(const char *name, map_t *(*gen)(size_t ks, size_t vs));
