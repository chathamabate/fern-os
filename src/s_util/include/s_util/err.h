#pragma once

#include <stdint.h>

typedef uint16_t fernos_error_t;

#define FOS_SUCCESS       (0)

#define FOS_UNKNWON_ERROR (1)

/**
 * A value which was expected to be aligned was not.
 */
#define FOS_ALIGN_ERROR   (2)

/**
 * Given when a range's end is less than a range's start.
 */
#define FOS_INVALID_RANGE (3)

/**
 * A given argument is invalid for an unspecified reason.
 */
#define FOS_BAD_ARGS    (4)

/**
 * Lack of memory stopped the behavoir from completing.
 */
#define FOS_NO_MEM      (5)

/**
 * We attempted to allocate an area which already was in use.
 */
#define FOS_ALREADY_ALLOCATED (6)

/**
 * You tried to allocate/deallocate pages in the identity area.
 */
#define FOS_IDENTITY_OVERWRITE (7)

/**
 * You gave an index which is invalid.
 */
#define FOS_INVALID_INDEX      (8)

/**
 * An operation cannot be completed due to an empty collection.
 */
#define FOS_EMPTY              (9)

/**
 * An operation expects one state, but finds another.
 */
#define FOS_STATE_MISMATCH     (10)




