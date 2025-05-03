
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "s_data/map.h"

/**
 * Run the map test suite.
 * 
 * name is the name of the test suite.
 * gen_map should generate a map with key size ks and val size vs.
 */
bool test_map(const char *name, map_t *(*gen)(size_t ks, size_t vs));
