/* Auto-generated combined C file */

/* =========================================
   Start of treble_clef_bitmap.h
   ========================================= */
#ifndef TREBLE_CLEF_BITMAP_H
#define TREBLE_CLEF_BITMAP_H

/* ═══════════════════════════════════════════════════════════════════════
   Treble clef bitmap  —  12 columns × 36 rows
   ───────────────────────────────────────────────────────────────────────
   Derived from uploaded treble clef image, scaled to fit the staff layout:
     STAFF_SPACING = 6 px, LINES_PER_STAFF = 5
     Glyph spans: 1 space above top line → 1 space below bottom line
                  = 6 + (4×6) + 6 = 36 px total height

   Each entry is one row. Bit 11 = column 0 (leftmost), bit 0 = column 11.
   # = black pixel drawn,  . = transparent (background shows through)
   ═══════════════════════════════════════════════════════════════════════ */

#define CLEF_BMP_W  12
#define CLEF_BMP_H  36

static const unsigned short treble_clef_bmp[CLEF_BMP_H] = {
    /* row  0 */  0x038,  /* ......###... */
    /* row  1 */  0x078,  /* .....####... */
    /* row  2 */  0x06C,  /* .....##.##.. */
    /* row  3 */  0x044,  /* .....#...#.. */
    /* row  4 */  0x044,  /* .....#...#.. */
    /* row  5 */  0x0C4,  /* ....##...#.. */
    /* row  6 */  0x0CC,  /* ....##..##.. */
    /* row  7 */  0x08C,  /* ....#...##.. */
    /* row  8 */  0x09C,  /* ....#..###.. */
    /* row  9 */  0x058,  /* .....#.##... */
    /* row 10 */  0x070,  /* .....###.... */
    /* row 11 */  0x070,  /* .....###.... */
    /* row 12 */  0x0E0,  /* ....###..... */
    /* row 13 */  0x1E0,  /* ...####..... */
    /* row 14 */  0x3C0,  /* ..####...... */
    /* row 15 */  0x3E0,  /* ..#####..... */
    /* row 16 */  0x720,  /* .###..#..... */
    /* row 17 */  0xE38,  /* ###...###... */
    /* row 18 */  0xE7C,  /* ###..#####.. */
    /* row 19 */  0xCFE,  /* ##..#######. */
    /* row 20 */  0xCF7,  /* ##..####.### */
    /* row 21 */  0xC93,  /* ##..#..#..## */
    /* row 22 */  0xC93,  /* ##..#..#..## */
    /* row 23 */  0x493,  /* .#..#..#..## */
    /* row 24 */  0x413,  /* .#.....#..## */
    /* row 25 */  0x202,  /* ..#.......#. */
    /* row 26 */  0x18C,  /* ...##...##.. */
    /* row 27 */  0x078,  /* .....####... */
    /* row 28 */  0x008,  /* ........#... */
    /* row 29 */  0x000,  /* .........#.. */
    /* row 30 */  0x0C4,  /* ....##...#.. */
    /* row 31 */  0x1E4,  /* ...####..#.. */
    /* row 32 */  0x1E4,  /* ...####..#.. */
    /* row 33 */  0x1E4,  /* ...####..#.. */
    /* row 34 */  0x1C8,  /* ...###..#... */
    /* row 35 */  0x0F0,  /* ....####.... */
};

#endif /* TREBLE_CLEF_BITMAP_H */
/* =========================================
   End of treble_clef_bitmap.h
   ========================================= */

/* =========================================
   Start of background.h
   ========================================= */
#ifndef BACKGROUND_H
#define BACKGROUND_H

/* ── Frame-buffer dimensions ── */
#define FB_WIDTH    320
#define FB_HEIGHT   240

/* ============== Staff layout ================
   4 lines per staff, 12 px between lines  so each staff is 36 px tall.
   Two staves fit comfortably in 240 px with room between them.          */
#define NUM_STAVES      4
#define LINES_PER_STAFF 5
#define STAFF_SPACING   6
#define STAFF_X0        20
#define STAFF_X1       (FB_WIDTH - 10)


/* Shared across background.c and vga_music_v2.c */
extern const int staff_top[NUM_STAVES];
extern short int bg[FB_HEIGHT][FB_WIDTH]; //precomputed background array, read by restore_pixel in vga_music_v2.c

/* Build bg[][] procedurally and blit to frame buffer. Call once at
   startup after pixel_buffer_start is set.                              */
void build_and_draw_background(void);

#endif
/* =========================================
   End of background.h
   ========================================= */

/* =========================================
   Start of background.c
   ========================================= */
