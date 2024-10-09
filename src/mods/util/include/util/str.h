
#ifndef UTIL_STR_H
#define UTIL_STR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

bool str_eq(const char *s1, const char *s2);

// These all return the number of characters written excluding
// the NULL terminator.

// Writes at most 11 characters including the NULL terminator.
size_t str_of_u(char *buf, uint32_t u);

// Writes at most 12 characters including the NULL terminator.
size_t str_of_i(char *buf, int32_t i);

// Writes at most 9 characters including the NULL terminator.
// hex_digs = the number of hex digits to write.
size_t str_of_hex(char *buf, uint32_t u, uint8_t hex_digs);

// Returns true if tests succeed.
bool test_str(void);

#endif
