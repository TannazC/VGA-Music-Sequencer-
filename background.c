/**
 * @file background.c
 * @brief VGA background rendering for the music sequencer.
 *
 * Handles drawing and maintaining the static background layer of the
 * sequencer display, including the pink background fill, musical staves,
 * barlines, and treble clefs. Also maintains the bg[][] software pixel
 * buffer which serves as ground truth for pixel restoration when notes
 * or the cursor are erased.
 *
 * @authors Tannaz Chowdhury, Dareen Alhudhaif
 */

#include "background.h"
#include "treble_clef_bitmap.h"

/* ═══════════════════════════════════════════════════════════════════════
   Colours (RGB565 format)
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Converts 8-bit RGB components to the RGB565 format used by the
 *        DE1-SoC VGA controller.
 *
 * @param r Red channel (0–255), truncated to 5 bits.
 * @param g Green channel (0–255), truncated to 6 bits.
 * @param b Blue channel (0–255), truncated to 5 bits.
 */
#define RGB565(r, g, b)  ((short int)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)))

#define BLACK      ((short int)0x0000)
#define BG_PINK    RGB565(250, 217, 229)
#define NOTE_GREEN RGB565(180, 80, 110)

/* ═══════════════════════════════════════════════════════════════════════
   Background buffer

   Stores one colour per visible pixel (320 x 240).
   This acts as a "ground truth" copy of the screen so we can restore
   pixels correctly when the cursor or a note is erased.

   Important:
   We NEVER read back from VGA memory directly — we always use bg[][]
   ═══════════════════════════════════════════════════════════════════════ */

/** @brief Software copy of every visible pixel. Used to restore pixels
 *         under the cursor and erased notes without reading VGA memory. */
short int bg[FB_HEIGHT][FB_WIDTH];

/** @brief Base address of the VGA frame buffer. Provided by vga_music_v2.c. */
extern int pixel_buffer_start;

/** @brief Memory-mapped base address of the PS/2 peripheral. */
#define PS2_BASE   0xFF200100

/** @brief Bit mask for the PS/2 FIFO data-valid flag. */
#define PS2_RVALID 0x8000

/**
 * @brief Y-coordinate of the top staff line for each of the four staves.
 *
 * Indexed 0–3 from top to bottom of the display.
 */
const int staff_top[NUM_STAVES] = {65, 105, 145, 185};

/* ═══════════════════════════════════════════════════════════════════════
   Static helper functions
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Writes a single pixel to both the software background buffer and
 *        the VGA hardware frame buffer.
 *
 * Both destinations must always be updated together so that bg[][] remains
 * an accurate mirror of what is displayed. The VGA address calculation uses
 * a 1024-byte (512 pixel) row stride: address = base + (y << 10) + (x << 1).
 *
 * @warning Writing outside [0, FB_WIDTH) x [0, FB_HEIGHT) would corrupt
 *          adjacent memory regions including the PS/2 peripheral registers.
 *          Bounds are checked and out-of-range writes are silently dropped.
 *
 * @param x Horizontal pixel coordinate (0 = left edge).
 * @param y Vertical pixel coordinate (0 = top edge).
 * @param c Pixel colour in RGB565 format.
 */
static void bg_plot(int x, int y, short int c)
{
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;

    bg[y][x] = c;

    *(volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = c;
}

/**
 * @brief Draws the five horizontal lines of each musical staff.
 *
 * Iterates over all NUM_STAVES staves. For each staff, draws LINES_PER_STAFF
 * horizontal lines separated by STAFF_SPACING pixels, spanning the full
 * horizontal extent of the staff from STAFF_X0 to STAFF_X1.
 */
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

/**
 * @brief Draws the left and right vertical barlines bounding each staff.
 *
 * Each barline runs from the top staff line to the bottom staff line of
 * the same staff, at x = STAFF_X0 (left) and x = STAFF_X1 - 1 (right).
 */
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

/**
 * @brief Renders a treble clef glyph at a given screen position using a
 *        packed 16-bit-per-row bitmap.
 *
 * Each entry in treble_clef_bmp[] encodes one row of the glyph as a
 * 16-bit bitmask. Bit CLEF_BMP_W-1 (MSB side) corresponds to the
 * leftmost pixel. A set bit causes bg_plot() to draw the supplied colour
 * at that position; a clear bit leaves the pixel unchanged.
 *
 * @param x0    Left edge of the glyph in screen coordinates.
 * @param y0    Top edge of the glyph in screen coordinates.
 * @param color RGB565 colour used to draw the set pixels.
 */
void draw_treble_clef(int x0, int y0, short int color)
{
    int row, col;

    for (row = 0; row < CLEF_BMP_H; row++) {

        unsigned short bits = treble_clef_bmp[row];

        for (col = 0; col < CLEF_BMP_W; col++) {

            if (bits & (1 << (CLEF_BMP_W - 1 - col)))
                bg_plot(x0 + col, y0 + row, color);
        }
    }
}

/**
 * @brief Initialises the entire display and software pixel buffer from scratch.
 *
 * Must be called once at startup and again whenever a page switch requires a
 * full redraw. Performs the following steps in order:
 *
 *  1. Clears the full 512×256 hardware frame buffer to BG_PINK (this covers
 *     the off-screen region as well, preventing garbage from appearing if the
 *     display controller ever scans past the visible area).
 *  2. Initialises every cell of bg[][] to BG_PINK to match the hardware state.
 *  3. Draws all staff lines via draw_staves().
 *  4. Draws all barlines via draw_barlines().
 *  5. Draws a treble clef at the left edge of every staff.
 *  6. Drains the PS/2 input FIFO.
 *
 * @note The PS/2 flush in step 6 is essential. Drawing takes long enough that
 *       the keyboard or mouse continues queuing bytes during the render.
 *       Leaving stale bytes in the FIFO breaks packet alignment in the main
 *       input loop, causing missed or misinterpreted key events.
 */
void build_and_draw_background(void)
{
    int x, y, s;

    /* ── Step 1: clear full frame buffer (512 x 256, not just visible area) ── */
    for (y = 0; y < 256; y++) {

        volatile short int *row =
            (volatile short int *)(pixel_buffer_start + (y << 10));

        for (x = 0; x < 512; x++)
            row[x] = BG_PINK;
    }

    /* ── Step 2: initialise bg[][] to match hardware state ── */
    for (y = 0; y < FB_HEIGHT; y++)
        for (x = 0; x < FB_WIDTH; x++)
            bg[y][x] = BG_PINK;

    /* ── Steps 3–4: draw staff structure ── */
    draw_staves();
    draw_barlines();

    /* ── Step 5: draw treble clefs ──
       Offset one STAFF_SPACING above the top staff line so the clef spiral
       aligns with the correct pitch reference (G4 on the second line). */
    for (s = 0; s < NUM_STAVES; s++)
        draw_treble_clef(STAFF_X0 + 1,
                         staff_top[s] - STAFF_SPACING,
                         BLACK);

    /* ── Step 6: flush PS/2 FIFO ──
       Drain every pending byte so the main loop starts with a clean,
       aligned input stream. */
    {
        volatile int *ps2 = (volatile int *)PS2_BASE;

        while (*ps2 & PS2_RVALID)
            (void)(*ps2);
    }
}