// Skipped local include by merge script: #include "background.h"
// Skipped local include by merge script: #include "treble_clef_bitmap.h"

/* ═══════════════════════════════════════════════════════════════════════
   Colours (RGB565 format)
   ═══════════════════════════════════════════════════════════════════════ */
#define WHITE  ((short int)0xFFFF)
#define BLACK  ((short int)0x0000)

/* ═══════════════════════════════════════════════════════════════════════
   Background buffer

   Stores one colour per visible pixel (320 x 240).
   This acts as a "ground truth" copy of the screen so we can restore
   pixels correctly when the cursor moves.

   Important:
   We NEVER read back from VGA memory directly — we always use bg[][]
   ═══════════════════════════════════════════════════════════════════════ */
short int bg[FB_HEIGHT][FB_WIDTH];

/* Provided by main VGA file — this is the base address of the frame buffer */
extern int pixel_buffer_start;

/* PS/2 hardware (used only for FIFO flush at the end) */
#define PS2_BASE    0xFF200100
#define PS2_RVALID  0x8000

const int staff_top[NUM_STAVES] = { 60, 100, 140, 180 };

/* ═══════════════════════════════════════════════════════════════════════
   bg_plot

   Writes a pixel BOTH:
     1. into bg[][] (software copy)
     2. into VGA memory (hardware display)

   This is critical because:
     - bg[][] is used later to restore pixels (cursor erase)
     - VGA memory is what actually shows on screen

   Parameters:
     x -> x-coordinate
     y -> y-coordinate
     c -> colour (RGB565)

   Input:
     pixel position + colour

   Output:
     none

   Side effects:
     updates both software buffer AND hardware frame buffer

   WARNING:
     Bounds checking is mandatory — writing out of bounds corrupts
     memory and can break unrelated hardware (like PS/2 mouse).
   ═══════════════════════════════════════════════════════════════════════ */
