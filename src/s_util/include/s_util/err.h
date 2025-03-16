#pragma once

#include <stdint.h>

typedef uint16_t fernos_error_t;

#define FOS_SUCCESS       (0)

#define FOS_UNKNWON_ERROR (1)

#define FOS_ALIGN_ERROR   (2)

/**
 * Given when a range's end is less than a range's start.
 */
#define FOS_INVALID_RANGE_ERROR  (3)


