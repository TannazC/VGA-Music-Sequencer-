#include "vga_music.h"
#include "graphics.h"
#include "charbuf.h"
#include "treble_clef_bitmap.h"
#include "background.h"

/* Staff top-row Y coordinates (one per staff) */
const int staff_top[NUM_STAFFS] = { 40, 95, 150, 195 };

/* ── Menu bar ─────────────────────────────────────────────────────────── */
void draw_menubar(void)
{
    fill_rect(0,            0,       FB_WIDTH, MENUBAR_H, DARK_GREY);
    fill_rect(BTN_PLAY_X,   BTN_Y,   BTN_W, BTN_H, GREEN);
    fill_rect(BTN_STOP_X,   BTN_Y,   BTN_W, BTN_H, RED);
    fill_rect(BTN_SPD_UP_X, BTN_Y,   BTN_W, BTN_H, BLUE);
    fill_rect(BTN_SPD_DN_X, BTN_Y,   BTN_W, BTN_H, ORANGE);
}

void draw_menubar_labels(void)
{
    char_buf_puts(CLABEL_PLAY,  CLABEL_ROW, "PLAY");
    char_buf_puts(CLABEL_STOP,  CLABEL_ROW, "STOP");
    char_buf_puts(CLABEL_SPDUP, CLABEL_ROW, "SPD+");
    char_buf_puts(CLABEL_SPDN,  CLABEL_ROW, "SPD-");
}

/* ── Staff lines ──────────────────────────────────────────────────────── */
void draw_staff_lines(void)
{
    int s, l, x;
    for (s = 0; s < NUM_STAFFS; s++)
        for (l = 0; l < NUM_LINES; l++)
            for (x = STAFF_LEFT; x <= STAFF_RIGHT; x++)
                plot_pixel(x, staff_top[s] + l * LINE_GAP, BLACK);
}

/* ── Treble clef ──────────────────────────────────────────────────────── */
/*
 * Source region inside sheet_music_bitmap (1-bpp, 630×480, 79 bytes/row):
 *   rows    10 .. 102  (93 source rows)
 *   columns 38 .. 81   (44 source columns)
 *
 * These are nearest-neighbour scaled down to CLEF_W × CLEF_H pixels.
 * Only set bits (black pixels) are drawn; white is transparent so the
 * staff lines remain visible through the clef body.
 *
 * FIX vs original: ox now receives STAFF_LEFT - CLEF_W - 2 so the glyph
 * sits fully to the LEFT of the staff lines and cannot be clipped by the
 * screen's left edge (minimum ox = 8 for a 18-px glyph → x range 8..25).
 */
void draw_treble_clef(int ox, int oy)
{
    /* Source region in the bitmap */
    static const int SRC_X0 = 38;   /* first source column               */
    static const int SRC_W  = 44;   /* source width  (columns)           */
    static const int SRC_Y0 = 10;   /* first source row                  */
    static const int SRC_H  = 93;   /* source height (rows)              */

    int row, col;

    for (row = 0; row < CLEF_H; row++) {
        int src_y = SRC_Y0 + (row * SRC_H) / CLEF_H;
        for (col = 0; col < CLEF_W; col++) {
            int src_x    = SRC_X0 + (col * SRC_W) / CLEF_W;
            int byte_idx = src_y * BITMAP_BYTES_PER_ROW + (src_x >> 3);
            int bit_idx  = 7 - (src_x & 7);
            if ((sheet_music_bitmap[byte_idx] >> bit_idx) & 1)
                plot_pixel(ox + col, oy + row, BLACK);
        }
    }
}

/* ── Full background ──────────────────────────────────────────────────── */
void draw_background(void)
{
    int x, y;
    int s;

    /* Wipe the full 512×256 frame buffer to white */
    for (y = 0; y < 256; y++) {
        volatile short int *row_ptr =
            (volatile short int *)(pixel_buffer_start + (y << 10));
        for (x = 0; x < 512; x++)
            row_ptr[x] = WHITE;
    }

    draw_menubar();
    draw_staff_lines();

    /*
     * Place the treble clef just to the LEFT of the staff lines.
     * ox = STAFF_LEFT - CLEF_W - 2  ensures no overlap with staff content
     * and the glyph never goes off the left edge (STAFF_LEFT >= 28,
     * CLEF_W = 18, so ox >= 8 — well clear of x=0).
     *
     * oy = staff_top[s] - 15 vertically centres the tall clef over
     * the five staff lines (staff span = 4*LINE_GAP = 24 px; the clef
     * is 56 px tall so it naturally extends above and below).
     */
    for (s = 0; s < NUM_STAFFS; s++)
        draw_treble_clef(STAFF_LEFT - CLEF_W - 2, staff_top[s] - 15);

    /* Character-buffer labels (persist automatically, no per-frame cost) */
    draw_menubar_labels();
}