static void bg_plot(int x, int y, short int c)
{
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;

    bg[y][x] = c;

    *(volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = c;
}

/* ═══════════════════════════════════════════════════════════════════════
   draw_staves

   Draws the 5 horizontal lines for each musical staff.

   Structure:
     - NUM_STAVES staffs total
     - each staff has LINES_PER_STAFF lines
     - vertical spacing between lines = STAFF_SPACING

   Parameters:
     none

   Input:
     constants (staff positions, spacing)

   Output:
     none

   Side effect:
     draws horizontal black lines into bg[][] and VGA
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
   draw_barlines

   Draws vertical boundary lines at the left and right edges of each staff.

   These define the start and end of each measure visually.

   Parameters:
     none

   Input:
     staff positions

   Output:
     none

   Side effect:
     draws vertical black lines into bg[][] and VGA
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
   draw_treble_clef

   Draws a treble clef using a bitmap (bitmask per row).

   How it works:
     - each row is a 16-bit value (treble_clef_bmp[row])
     - each bit represents whether a pixel should be drawn
     - we scan across bits and draw where bit = 1

   Parameters:
     x0 -> left position where bitmap starts
     y0 -> top position where bitmap starts

   Input:
     bitmap + placement coordinates

   Output:
     none

   Side effect:
     draws the clef into bg[][] and VGA

   Note:
     bg_plot handles bounds checking, so no need here
   ═══════════════════════════════════════════════════════════════════════ */
static void draw_treble_clef(int x0, int y0)
{
    int row, col;

    for (row = 0; row < CLEF_BMP_H; row++) {

        unsigned short bits = treble_clef_bmp[row];

        for (col = 0; col < CLEF_BMP_W; col++) {

            /* Check if this bit is set (pixel should be drawn) */
            if (bits & (1 << (CLEF_BMP_W - 1 - col)))
                bg_plot(x0 + col, y0 + row, BLACK);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   build_and_draw_background

   Builds the entire background once at startup.

   Steps:
     1. clear full VGA memory (including hidden region)
     2. initialise bg[][] to match (all white)
     3. draw musical staves
     4. draw barlines
     5. draw treble clefs
     6. flush PS/2 FIFO to avoid corrupted mouse packets

   Parameters:
     none

   Input:
     none

   Output:
     none

   Side effects:
     fully initializes the screen and bg[][] buffer
   ═══════════════════════════════════════════════════════════════════════ */
void build_and_draw_background(void)
{
    int x, y, s;

    /* ── Step 1: clear full frame buffer (512 x 256, not just visible area) ──
       This ensures no garbage remains in off-screen memory */
    for (y = 0; y < 256; y++) {

        volatile short int *row =
            (volatile short int *)(pixel_buffer_start + (y << 10));

        for (x = 0; x < 512; x++)
            row[x] = WHITE;
    }

    /* ── Step 2: initialise bg[][] to match screen (all white) ── */
    for (y = 0; y < FB_HEIGHT; y++)
        for (x = 0; x < FB_WIDTH; x++)
            bg[y][x] = WHITE;

    /* ── Step 3–4: draw staff structure ── */
    draw_staves();
    draw_barlines();

    /* ── Step 5: draw treble clefs ──
       Positioned slightly above the top staff line so that the
       spiral aligns with the correct pitch reference */
    for (s = 0; s < NUM_STAVES; s++)
        draw_treble_clef(STAFF_X0 + 1,
                         staff_top[s] - STAFF_SPACING);

    /* ── Step 6: flush PS/2 FIFO ─────────────────────────────────────────
       Why this matters:

       Drawing the background takes a noticeable amount of time.
       During this time, the mouse continues sending movement bytes.

       If we do NOT flush:
         - leftover bytes stay in the FIFO
         - packet alignment breaks
         - main loop misinterprets data
         - mouse appears "frozen" or glitchy

       Fix:
         drain everything before entering main loop
       ─────────────────────────────────────────────────────────────────── */
    {
        volatile int *ps2 = (volatile int *)PS2_BASE;

        while (*ps2 & PS2_RVALID)
            (void)(*ps2);
    }
}

/* =========================================
   End of background.c
   ========================================= */

/* =========================================
   Start of vga_music_v2.c
   ========================================= */
#include <stdlib.h>
// Skipped local include by merge script: #include "background.h"

/* Hardware addresses */
#define PIXEL_BUF_CTRL  0xFF203020
#define PS2_BASE        0xFF200100
#define PS2_RVALID      0x8000

/* Arrow glyph */
#define ARROW_W         11
#define ARROW_H         16

/* Mouse speed */
#define SPEED_DIV       2

/* Note head oval size (half-widths) */
#define NOTE_RX         4          /* horizontal radius                   */
#define NOTE_RY         3          /* vertical radius                     */

/* Max notes on screen */
#define MAX_NOTES       256

/* Snap threshold: only snap if cursor is within this many px of a staff */
#define SNAP_THRESH     (STAFF_SPACING * LINES_PER_STAFF)

/* Colours RGB565 */
#define WHITE  ((short int)0xFFFF)
#define BLACK  ((short int)0x0000)

/* ═══════════════════════════════════════════════════════════════════════
   Globals
   ═══════════════════════════════════════════════════════════════════════ */
int pixel_buffer_start;

/* bg[][] defined in background.c */
extern short int bg[FB_HEIGHT][FB_WIDTH];


/* ── CHANGED: replaced dot_x/dot_y arrays with a Note struct ── */
typedef struct {
    int x;          /* snapped screen x (column centre)  */
    int y;          /* snapped screen y (pitch centre)   */
} Note;

Note notes[MAX_NOTES];
int  num_notes = 0;

/* ═══════════════════════════════════════════════════════════════════════
   Pixel helpers
   ═══════════════════════════════════════════════════════════════════════ */
void plot_pixel(int x, int y, short int c)
{
    if (x < 0 || x >= FB_WIDTH)  return;
    if (y < 0 || y >= FB_HEIGHT) return;
    *(volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = c;
}

/* ── CHANGED: restore_pixel now checks notes[] oval instead of dot circle ── */
void restore_pixel(int x, int y)
{
    int i;
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;

    for (i = 0; i < num_notes; i++) {
        int ddx = x - notes[i].x;
        int ddy = y - notes[i].y;
        /* Ellipse test: (ddx/NOTE_RX)^2 + (ddy/NOTE_RY)^2 <= 1
           Multiply through to avoid floats:
           (ddx*NOTE_RY)^2 + (ddy*NOTE_RX)^2 <= (NOTE_RX*NOTE_RY)^2  */
        int a = ddx * NOTE_RY;
        int b = ddy * NOTE_RX;
        int r = NOTE_RX * NOTE_RY;
        if (a*a + b*b <= r*r) {
            plot_pixel(x, y, BLACK);
            return;
        }
    }
    plot_pixel(x, y, bg[y][x]);
}

/* ═══════════════════════════════════════════════════════════════════════
   NEW: snap_to_staff
   Given cursor y, find the nearest staff and pitch slot.
   Pitch slots: 0 = top line, 1 = space below, 2 = next line, ...
   (LINES_PER_STAFF=5 lines + 4 spaces = 9 slots per staff, index 0–8)
   Returns the snapped screen-y, or -1 if not close enough to any staff.
   ═══════════════════════════════════════════════════════════════════════ */
int snap_to_staff(int cy, int *staff_out, int *pitch_out)
{
    int s;
    int best_dist  = SNAP_THRESH + 1;
    int best_staff = -1;
    int best_slot  = 0;

    for (s = 0; s < NUM_STAVES; s++) {
        int slot;
        /* Each half-slot is STAFF_SPACING/2 pixels.
           Slots run from the top line downward in half-spacing steps.
           We have 9 slots (0..8) covering the full staff height.       */
        for (slot = 0; slot <= (LINES_PER_STAFF - 1) * 2; slot++) {
            /* slot 0 = top staff line, slot 2 = next line, etc.
               odd slots are spaces between lines                        */
            int slot_y = staff_top[s] + slot * (STAFF_SPACING / 2);
            int dist   = cy - slot_y;
            if (dist < 0) dist = -dist;
            if (dist < best_dist) {
                best_dist  = dist;
                best_staff = s;
                best_slot  = slot;
            }
        }
    }

    if (best_staff < 0) return -1;

    *staff_out = best_staff;
    *pitch_out = best_slot;
    return staff_top[best_staff] + best_slot * (STAFF_SPACING / 2);
}

/* ═══════════════════════════════════════════════════════════════════════
   NEW: draw_note_head  — filled black oval at (cx, cy)
   ═══════════════════════════════════════════════════════════════════════ */
void draw_note_head(int cx, int cy)
{
    int dx, dy;
    for (dy = -NOTE_RY; dy <= NOTE_RY; dy++) {
        for (dx = -NOTE_RX; dx <= NOTE_RX; dx++) {
            /* Same ellipse test as restore_pixel */
            int a = dx * NOTE_RY;
            int b = dy * NOTE_RX;
            int r = NOTE_RX * NOTE_RY;
            if (a*a + b*b <= r*r)
                plot_pixel(cx + dx, cy + dy, BLACK);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   NEW: erase_note_head — restores pixels under oval using bg[][]
   ═══════════════════════════════════════════════════════════════════════ */
void erase_note_head(int cx, int cy)
{
    int dx, dy;
    for (dy = -NOTE_RY; dy <= NOTE_RY; dy++) {
        for (dx = -NOTE_RX; dx <= NOTE_RX; dx++) {
            int a = dx * NOTE_RY;
            int b = dy * NOTE_RX;
            int r = NOTE_RX * NOTE_RY;
            if (a*a + b*b <= r*r)
                plot_pixel(cx + dx, cy + dy, bg[cy + dy][cx + dx]);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   Arrow cursor
   ═══════════════════════════════════════════════════════════════════════ */
static const unsigned char AX0[16] = {0,0,0,0,0,0,0,0,0,0, 0,3,3,3,3,3};
static const unsigned char AX1[16] = {0,1,2,3,4,5,6,7,8,9,10,6,6,6,6,6};

void draw_arrow(int tx, int ty)
{
    int row, col;
    for (row = 0; row < ARROW_H; row++)
        for (col = AX0[row]; col <= AX1[row]; col++)
            plot_pixel(tx + col, ty + row, BLACK);
}

void erase_arrow(int tx, int ty)
{
    int row, col;
    for (row = 0; row < ARROW_H; row++)
        for (col = AX0[row]; col <= AX1[row]; col++)
            restore_pixel(tx + col, ty + row);
}

/* ═══════════════════════════════════════════════════════════════════════
   PS/2 helpers
   ═══════════════════════════════════════════════════════════════════════ */
static int ps2_read_byte(volatile int *ps2)
{
    int v = *ps2;
    if (v & PS2_RVALID) return v & 0xFF;
    return -1;
}

static void ps2_send_byte(volatile int *ps2, unsigned char b)
{
    int timeout = 2000000;
    while (*ps2 & PS2_RVALID) (void)(*ps2);
    *ps2 = (int)b;
    while (timeout-- > 0) {
        int v = *ps2;
        if ((v & PS2_RVALID) && (v & 0xFF) == 0xFA) break;
    }
}

void mouse_init(void)
{
    volatile int *ps2 = (volatile int *)PS2_BASE;
    int i;

    for (i = 0; i < 256; i++) (void)(*ps2);
    while (*ps2 & PS2_RVALID) (void)(*ps2);

    ps2_send_byte(ps2, 0xFF);

    {
        int got_aa = 0, timeout = 2000000;
        while (timeout-- > 0) {
            int v = *ps2;
            if (v & PS2_RVALID) {
                int b = v & 0xFF;
                if (b == 0xAA) got_aa = 1;
                if (got_aa && b == 0x00) break;
            }
        }
    }

    while (*ps2 & PS2_RVALID) (void)(*ps2);
    ps2_send_byte(ps2, 0xF4);
    while (*ps2 & PS2_RVALID) (void)(*ps2);
}

/* ═══════════════════════════════════════════════════════════════════════
   Main
   ═══════════════════════════════════════════════════════════════════════ */
int main(void)
{
    volatile int *pixel_ctrl = (volatile int *)PIXEL_BUF_CTRL;
    volatile int *ps2        = (volatile int *)PS2_BASE;

    pixel_buffer_start = *pixel_ctrl;
    *(pixel_ctrl + 1)  = pixel_buffer_start;

    mouse_init();
    build_and_draw_background();

    int cx = FB_WIDTH  / 2;
    int cy = FB_HEIGHT / 2;
    int cx_max = FB_WIDTH  - ARROW_W;
    int cy_max = FB_HEIGHT - ARROW_H;

    int ax = 0, ay = 0;

    int byte_idx   = 0;
    unsigned char pkt[3];
    int prev_left  = 0;
    /* ── NEW: track previous right button state for rising-edge detect ── */
    int prev_right = 0;

    draw_arrow(cx, cy);

    while (1)
    {
        int raw = ps2_read_byte(ps2);
        if (raw < 0) continue;

        unsigned char b = (unsigned char)raw;

        if (byte_idx == 0 && !(b & 0x08)) continue;

        pkt[byte_idx++] = b;
        if (byte_idx < 3) continue;
        byte_idx = 0;

        unsigned char flags = pkt[0];
        int dx = (int)pkt[1];
        int dy = (int)pkt[2];

        if (flags & 0x10) dx |= 0xFFFFFF00;
        if (flags & 0x20) dy |= 0xFFFFFF00;
        if (flags & 0xC0) {
            prev_left  = (flags & 0x01);
            prev_right = (flags & 0x02) ? 1 : 0;
            continue;
        }

        ax += dx;  ay += dy;
        int mx = ax / SPEED_DIV;
        int my = ay / SPEED_DIV;
        ax -= mx * SPEED_DIV;
        ay -= my * SPEED_DIV;

        erase_arrow(cx, cy);

        cx += mx;
        cy -= my;
        if (cx < 0)       cx = 0;
        if (cx > cx_max)  cx = cx_max;
        if (cy < 0)       cy = 0;
        if (cy > cy_max)  cy = cy_max;

        int left_now  = (flags & 0x01) ? 1 : 0;
        /* ── NEW: right button bit is bit 1 of flags byte ── */
        int right_now = (flags & 0x02) ? 1 : 0;

        /* ── CHANGED: left click → snap to staff and place note oval ── */
        if (left_now && !prev_left) {
            int staff_idx, pitch_slot;
            int snapped_y = snap_to_staff(cy, &staff_idx, &pitch_slot);
            if (snapped_y >= 0 && num_notes < MAX_NOTES) {
                /* Use cursor x as the note x position */
                notes[num_notes].x = cx;
                notes[num_notes].y = snapped_y;
                num_notes++;
                draw_note_head(cx, snapped_y);
            }
        }

        /* ── NEW: right click → remove nearest note within oval range ── */
        if (right_now && !prev_right) {
            int i, best = -1, best_dist2 = (NOTE_RX * 3) * (NOTE_RX * 3);
            for (i = 0; i < num_notes; i++) {
                int ddx = cx - notes[i].x;
                int ddy = cy - notes[i].y;
                int d2  = ddx*ddx + ddy*ddy;
                if (d2 < best_dist2) {
                    best_dist2 = d2;
                    best = i;
                }
            }
            if (best >= 0) {
                /* Erase the oval from screen */
                erase_note_head(notes[best].x, notes[best].y);
                /* Remove from array by swapping with last */
                notes[best] = notes[num_notes - 1];
                num_notes--;
                /* Redraw any remaining notes that may have been under cursor
                   (rare, but keeps display consistent) */
                int i2;
                for (i2 = 0; i2 < num_notes; i2++)
                    draw_note_head(notes[i2].x, notes[i2].y);
            }
        }

        prev_left  = left_now;
        prev_right = right_now;

        draw_arrow(cx, cy);

        pixel_buffer_start = *pixel_ctrl;
    }

    return 0;
}
/* =========================================
   End of vga_music_v2.c
   ========================================= */

