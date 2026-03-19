#ifndef BACKGROUND_H
#define BACKGROUND_H

#include "sheet_music_pixels.h"

/* Frame-buffer dimensions (needed by bg[][] size and callers) */
#define FB_WIDTH    320
#define FB_HEIGHT   240

/* Precomputed background: one RGB565 value per visible pixel.
   Exposed so restore_pixel (in vga_music_v2.c) can read bg[y][x]. */
extern short int bg[FB_HEIGHT][FB_WIDTH];

/* Build bg[][] from the sheet-music bitmap AND blit it to the
   frame buffer.  Call once at startup after pixel_buffer_start is set. */
void build_and_draw_background(void);

#endif
