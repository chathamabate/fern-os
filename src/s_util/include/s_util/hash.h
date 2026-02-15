
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "s_util/str.h"

/*
 * NOTE: you may find it strange there is an equator and comparator type.
 * You can determine if two values are equal using a comparator, why have an
 * equator?
 *
 * Well, you may have a set of values which has a notion of equality, but no
 * notion of order. If you only care about equality, you should always use
 * an equator to broaden what values are accepted.
 */

/**
 * A Comparator returns 0 iff `k0` and `k1` point to equal emantically values.
 * Returns a negative number iff `k0`'s value is "less than" `k1`'s value.
 * Returns a positive number iff `k0`'s value is "greater than" `k1`'s value.
 */
typedef int32_t (*comparator_ft)(const void *k0, const void *k1);

/**
 * An equator should return true iff `k0` and `k1` point to semantically equal
 * values.
 */
typedef bool (*equator_ft)(const void *k0, const void *k1); 

/**
 * A hasher should return some 32-bit integer generated from the value `k` points
 * to.
 *
 * Too values which are semantically equivalent MUST return the same hash value.
 * It is ok, for two values which are not equivalent to also return the same hash value.
 */
typedef uint32_t (*hasher_ft)(const void *k);

/**
 * Equator function for NULL terminatoed C strings.
 *
 * `k0` and `k1` should really be of type `const char * const *`
 */
bool str_equator(const void *k0, const void *k1);

/**
 * Hash function for NULL terminatoed C strings.
 *
 * `k` should really be of type `const char * const *`
 */
uint32_t str_hasher(const void *k);
