
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

bool mem_cmp(const void *d0, const void *d1, size_t n);
void mem_cpy(void *dest, const void *src, size_t n);

bool mem_chk(const void *src, uint8_t val, size_t n);
void mem_set(void *dest, uint8_t val, size_t n);

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

// Writes dig + 1.
// All leading 0s, replaced with pad character.
size_t str_of_hex_pad(char *buf, uint32_t u, uint8_t digs, char pad);

// Writes at most 9 characters including the NULL terminator.
// Trims leading 0s.
size_t str_of_hex_no_pad(char *buf, uint32_t u);

// NOTE: Each of the below calls always write n + 1 characters!
// (That is they always write \0)

void str_la(char *buf, size_t n, char pad, const char *s);
void str_ra(char *buf, size_t n, char pad, const char *s);
void str_center(char *buf, size_t n, char pad, const char *s);

// Supported Specifiers:
// Returns number of characters written excluding \0
// %%       - The % sign
// %X       - 32-bit Hex Value (trims leading 0s)
// %u       - 32-bit Unsigned Integer
// %d       - 32-bit Signed Integer
// %s       - Pointer to a string.
size_t str_fmt(char *buf, const char *fmt,...);
size_t str_vfmt(char *buf, const char *fmt, va_list va);
