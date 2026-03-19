#ifndef BACKGROUND_H
#define BACKGROUND_H

/* ── Frame-buffer dimensions ── */
#define FB_WIDTH    320
#define FB_HEIGHT   240

/* ── Staff layout ───────────────────────────────────────────────────────
   4 lines per staff, 12 px between lines → each staff is 36 px tall.
   Two staves fit comfortably in 240 px with room between them.          */
#define NUM_STAVES      4
#define LINES_PER_STAFF 5
#define STAFF_SPACING   6
#define STAFF_X0        20
#define STAFF_X1       (FB_WIDTH - 10)

/* Precomputed background array — read by restore_pixel in vga_music_v2.c */
extern short int bg[FB_HEIGHT][FB_WIDTH];

/* Build bg[][] procedurally and blit to frame buffer. Call once at
   startup after pixel_buffer_start is set.                              */
void build_and_draw_background(void);

#endif