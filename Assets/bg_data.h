/*
 * bg_data.h — Auto-generated VGA background pixel array
 *
 * DO NOT EDIT BY HAND — regenerate using: python3 convert.py bg.png bg_data.h
 *
 * Contents (once generated):
 *   - BG_WIDTH  320
 *   - BG_HEIGHT 240
 *   - uint16_t bg_pixels[320 * 240]
 *       RGB565 pixel data in row-major order, ready to blit to VGA_PIXEL_BASE
 *
 * Placeholder: replace this file by running convert.py on your background image.
 * Until then, bg.c will fall back to vga_clear() with a solid color.
 */

#ifndef BG_DATA_H
#define BG_DATA_H
#include <stdint.h>
#define BG_WIDTH  320
#define BG_HEIGHT 240
/* Placeholder: empty array — replace with output of convert.py */
static const uint16_t bg_pixels[1] = { 0x0000 };
#endif
