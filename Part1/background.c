#include "background.h"
#include "treble_clef_bitmap.h"

/* ═══════════════════════════════════════════════════════════════════════
   Colours RGB565
   ═══════════════════════════════════════════════════════════════════════ */
#define WHITE  ((short int)0xFFFF)
#define BLACK  ((short int)0x0000)

/* Precomputed background: one RGB565 value per visible pixel (320x240). */
short int bg[FB_HEIGHT][FB_WIDTH];

/* Provided by vga_music_v2.c */
extern int pixel_buffer_start;

/* Top line of each staff (screen y-coordinate) */
static const int staff_top[NUM_STAVES] = { 60, 100, 140, 180};

/* ═══════════════════════════════════════════════════════════════════════
   Helper: write one pixel to both bg[][] and the frame buffer.
   BOUNDS CHECK REQUIRED — an out-of-range write here silently corrupts
   pixel_buffer_start and kills the mouse.
   ═══════════════════════════════════════════════════════════════════════ */
static void bg_plot(int x, int y, short int c)
{
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;
    bg[y][x] = c;
    *(volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = c;
}

/* ═══════════════════════════════════════════════════════════════════════
   Draw staff lines
   ═══════════════════════════════════════════════════════════════════════ */
static void draw_staves(void)
{
    int s, l, x;
    for (s = 0; s < NUM_STAVES; s++) {
        for (l = 0; l < LINES_PER_STAFF; l++) {
            int y = staff_top[s] + l * STAFF_SPACING;
            for (x = STAFF_X0; x < STAFF_X1; x++)
                bg_plot(x, y, BLACK);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   Draw vertical bar lines (left + right ends of each staff)
   ═══════════════════════════════════════════════════════════════════════ */
static void draw_barlines(void)
{
    int s, y;
    for (s = 0; s < NUM_STAVES; s++) {
        int y0 = staff_top[s];
        int y1 = staff_top[s] + (LINES_PER_STAFF - 1) * STAFF_SPACING;
        for (y = y0; y <= y1; y++) {
            bg_plot(STAFF_X0,     y, BLACK);
            bg_plot(STAFF_X1 - 1, y, BLACK);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   Draw treble clef from bitmap
   ═══════════════════════════════════════════════════════════════════════ */
static void draw_treble_clef(int x0, int y0)
{
    int row, col;
    for (row = 0; row < CLEF_BMP_H; row++) {
        unsigned short bits = treble_clef_bmp[row];
        for (col = 0; col < CLEF_BMP_W; col++) {
            if (bits & (1 << (CLEF_BMP_W - 1 - col)))
                bg_plot(x0 + col, y0 + row, BLACK);  /* bg_plot guards bounds */
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   build_and_draw_background
   ═══════════════════════════════════════════════════════════════════════ */
void build_and_draw_background(void)
{
    int x, y, s;

    /* Fill entire frame buffer (including non-visible border) white */
    for (y = 0; y < 256; y++) {
        volatile short int *row =
            (volatile short int *)(pixel_buffer_start + (y << 10));
        for (x = 0; x < 512; x++)
            row[x] = WHITE;
    }

    /* Initialise bg[][] to white */
    for (y = 0; y < FB_HEIGHT; y++)
        for (x = 0; x < FB_WIDTH; x++)
            bg[y][x] = WHITE;

    draw_staves();
    draw_barlines();

    /* Treble clef: top of bitmap sits one STAFF_SPACING above the top
       staff line so the curl aligns with the correct pitch position.   */
    for (s = 0; s < NUM_STAVES; s++)
        draw_treble_clef(STAFF_X0 + 1, staff_top[s] - STAFF_SPACING);
}