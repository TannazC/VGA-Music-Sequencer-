#include "background.h"

/* ═══════════════════════════════════════════════════════════════════════
   Colours RGB565
   ═══════════════════════════════════════════════════════════════════════ */
#define WHITE  ((short int)0xFFFF)
#define BLACK  ((short int)0x0000)

/* Precomputed background: one RGB565 value per visible pixel (320x240).
   Stored in a flat array so bg lookup is a single array read — no
   multiply, divide, or bit-extract at runtime.                          */
short int bg[FB_HEIGHT][FB_WIDTH];

/* Provided by vga_music_v2.c — background needs it to write to the fb */
extern int pixel_buffer_start;

/* ═══════════════════════════════════════════════════════════════════════
   Background precompute + draw
   ───────────────────────────────────────────────────────────────────────
   Called ONCE at startup. Iterates over 320x240 destination pixels,
   maps each to the 630x480 source bitmap using nearest-neighbour scale,
   stores result in bg[][] AND writes to the frame buffer.

   Key optimisation: sy = fy*2 (exact integer, no divide).
   sx uses a precomputed lookup table (320 entries) so no per-pixel divide.
   ═══════════════════════════════════════════════════════════════════════ */
#define BG_BYTES_ROW  79   /* ceil(630/8) */

void build_and_draw_background(void)
{
    int x, y;

    /* Precompute sx lookup: for each dst column fx, what source column? */
    /* sx = fx * 630 / 320  — compute once, store in small array          */
    static unsigned short sx_lut[FB_WIDTH];
    for (x = 0; x < FB_WIDTH; x++)
        sx_lut[x] = (unsigned short)((x * 630) / 320);

    /* Wipe the full 512x256 frame buffer to white (kills border garbage) */
    for (y = 0; y < 256; y++) {
        volatile short int *row =
            (volatile short int *)(pixel_buffer_start + (y << 10));
        for (x = 0; x < 512; x++)
            row[x] = WHITE;
    }

    /* Render sheet music into visible 320x240, filling bg[][] too */
    for (y = 0; y < FB_HEIGHT; y++) {
        int sy = y * 2;                        /* exact: 480/240 = 2     */
        const unsigned char *src_row =
            sheet_music_bitmap + sy * BG_BYTES_ROW;

        volatile short int *dst_row =
            (volatile short int *)(pixel_buffer_start + (y << 10));

        for (x = 0; x < FB_WIDTH; x++) {
            int sx      = sx_lut[x];
            int bit_off = 7 - (sx & 7);
            short int c = (src_row[sx >> 3] >> bit_off) & 1 ? BLACK : WHITE;
            bg[y][x] = c;
            dst_row[x] = c;
        }
    }
}
