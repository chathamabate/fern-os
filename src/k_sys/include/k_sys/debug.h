
#pragma once

#include <stdint.h>
#include <stdint.h>

uint32_t read_eflags(void);

uint32_t read_cr0(void);

// Returns the value of the stack pointer
// before this function is called.
uint32_t *read_esp(void);

uint32_t read_cs(void);
uint32_t read_ds(void);

// This function just stops the computer basically.
// YOU CANNOT RECOVER FROM THIS.
void lock_up(void);

/*
 * Basic helpers for using the BIOS VGA Terminal.
 */

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
static volatile uint16_t * const TERMINAL_BUFFER = (uint16_t *)0xB8000;

/* Hardware text mode color constants. */
typedef enum _vga_color_t {
	VGA_COLOR_BLACK = 0,
	VGA_COLOR_BLUE = 1,
	VGA_COLOR_GREEN = 2,
	VGA_COLOR_CYAN = 3,
	VGA_COLOR_RED = 4,
	VGA_COLOR_MAGENTA = 5,
	VGA_COLOR_BROWN = 6,
	VGA_COLOR_LIGHT_GREY = 7,
	VGA_COLOR_DARK_GREY = 8,
	VGA_COLOR_LIGHT_BLUE = 9,
	VGA_COLOR_LIGHT_GREEN = 10,
	VGA_COLOR_LIGHT_CYAN = 11,
	VGA_COLOR_LIGHT_RED = 12,
	VGA_COLOR_LIGHT_MAGENTA = 13,
	VGA_COLOR_LIGHT_BROWN = 14,
	VGA_COLOR_WHITE = 15,
} vga_color_t;

static inline uint8_t vga_entry_color(vga_color_t fg, vga_color_t bg) {
	return fg | bg << 4;
}

static inline uint8_t vga_color_flip(uint8_t color) {
    return (color << 4) | (color >> 4);
}
 
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
	return (uint16_t) uc | (uint16_t) color << 8;
}

static inline uint8_t vga_extract_color(uint16_t entry) {
    return (uint8_t)(entry >> 8);
}

static inline vga_color_t vga_extract_bg_color(uint8_t color) {
    return (vga_color_t)(color >> 4);
}

static inline vga_color_t vga_extract_fg_color(uint8_t color) {
    return (vga_color_t)(color & 0x0F);
}

static inline unsigned char vga_extract_char(uint16_t entry) {
    return (unsigned char)(entry & 0xFF);
}

static inline volatile uint16_t *vga_get_entry_p(uint32_t row, uint32_t col) {
    return &(TERMINAL_BUFFER[(row * VGA_WIDTH) + col]);
}

static inline void vga_set_entry(uint32_t row, uint32_t col, uint16_t entry) {
    *vga_get_entry_p(row, col) = entry;
}

static inline uint16_t vga_get_entry(uint32_t row, uint32_t col) {
    return *vga_get_entry_p(row, col);
}

static inline void vga_set_entry_color_p(volatile uint16_t *entry, uint8_t color) {
    *entry = (*entry & 0x00FF) | ((uint16_t)color << 8);
}

static inline void vga_set_entry_color(uint32_t row, uint32_t col, uint8_t color) {
    vga_set_entry_color_p(vga_get_entry_p(row, col), color);
}

static inline void vga_set_entry_char_p(volatile uint16_t *entry, unsigned char c) {
    *entry = (*entry & 0xFF00) | (uint16_t)c;
}

static inline void vga_set_entry_char(uint32_t row, uint32_t col, unsigned char c) {
    vga_set_entry_char_p(vga_get_entry_p(row, col), c);
}

/**
 * Write a string directly to the BIOS VGA Terminal.
 */
void out_bios_vga(uint8_t style, const char *str);

