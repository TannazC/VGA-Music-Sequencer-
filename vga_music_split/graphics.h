#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "vga_music.h"

/* Low-level pixel write */
void plot_pixel(int x, int y, short int c);

/* Filled rectangle */
void fill_rect(int x, int y, int w, int h, short int c);

/* Returns 1 if (x,y) is on a drawn staff line */
int is_staff_line(int x, int y);

/* Returns the background colour at (x,y) — WHITE or BLACK (staff line) */
short int bg_color(int x, int y);

/*
 * Restore a single pixel to its "natural" background state.
 * Priority: dot > menubar grey > staff line > white
 */
void restore_pixel(int x, int y);

#endif /* GRAPHICS_H */
