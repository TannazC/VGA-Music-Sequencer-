/*
 * vga.c — Low-level VGA pixel and character buffer implementation
 *
 * Implement everything declared in vga.h:
 *
 *   vga_clear(uint16_t color)
 *     - Loop over all (x, y) from (0,0) to (SCREEN_W-1, SCREEN_H-1)
 *     - Write color to each pixel address
 *     - Used at startup and when switching display states
 *
 *   vga_draw_pixel(int x, int y, uint16_t color)
 *     - Bounds-check: return immediately if x or y out of range
 *     - Compute address: VGA_PIXEL_BASE + (y << 9) + (x << 1)
 *     - Write color as a 16-bit half-word using a volatile pointer
 *
 *   vga_draw_rect(int x, int y, int w, int h, uint16_t color)
 *     - Call vga_draw_pixel in a nested x/y loop
 *     - Used by draw.c to fill grid cells and UI panels
 *
 *   vga_draw_char(int col, int row, char c, uint16_t fg, uint16_t bg)
 *     - Compute character buffer address
 *     - Write fg color and ASCII char value (see DE1-SoC char buffer format)
 *
 *   vga_draw_string(int col, int row, const char *str, uint16_t fg, uint16_t bg)
 *     - Iterate over str, calling vga_draw_char for each character
 *     - Advance col; do not wrap lines
 */
