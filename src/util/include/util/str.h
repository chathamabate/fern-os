
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

bool str_eq(const char *s1, const char *s2);

// NOTE: Unless specified otherwise, all calls that return size_t
// return the number of characters written excluding \0.

// Returns str_len(src)
size_t str_cpy(char *dest, const char *src);
size_t str_len(const char *src);

// Writes at most 11 characters including the NULL terminator.
size_t str_of_u(char *buf, uint32_t u);

// Writes at most 12 characters including the NULL terminator.
size_t str_of_i(char *buf, int32_t i);

// Writes at most 9 characters including the NULL terminator.
// hex_digs = the number of hex digits to write.
size_t str_of_hex(char *buf, uint32_t u, uint8_t hex_digs);

// NOTE: Each of the below calls always write n + 1 characters!
// (That is they always write \0)

void str_la(char *buf, size_t n, char pad, const char *s);
void str_ra(char *buf, size_t n, char pad, const char *s);
void str_center(char *buf, size_t n, char pad, const char *s);

// Supported Specifiers:
// Returns number of characters written excluding \0
// %%   - The % sign
// %x   - 32-bit Hex Value
// %u   - 32-bit Unsigned Integer
// %d   - 32-bit Signed Integer
// %s   - Pointer to a string.
size_t str_fmt(char *buf, const char *fmt,...);
size_t str_vfmt(char *buf, const char *fmt, va_list va);

// Returns true if tests succeed.
bool test_str(void);

