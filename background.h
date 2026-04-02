/**
 * @file background.h
 * @brief Public interface and layout constants for the VGA background layer.
 *
 * Defines all geometry constants that govern the staff grid (frame buffer
 * dimensions, staff positions, step widths), declares the shared bg[][]
 * software pixel buffer, and exposes the two functions used to initialise
 * and render the background.
 *
 * Include this header in any translation unit that needs to read or write
 * to the background buffer or reference grid geometry.
 *
 * @authors Tannaz Chowdhury, Dareen Nasreldin
 */

#ifndef BACKGROUND_H
#define BACKGROUND_H

/* ── Frame-buffer dimensions ── */

/** @brief Visible pixel width of the VGA frame buffer. */
#define FB_WIDTH    320

/** @brief Visible pixel height of the VGA frame buffer. */
#define FB_HEIGHT   240

/* ═══════════════════════════════════════════════════════════════════════
   Staff layout
   4 staves, each with 5 lines spaced 6 px apart (36 px tall per staff).
   All four staves fit within the 240 px display height with inter-staff
   gaps handled by the staff_top[] position array in background.c.
   ═══════════════════════════════════════════════════════════════════════ */

/** @brief Number of musical staves rendered on screen. */
#define NUM_STAVES      4

/** @brief Number of horizontal lines per staff. */
#define LINES_PER_STAFF 5

/** @brief Vertical pixel distance between adjacent staff lines. */
#define STAFF_SPACING   6

/** @brief X-coordinate of the left edge of every staff. */
#define STAFF_X0        10

/** @brief X-coordinate of the right edge of every staff (10 px from the
 *         right side of the frame buffer). */
#define STAFF_X1       (FB_WIDTH - 10)

/* ── Step/column grid ── */

/**
 * @brief Total number of column slots across the staff width.
 *
 * Column 0 is reserved for the treble clef glyph; note placement begins
 * at column 1.
 */
#define NUM_STEPS   17

/**
 * @brief Pixel width of one column slot.
 *
 * Derived from the usable staff width divided evenly across NUM_STEPS.
 */
#define STEP_W      ((STAFF_X1 - STAFF_X0) / NUM_STEPS)

/* ── Shared globals ── */

/**
 * @brief Y-coordinate of the top staff line for each staff, indexed 0–3.
 *
 * Defined in background.c. Read by vga_music_v2.c and sequencer_audio.c
 * to convert between grid rows and screen pixel positions.
 */
extern const int staff_top[NUM_STAVES];

/**
 * @brief Software copy of every visible pixel on screen.
 *
 * Maintained by background.c and kept in sync with the VGA hardware frame
 * buffer at all times. Used by vga_music_v2.c and sequencer_audio.c to
 * restore pixels underneath the cursor, erased notes, and the playhead
 * without reading back from VGA memory.
 */
extern short int bg[FB_HEIGHT][FB_WIDTH];

/* ── Public functions ── */

/**
 * @brief Builds the software pixel buffer and renders the full background.
 *
 * Clears the hardware frame buffer, initialises bg[][], draws all staves,
 * barlines, and treble clefs, then flushes the PS/2 FIFO. Must be called
 * once after pixel_buffer_start is set, and again on every page switch.
 */
void build_and_draw_background(void);

/**
 * @brief Draws a treble clef glyph at the specified screen position.
 *
 * Uses the packed bitmap in treble_clef_bitmap.h. Writes through bg_plot()
 * so both the software buffer and VGA hardware are updated.
 *
 * @param x0    Left edge of the glyph in screen coordinates.
 * @param y0    Top edge of the glyph in screen coordinates.
 * @param color RGB565 colour for the glyph pixels.
 */
void draw_treble_clef(int x0, int y0, short int color);

#endif /* BACKGROUND_H */
