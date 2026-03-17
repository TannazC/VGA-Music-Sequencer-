/*
 * vga.h — Low-level VGA pixel and character buffer interface
 *
 * Responsibilities:
 *   - Define VGA pixel buffer base address (VGA_PIXEL_BASE from config.h)
 *   - Define VGA character buffer base address (VGA_CHAR_BASE from config.h)
 *   - Define screen dimensions: SCREEN_W 320, SCREEN_H 240 (pixel mode)
 *   - Define character grid dimensions: CHAR_COLS 80, CHAR_ROWS 60
 *   - Declare vga_clear() — fills entire pixel buffer with a single color
 *   - Declare vga_draw_pixel(x, y, color) — writes one 16-bit RGB565 pixel
 *   - Declare vga_draw_rect(x, y, w, h, color) — fills a rectangle
 *   - Declare vga_draw_char(col, row, c, fg, bg) — writes one ASCII char
 *     to the character buffer at the given grid position
 *   - Declare vga_draw_string(col, row, str, fg, bg) — writes a null-terminated string
 *   - Define RGB565 color macros: COLOR_BLACK, COLOR_WHITE, COLOR_RED,
 *     COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW, COLOR_CYAN, COLOR_GRAY
 *   - Define a macro RGB565(r, g, b) to pack 8-bit r/g/b into a 16-bit value
 *
 * Notes:
 *   - Pixel buffer is word-addressed: address = VGA_PIXEL_BASE + (y << 9) + (x << 1)
 *   - Character buffer: address = VGA_CHAR_BASE + (row * CHAR_COLS + col) * 2
 *   - All drawing is immediate (no double buffering) unless noted in draw.c
 */
