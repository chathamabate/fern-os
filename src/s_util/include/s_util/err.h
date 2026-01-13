#pragma once

#include <stdint.h>

typedef uint16_t fernos_error_t;

#define FOS_E_SUCCESS       (0)

#define FOS_E_UNKNWON_ERROR (1)

/**
 * A value which was expected to be aligned was not.
 */
#define FOS_E_ALIGN_ERROR   (2)

/**
 * Given when a range's end is less than a range's start.
 */
#define FOS_E_INVALID_RANGE (3)

/**
 * A given argument is invalid for an unspecified reason.
 */
#define FOS_E_BAD_ARGS    (4)

/**
 * Lack of memory stopped the behavoir from completing.
 */
#define FOS_E_NO_MEM      (5)

/**
 * We attempted to allocate an area which already was in use.
 */
#define FOS_E_ALREADY_ALLOCATED (6)

/**
 * You tried to allocate/deallocate pages in the identity area.
 */
#define FOS_E_IDENTITY_OVERWRITE (7)

/**
 * You gave an index which is invalid.
 */
#define FOS_E_INVALID_INDEX      (8)

/**
 * An operation cannot be completed due to an empty collection.
 */
#define FOS_E_EMPTY              (9)

/**
 * An operation expects one state, but finds another.
 */
#define FOS_E_STATE_MISMATCH     (10)

/**
 * An extremely signficant event occured, the entire system should stop.
 */
#define FOS_E_ABORT_SYSTEM       (11)

/**
 * A requested behavior isn't implemented yet!
 */
#define FOS_E_NOT_IMPLEMENTED    (12)

/**
 * A resource is in use by some other party.
 */ 
#define FOS_E_IN_USE             (13)

/**
 * Basically FOS_E_NO_MEM, but no pertaining so some other type of
 * storage. (Like disk space)
 */
#define FOS_E_NO_SPACE           (14)

/**
 * A requested behavior is valid, but not allowed.
 */
#define FOS_E_NOT_PERMITTED      (15)

/**
 * An error has occured which more significant than other possible errors.
 */
#define FOS_E_FATAL              (16)




