#ifndef VGA_H
#define VGA_H

#include <stdint.h>
#include "config.h"

/* ── RGB565 color packing ────────────────────────────────────────────────────
 * Pack 8-bit r, g, b into a 16-bit RGB565 value.
 * r: 5 bits (bits 15-11), g: 6 bits (bits 10-5), b: 5 bits (bits 4-0)
 */
#define RGB565(r, g, b) \
    ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3)))

/* Common colors */
#define COLOR_BLACK   RGB565(  0,   0,   0)
#define COLOR_WHITE   RGB565(255, 255, 255)
#define COLOR_RED     RGB565(255,   0,   0)
#define COLOR_GREEN   RGB565(  0, 255,   0)
#define COLOR_BLUE    RGB565(  0,   0, 255)
#define COLOR_YELLOW  RGB565(255, 255,   0)
#define COLOR_CYAN    RGB565(  0, 255, 255)
#define COLOR_GRAY    RGB565(128, 128, 128)
#define COLOR_ORANGE  RGB565(255, 165,   0)

/* ── API ─────────────────────────────────────────────────────────────────────*/

/* Fill the entire screen with one color */
void vga_clear(uint16_t color);

/* Write a single pixel at (x, y). No-op if out of bounds. */
void vga_draw_pixel(int x, int y, uint16_t color);

/* Fill a rectangle at (x, y) with size w×h */
void vga_draw_rect(int x, int y, int w, int h, uint16_t color);

#endif /* VGA_H */