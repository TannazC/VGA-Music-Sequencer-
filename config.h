#ifndef CONFIG_H
#define CONFIG_H

/* VGA pixel buffer — word-addressed, 320x240, 16-bit RGB565 */
#define VGA_PIXEL_BASE  0xC8000000

/* Screen dimensions */
#define SCREEN_W  320
#define SCREEN_H  240

/* PS/2 ports */
#define PS2_BASE   0xFF200100   /* keyboard (port 1) */
#define PS2_BASE2  0xFF200108   /* mouse    (port 2, Y-splitter) */

#endif