#include "vga.h"

/* Write one 16-bit pixel directly to the VGA pixel buffer.
 * Address formula from DE1-SoC_Computer_NiosV.pdf:
 *   addr = VGA_PIXEL_BASE + (y << 9) + (x << 1)
 * The row stride is 512 bytes (2^9), not 320*2=640, because the
 * pixel buffer is padded to the next power-of-two width.
 */
static inline void write_pixel(int x, int y, uint16_t color) {
    volatile uint16_t *ptr =
        (volatile uint16_t *)(VGA_PIXEL_BASE + (y << 9) + (x << 1));
    *ptr = color;
}

void vga_clear(uint16_t color) {
    int x, y;
    for (y = 0; y < SCREEN_H; y++)
        for (x = 0; x < SCREEN_W; x++)
            write_pixel(x, y, color);
}

void vga_draw_pixel(int x, int y, uint16_t color) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H)
        return;
    write_pixel(x, y, color);
}

void vga_draw_rect(int x, int y, int w, int h, uint16_t color) {
    int px, py;
    for (py = y; py < y + h; py++)
        for (px = x; px < x + w; px++)
            vga_draw_pixel(px, py, color);
}