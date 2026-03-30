#include "background.h"
#include "treble_clef_bitmap.h"

/* ═══════════════════════════════════════════════════════════════════════
   Colours (RGB565 format)
   ═══════════════════════════════════════════════════════════════════════ */
/* Converts standard 0-255 RGB values into FPGA-ready RGB 5-6-5 format */
#define RGB565(r, g, b)  ((short int)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)))

#define BLACK  ((short int)0x0000)
#define BG_PINK               RGB565(250, 217, 229) 
#define NOTE_GREEN            RGB565(1, 54, 13)
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
                bg_plot(x, y, NOTE_GREEN);
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
            bg_plot(STAFF_X0,     y, NOTE_GREEN);
            bg_plot(STAFF_X1 - 1, y, NOTE_GREEN);
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
void draw_treble_clef(int x0, int y0, short int color)
{
    int row, col;

    for (row = 0; row < CLEF_BMP_H; row++) {

        unsigned short bits = treble_clef_bmp[row];

        for (col = 0; col < CLEF_BMP_W; col++) {

            /* Check if this bit is set (pixel should be drawn) */
            if (bits & (1 << (CLEF_BMP_W - 1 - col)))
                bg_plot(x0 + col, y0 + row, color);
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
            row[x] = BG_PINK;
    }

    /* ── Step 2: initialise bg[][] to match screen (all white) ── */
    for (y = 0; y < FB_HEIGHT; y++)
        for (x = 0; x < FB_WIDTH; x++)
            bg[y][x] = BG_PINK;

    /* ── Step 3–4: draw staff structure ── */
    draw_staves();
    draw_barlines();

    /* ── Step 5: draw treble clefs ──
       Positioned slightly above the top staff line so that the
       spiral aligns with the correct pitch reference */
    for (s = 0; s < NUM_STAVES; s++)
        draw_treble_clef(STAFF_X0 + 1,
                         staff_top[s] - STAFF_SPACING,
                         NOTE_GREEN);

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
