/**
 * @file vga_music_v2.c
 * @brief Main entry point and sequencer logic for the VGA music sequencer.
 *
 * Owns the note array, cursor state, page management, and the top-level PS/2
 * keyboard event loop. Delegates audio playback to sequencer_audio.c and
 * all rendering to background.c, toolbar.c, and start_menu.c.
 *
 * Responsibilities:
 *  - Initialising the VGA frame buffer and PS/2 keyboard.
 *  - Running the start screen and song-select menu.
 *  - Processing every keyboard event: cursor movement, note placement and
 *    deletion, pitch editing, accidentals, tempo, page navigation, and
 *    transport controls.
 *  - Managing the notes[] array (placement, deletion, page filtering).
 *  - Coordinating page switches and full-screen redraws.
 *
 * @authors Tannaz Chowdhury, Dareen Nasreldin
 */

#define TOOLBAR_IMPL
#include "toolbar.h"
#include <stdlib.h>
#include "background.h"
#include "sequencer_audio.h"
#include "sprites.h"
#include "start_menu.h"

/** @brief Base address of the VGA pixel buffer. Set once in main(). */
int pixel_buffer_start;

/* ═══════════════════════════════════════════════════════════════════════
   Hardware addresses
   ═══════════════════════════════════════════════════════════════════════ */

/** @brief Memory-mapped address of the VGA pixel buffer controller. */
#define PIXEL_BUF_CTRL 0xFF203020

/** @brief Memory-mapped base address of the PS/2 peripheral. */
#define PS2_BASE       0xFF200100

/** @brief Bit mask for the PS/2 FIFO data-valid flag. */
#define PS2_RVALID     0x8000

/* ═══════════════════════════════════════════════════════════════════════
   PS/2 Set-2 scan codes
   ═══════════════════════════════════════════════════════════════════════ */
#define KEY_W 0x1D
#define KEY_A 0x1C
#define KEY_S 0x1B
#define KEY_D 0x23
#define KEY_Z 0x1A
#define KEY_X 0x22
#define KEY_C 0x21
#define KEY_V 0x2A

#define KEY_1 0x16
#define KEY_2 0x1E
#define KEY_3 0x26
#define KEY_4 0x25
#define KEY_5 0x2E
#define KEY_6 0x36
#define KEY_7 0x3D
#define KEY_8 0x3E

#define KEY_SPACE  0x29
#define KEY_DELETE 0x66
#define KEY_Q      0x15
#define KEY_E      0x24
#define KEY_T      0x2C
#define KEY_R      0x2D
#define KEY_M      0x3A
#define KEY_N      0x31

#define KEY_K 0x42  /**< Add page.    */
#define KEY_L 0x4B  /**< Remove page. */

/** @brief PS/2 break prefix: next byte is a key-release event. */
#define KEY_BREAK 0xF0

/* Arrow keys — always preceded by the 0xE0 extended prefix byte */
#define KEY_UP    0x75
#define KEY_DOWN  0x72
#define KEY_LEFT  0x6B
#define KEY_RIGHT 0x74

#define KEY_MINUS  0x4E  /**< Decrease BPM by 5. */
#define KEY_EQUALS 0x55  /**< Increase BPM by 5. */

/* ═══════════════════════════════════════════════════════════════════════
   Global state
   ═══════════════════════════════════════════════════════════════════════ */

/** @brief Currently displayed page (1-based). */
int cur_page = 1;

/** @brief Total number of pages in the composition (1–8). */
int max_pages = 4;

/**
 * @brief Non-zero while a toolbar or UI overlay is being drawn.
 *
 * Suppresses the toolbar-protection guard in plot_pixel() so that UI
 * elements can write into the top 46 rows during redraws.
 */
volatile int g_drawing_ui = 0;

/** @brief Row 2 key-held bitmask: bit 0 = PREV (←), bit 1 = NEXT (→). */
int active_page_nav = 0;

/** @brief Row 2 key-held bitmask: bit 0 = +PG (K), bit 1 = -PG (L). */
int active_page_struct = 0;

/* ── Audio transport flags (read by sequencer_audio.c) ── */

/** @brief Non-zero while the audio engine should continue playing. */
volatile int seq_is_playing = 0;

/** @brief Non-zero while playback is paused between columns. */
volatile int seq_is_paused = 0;

/** @brief Set by the audio engine when the user presses Stop (T). */
volatile int seq_user_stopped = 0;

/** @brief Set by the audio engine when the user presses Restart (R). */
volatile int seq_user_restarted = 0;

/**
 * @brief Page of the last note in the composition.
 *
 * The audio engine stops after finishing this page. -1 = play everything.
 */
int seq_last_note_page  = -1;

/** @brief Staff of the last note on seq_last_note_page. */
int seq_last_note_staff = -1;

/** @brief Column of the last note on seq_last_note_page/staff. */
int seq_last_note_col   = -1;

/**
 * @brief Minimum Y coordinate that plot_pixel() will write to during normal
 *        note/cursor drawing (excludes toolbar rows 0–45).
 */
#define UI_SAFE_ZONE 46

/* ═══════════════════════════════════════════════════════════════════════
   Note type constants and lookup tables
   ═══════════════════════════════════════════════════════════════════════ */
#define NOTE_WHOLE      0
#define NOTE_HALF       1
#define NOTE_QUARTER    2
#define NOTE_BEAM2_8TH  3
#define NOTE_BEAM4_16TH 4
#define NOTE_BEAM2_16TH 5
#define NOTE_SINGLE16TH 6
#define NOTE_REST       7
#define NUM_NOTE_TYPES  8

#define ACC_NONE    0
#define ACC_SHARP   1
#define ACC_FLAT    2
#define ACC_NATURAL 3

/**
 * @brief Duration of each note type in 64th-note units.
 *
 * Indexed by NOTE_WHOLE … NOTE_REST. Used to set Note.duration_64 at
 * placement time and to compute sample counts in sequencer_audio.c.
 */
static const int note_duration_64[NUM_NOTE_TYPES] = {
    64, 32, 16, 16, 16, 8, 4, 16
};

/**
 * @brief Number of note heads per note type.
 *
 * Multi-head types (beamed groups) occupy that many consecutive columns.
 */
static const int note_num_heads[NUM_NOTE_TYPES] = {
    1, 1, 1, 2, 4, 2, 1, 1
};

/* TOTAL_COLS must be 17 in this file (background.h defines 16 via NUM_STEPS) */
#ifdef TOTAL_COLS
#undef TOTAL_COLS
#endif
#define TOTAL_COLS 17

/** @brief Total pitch rows across all staves (NUM_STAVES × SLOTS_PER_STAFF). */
#define SLOTS_PER_STAFF ((LINES_PER_STAFF - 1) * 2 + 3)
#define TOTAL_ROWS      (NUM_STAVES * SLOTS_PER_STAFF)

#define CURSOR_COLOR ((short int)0x051F)  /**< Blue cursor cell colour.   */
#define WHITE        ((short int)0xFFFF)
#define BLACK        ((short int)0x0000)

/* Note glyph geometry */
#define STEM_X_OFF  (OVAL_W / 2) /**< X offset from head centre to stem. */
#define STEM_HEIGHT 11            /**< Stem length in pixels.             */
#define BEAM_THICK   2            /**< Beam bar height in pixels.         */
#define FLAG_LEN     5            /**< Length of each flag stroke.        */
#define CELL_W       (STEP_W - 1) /**< Cursor cell width.                 */
#define CELL_H       (STAFF_SPACING / 2) /**< Cursor cell height.         */

/** @brief Maximum number of notes that can be stored simultaneously. */
#define MAX_NOTES 512

/** @brief Maximum number of heads in a single Note struct. */
#define MAX_HEADS 4

#ifndef NOTE_STRUCT_DEFINED
#define NOTE_STRUCT_DEFINED
/**
 * @brief Represents one placed note or rest on the sequencer grid.
 *
 * Multi-head notes (beamed groups) store up to MAX_HEADS head positions.
 * All heads share the same note_type, duration_64, and accidental.
 */
typedef struct {
    int step;                       /**< Grid column of the first head (1-based).   */
    int staff;                      /**< Staff index (0 = top staff).               */
    int pitch_slot;                 /**< Pitch row of the first head (0–10).        */
    int note_type;                  /**< One of NOTE_WHOLE … NOTE_REST.             */
    int duration_64;                /**< Duration in 64th-note units.               */
    int accidental;                 /**< ACC_NONE, ACC_SHARP, ACC_FLAT, ACC_NATURAL.*/
    int num_heads;                  /**< Number of heads in this note (1–4).        */
    int head_step[MAX_HEADS];       /**< Column index for each head.                */
    int head_pitch_slot[MAX_HEADS]; /**< Pitch slot for each head.                  */
    int screen_x;                   /**< Pixel X of the first head.                 */
    int screen_y;                   /**< Pixel Y of the first head.                 */
    int head_x[MAX_HEADS];          /**< Pixel X for each head.                     */
    int head_y[MAX_HEADS];          /**< Pixel Y for each head.                     */
    int page;                       /**< Page number this note belongs to (1-based).*/
} Note;
#endif /* NOTE_STRUCT_DEFINED */

/** @brief Flat array of all placed notes across all pages. */
Note notes[MAX_NOTES];

/** @brief Current number of valid entries in notes[]. */
int num_notes = 0;

/** @brief Currently selected note type (NOTE_WHOLE … NOTE_REST). */
int cur_note_type = NOTE_QUARTER;

/** @brief Currently selected accidental (ACC_NONE … ACC_NATURAL). */
int cur_accidental = ACC_NONE;

/* ═══════════════════════════════════════════════════════════════════════
   Grid coordinate helpers
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Converts a grid column index to the centre X pixel of that column.
 *
 * @param col Grid column (0-based, clamped to [0, TOTAL_COLS-1]).
 * @return Centre X pixel coordinate.
 */
static int col_to_x(int col)
{
    if (col < 0) col = 0;
    if (col >= TOTAL_COLS) col = TOTAL_COLS - 1;
    return STAFF_X0 + col * STEP_W + STEP_W / 2;
}

/**
 * @brief Converts a linear grid row index to a screen Y coordinate and
 *        optionally returns the staff and slot indices.
 *
 * Row = staff * SLOTS_PER_STAFF + slot. Slot 0 is the top ledger line
 * position; slot 1 is the top staff line; and so on.
 *
 * @param row      Linear row index (0 = top of staff 0), clamped to valid range.
 * @param staff_out If non-NULL, receives the staff index (0–NUM_STAVES-1).
 * @param slot_out  If non-NULL, receives the slot within the staff (0–10).
 * @return Y pixel coordinate of that row.
 */
static int row_to_y(int row, int *staff_out, int *slot_out)
{
    int s, slot;
    if (row < 0) row = 0;
    if (row >= TOTAL_ROWS) row = TOTAL_ROWS - 1;
    s    = row / SLOTS_PER_STAFF;
    slot = row % SLOTS_PER_STAFF;
    if (staff_out) *staff_out = s;
    if (slot_out)  *slot_out  = slot;
    return staff_top[s] + (slot - 1) * (STAFF_SPACING / 2);
}

/* ═══════════════════════════════════════════════════════════════════════
   Pixel output and UI-safe wrappers
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Writes one pixel to the VGA hardware frame buffer.
 *
 * Enforces three guards:
 *  1. Out-of-bounds coordinates are silently dropped.
 *  2. Writes to y < UI_SAFE_ZONE are blocked unless g_start_screen_active
 *     or g_drawing_ui is set, preventing note/cursor drawing from
 *     overwriting the toolbar.
 *
 * @param x X coordinate (0 = left).
 * @param y Y coordinate (0 = top).
 * @param c RGB565 colour.
 */
void plot_pixel(int x, int y, short int c)
{
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;
    if (y < UI_SAFE_ZONE && !g_start_screen_active && !g_drawing_ui) return;
    *(volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = c;
}

/**
 * @brief Redraws the full row 1 toolbar with g_drawing_ui set so that
 *        plot_pixel() permits writes into the toolbar region.
 *
 * @param nt Currently selected note type index.
 */
void safe_draw_toolbar(int nt)
{
    g_drawing_ui = 1;
    draw_toolbar(nt);
    g_drawing_ui = 0;
}

/**
 * @brief Redraws the full row 2 toolbar with g_drawing_ui set.
 *
 * @param acc Currently selected accidental index.
 */
void safe_draw_row2(int acc)
{
    g_drawing_ui = 1;
    draw_toolbar_row2(acc, active_page_nav, active_page_struct);
    g_drawing_ui = 0;
}

/**
 * @brief Updates the note-type badges in row 1 with g_drawing_ui set.
 *
 * @param nt Newly selected note type index.
 */
void safe_set_note_type(int nt)
{
    g_drawing_ui = 1;
    toolbar_set_note_type(nt);
    g_drawing_ui = 0;
}

/**
 * @brief Restores the pixel at (x, y) to the correct colour, accounting for
 *        any note head that may be drawn there.
 *
 * Scans all note heads; if the pixel falls inside a note-head bitmap, draws
 * black. Otherwise restores bg[y][x].
 *
 * @param x X coordinate.
 * @param y Y coordinate.
 */
void restore_pixel(int x, int y)
{
    int i, h;
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;

    for (i = 0; i < num_notes; i++) {
        for (h = 0; h < notes[i].num_heads; h++) {
            int ddx = x - notes[i].head_x[h];
            int ddy = y - notes[i].head_y[h];
            int nt  = notes[i].note_type;
            if (nt == NOTE_WHOLE || nt == NOTE_HALF) {
                int bx = ddx + OPEN_OVAL_W / 2;
                int by = ddy + OPEN_OVAL_H / 2;
                if (bx >= 0 && bx < OPEN_OVAL_W && by >= 0 && by < OPEN_OVAL_H)
                    if (OPEN_OVAL[by][bx]) { plot_pixel(x, y, BLACK); return; }
            } else {
                int bx = ddx + OVAL_W / 2;
                int by = ddy + OVAL_H / 2;
                if (bx >= 0 && bx < OVAL_W && by >= 0 && by < OVAL_H)
                    if (FILLED_OVAL[by][bx]) { plot_pixel(x, y, BLACK); return; }
            }
        }
    }
    plot_pixel(x, y, bg[y][x]);
}

/**
 * @brief Draws the cursor cell (a filled rectangle) centred at (cx, cy).
 *
 * @param cx Centre X of the cursor.
 * @param cy Centre Y of the cursor.
 */
static void draw_cursor_cell(int cx, int cy)
{
    int x, y;
    int x0 = cx - CELL_W / 2, x1 = cx + CELL_W / 2;
    int y0 = cy - CELL_H / 2, y1 = cy + CELL_H / 2;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            plot_pixel(x, y, CURSOR_COLOR);
}

/**
 * @brief Erases the cursor cell by restoring background pixels at (cx, cy).
 *
 * @param cx Centre X of the cursor.
 * @param cy Centre Y of the cursor.
 */
static void erase_cursor_cell(int cx, int cy)
{
    int x, y;
    int x0 = cx - CELL_W / 2, x1 = cx + CELL_W / 2;
    int y0 = cy - CELL_H / 2, y1 = cy + CELL_H / 2;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT)
                plot_pixel(x, y, bg[y][x]);
}

/* ═══════════════════════════════════════════════════════════════════════
   Note glyph primitives
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Draws a filled oval (solid note head) centred at (ax, ay).
 *
 * @param ax Centre X.
 * @param ay Centre Y.
 * @param c  RGB565 colour.
 */
static void filled_oval(int ax, int ay, short int c)
{
    int dx, dy;
    for (dy = 0; dy < OVAL_H; dy++)
        for (dx = 0; dx < OVAL_W; dx++)
            if (FILLED_OVAL[dy][dx])
                plot_pixel(ax + dx - OVAL_W / 2, ay + dy - OVAL_H / 2, c);
}

/**
 * @brief Draws an open oval (hollow note head for whole/half notes) centred
 *        at (ax, ay).
 *
 * @param ax Centre X.
 * @param ay Centre Y.
 * @param c  RGB565 colour.
 */
static void open_oval(int ax, int ay, short int c)
{
    int dx, dy;
    for (dy = 0; dy < OPEN_OVAL_H; dy++)
        for (dx = 0; dx < OPEN_OVAL_W; dx++)
            if (OPEN_OVAL[dy][dx])
                plot_pixel(ax + dx - OPEN_OVAL_W / 2, ay + dy - OPEN_OVAL_H / 2, c);
}

/**
 * @brief Draws the two-flag decoration on a single 16th note.
 *
 * Both flags originate from the top of the stem (ax + STEM_X_OFF, ay -
 * STEM_HEIGHT) and sweep to the right.
 *
 * @param ax Head centre X.
 * @param ay Head centre Y.
 * @param c  RGB565 colour.
 */
static void double_flag(int ax, int ay, short int c)
{
    int k;
    int sx = ax + STEM_X_OFF;
    for (k = 0; k < FLAG_LEN; k++) {
        plot_pixel(sx + k,     ay - STEM_HEIGHT + k,     c);
        plot_pixel(sx + k + 1, ay - STEM_HEIGHT + k,     c);
        plot_pixel(sx + k,     ay - STEM_HEIGHT + k + 1, c);
    }
    for (k = 0; k < FLAG_LEN; k++) {
        plot_pixel(sx + k,     ay - STEM_HEIGHT + 3 + k,     c);
        plot_pixel(sx + k + 1, ay - STEM_HEIGHT + 3 + k,     c);
        plot_pixel(sx + k,     ay - STEM_HEIGHT + 3 + k + 1, c);
    }
}

/**
 * @brief Draws a horizontal beam bar spanning x0 to x1 at y_top.
 *
 * @param x0    Left X (inclusive).
 * @param x1    Right X (inclusive).
 * @param y_top Top Y of the bar.
 * @param thick Bar height in pixels.
 * @param c     RGB565 colour.
 */
static void beam_bar(int x0, int x1, int y_top, int thick, short int c)
{
    int x, t;
    for (t = 0; t < thick; t++)
        for (x = x0; x <= x1; x++)
            plot_pixel(x, y_top + t, c);
}

/**
 * @brief Draws the accidental symbol to the left of a note head.
 *
 * Does nothing for ACC_NONE. Draws a standard sharp (#), flat (b), or
 * natural (♮) glyph at the appropriate pixel offset.
 *
 * @param cx         Head centre X.
 * @param cy         Head centre Y.
 * @param accidental Accidental constant (ACC_SHARP, ACC_FLAT, ACC_NATURAL).
 * @param c          RGB565 colour.
 */
static void draw_accidental_symbol(int cx, int cy, int accidental, short int c)
{
    int x, y;
    int ax = cx - STEP_W / 2;

    if (accidental == ACC_NONE) return;

    if (accidental == ACC_SHARP) {
        for (y = cy - 6; y <= cy + 2; y++) {
            plot_pixel(ax - 1, y, c);
            plot_pixel(ax + 1, y, c);
        }
        for (x = ax - 4; x <= ax + 2; x++) {
            plot_pixel(x,     cy - 3, c);
            plot_pixel(x + 1, cy + 1, c);
        }
        return;
    }

    if (accidental == ACC_FLAT) {
        for (y = cy - 6; y <= cy + 3; y++)
            plot_pixel(ax - 1, y, c);
        plot_pixel(ax,     cy - 1, c);
        plot_pixel(ax + 1, cy,     c);
        plot_pixel(ax + 2, cy + 1, c);
        plot_pixel(ax + 2, cy + 2, c);
        plot_pixel(ax + 1, cy + 3, c);
        plot_pixel(ax,     cy + 4, c);
        return;
    }

    if (accidental == ACC_NATURAL) {
        for (y = cy - 6; y <= cy + 3; y++) {
            plot_pixel(ax - 1, y,     c);
            plot_pixel(ax + 1, y - 2, c);
        }
        for (x = ax - 1; x <= ax + 2; x++) {
            plot_pixel(x, cy - 2, c);
            plot_pixel(x, cy + 2, c);
        }
    }
}

/**
 * @brief Draws the stem of a note from the head down to STEM_HEIGHT above it.
 *
 * @param ax Head centre X.
 * @param ay Head centre Y.
 * @param c  RGB565 colour.
 */
static void draw_stem_segment(int ax, int ay, short int c)
{
    int y;
    for (y = ay - STEM_HEIGHT; y <= ay; y++)
        plot_pixel(ax + STEM_X_OFF, y, c);
}

/**
 * @brief Draws a stem from the head up to an explicit top y coordinate.
 *
 * Used for beamed groups where stems must reach a shared beam level.
 *
 * @param ax    Head centre X.
 * @param ay    Head centre Y.
 * @param y_top Top Y of the stem (beam level).
 * @param c     RGB565 colour.
 */
static void draw_stem_to_top(int ax, int ay, int y_top, short int c)
{
    int y0 = (y_top < ay) ? y_top : ay;
    int y1 = (y_top < ay) ? ay    : y_top;
    int y;
    for (y = y0; y <= y1; y++)
        plot_pixel(ax + STEM_X_OFF, y, c);
}

/**
 * @brief Draws a possibly-slanted beam segment between two stem tops.
 *
 * Interpolates Y linearly between (x0, y0) and (x1, y1). Draws a vertical
 * slab of `thick` pixels at each X position.
 *
 * @param x0    Start X.
 * @param y0    Start Y.
 * @param x1    End X.
 * @param y1    End Y.
 * @param thick Bar height in pixels.
 * @param c     RGB565 colour.
 */
static void beam_segment(int x0, int y0, int x1, int y1, int thick, short int c)
{
    int x, t;
    if (x1 < x0) { int tx = x0, ty = y0; x0 = x1; y0 = y1; x1 = tx; y1 = ty; }
    if (x1 == x0) { beam_bar(x0, x1, y0, thick, c); return; }
    for (x = x0; x <= x1; x++) {
        int y = y0 + (y1 - y0) * (x - x0) / (x1 - x0);
        for (t = 0; t < thick; t++)
            plot_pixel(x, y + t, c);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   Note rendering
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Draws the full visual representation of a note (heads, stems, beams,
 *        flags, accidentals, and rest glyphs).
 *
 * Dispatches on n->note_type. For beamed types (NOTE_BEAM2_8TH,
 * NOTE_BEAM4_16TH, NOTE_BEAM2_16TH), handles both the 2-head case with a
 * slanted beam and the general case with a flat beam at the highest stem top.
 *
 * @param n Pointer to the Note to draw. Must have num_heads > 0.
 * @param c RGB565 colour (use BLACK to draw, bg colour to erase).
 */
static void draw_note_instance(const Note *n, short int c)
{
    int i;
    if (n->num_heads <= 0) return;
    if (n->note_type != NOTE_REST)
        draw_accidental_symbol(n->head_x[0], n->head_y[0], n->accidental, c);

    switch (n->note_type) {
    case NOTE_WHOLE:
        open_oval(n->head_x[0], n->head_y[0], c);
        break;

    case NOTE_HALF:
        open_oval(n->head_x[0], n->head_y[0], c);
        draw_stem_segment(n->head_x[0], n->head_y[0], c);
        break;

    case NOTE_QUARTER:
        filled_oval(n->head_x[0], n->head_y[0], c);
        draw_stem_segment(n->head_x[0], n->head_y[0], c);
        break;

    case NOTE_BEAM2_8TH:
        if (n->num_heads == 2) {
            for (i = 0; i < n->num_heads; i++) {
                filled_oval(n->head_x[i], n->head_y[i], c);
                draw_stem_segment(n->head_x[i], n->head_y[i], c);
            }
            beam_segment(n->head_x[0] + STEM_X_OFF, n->head_y[0] - STEM_HEIGHT,
                         n->head_x[1] + STEM_X_OFF, n->head_y[1] - STEM_HEIGHT,
                         BEAM_THICK, c);
        } else {
            int beam_y = n->head_y[0] - STEM_HEIGHT;
            for (i = 1; i < n->num_heads; i++) {
                int top_i = n->head_y[i] - STEM_HEIGHT;
                if (top_i < beam_y) beam_y = top_i;
            }
            for (i = 0; i < n->num_heads; i++) {
                filled_oval(n->head_x[i], n->head_y[i], c);
                draw_stem_to_top(n->head_x[i], n->head_y[i], beam_y, c);
            }
            beam_bar(n->head_x[0] + STEM_X_OFF,
                     n->head_x[n->num_heads - 1] + STEM_X_OFF,
                     beam_y, BEAM_THICK, c);
        }
        break;

    case NOTE_BEAM4_16TH:
    case NOTE_BEAM2_16TH:
        if (n->num_heads == 2) {
            for (i = 0; i < n->num_heads; i++) {
                filled_oval(n->head_x[i], n->head_y[i], c);
                draw_stem_segment(n->head_x[i], n->head_y[i], c);
            }
            beam_segment(n->head_x[0] + STEM_X_OFF, n->head_y[0] - STEM_HEIGHT,
                         n->head_x[1] + STEM_X_OFF, n->head_y[1] - STEM_HEIGHT,
                         BEAM_THICK, c);
            beam_segment(n->head_x[0] + STEM_X_OFF, n->head_y[0] - STEM_HEIGHT + BEAM_THICK + 1,
                         n->head_x[1] + STEM_X_OFF, n->head_y[1] - STEM_HEIGHT + BEAM_THICK + 1,
                         BEAM_THICK, c);
        } else {
            int beam_y = n->head_y[0] - STEM_HEIGHT;
            for (i = 1; i < n->num_heads; i++) {
                int top_i = n->head_y[i] - STEM_HEIGHT;
                if (top_i < beam_y) beam_y = top_i;
            }
            for (i = 0; i < n->num_heads; i++) {
                filled_oval(n->head_x[i], n->head_y[i], c);
                draw_stem_to_top(n->head_x[i], n->head_y[i], beam_y + BEAM_THICK + 1, c);
            }
            beam_bar(n->head_x[0] + STEM_X_OFF,
                     n->head_x[n->num_heads - 1] + STEM_X_OFF,
                     beam_y, BEAM_THICK, c);
            beam_bar(n->head_x[0] + STEM_X_OFF,
                     n->head_x[n->num_heads - 1] + STEM_X_OFF,
                     beam_y + BEAM_THICK + 1, BEAM_THICK, c);
        }
        break;

    case NOTE_SINGLE16TH:
        filled_oval(n->head_x[0], n->head_y[0], c);
        draw_stem_segment(n->head_x[0], n->head_y[0], c);
        double_flag(n->head_x[0], n->head_y[0], c);
        break;

    case NOTE_REST: {
        /* Hand-coded quarter-rest glyph, pixel by pixel */
        int cx = n->head_x[0], cy = n->head_y[0];
        plot_pixel(cx-1, cy-9, c); plot_pixel(cx,   cy-8, c);
        plot_pixel(cx,   cy-7, c); plot_pixel(cx+1, cy-7, c);
        plot_pixel(cx,   cy-6, c); plot_pixel(cx+1, cy-6, c); plot_pixel(cx+2, cy-6, c);
        plot_pixel(cx,   cy-5, c); plot_pixel(cx+1, cy-5, c); plot_pixel(cx+2, cy-5, c);
        plot_pixel(cx-1, cy-4, c); plot_pixel(cx,   cy-4, c); plot_pixel(cx+1, cy-4, c); plot_pixel(cx+2, cy-4, c);
        plot_pixel(cx-2, cy-3, c); plot_pixel(cx-1, cy-3, c); plot_pixel(cx,   cy-3, c); plot_pixel(cx+1, cy-3, c);
        plot_pixel(cx-2, cy-2, c); plot_pixel(cx-1, cy-2, c); plot_pixel(cx,   cy-2, c); plot_pixel(cx+1, cy-2, c);
        plot_pixel(cx-2, cy-1, c); plot_pixel(cx-1, cy-1, c); plot_pixel(cx,   cy-1, c);
        plot_pixel(cx-2, cy,   c); plot_pixel(cx-1, cy,   c); plot_pixel(cx,   cy,   c);
        plot_pixel(cx-1, cy+1, c); plot_pixel(cx,   cy+1, c);
        plot_pixel(cx,   cy+2, c); plot_pixel(cx+1, cy+2, c);
        plot_pixel(cx-2, cy+3, c); plot_pixel(cx-1, cy+3, c); plot_pixel(cx,   cy+3, c); plot_pixel(cx+1, cy+3, c); plot_pixel(cx+2, cy+3, c);
        plot_pixel(cx-3, cy+4, c); plot_pixel(cx-2, cy+4, c); plot_pixel(cx-1, cy+4, c); plot_pixel(cx,   cy+4, c); plot_pixel(cx+1, cy+4, c); plot_pixel(cx+2, cy+4, c);
        plot_pixel(cx-3, cy+5, c); plot_pixel(cx-2, cy+5, c); plot_pixel(cx-1, cy+5, c);
        plot_pixel(cx-3, cy+6, c); plot_pixel(cx-2, cy+6, c); plot_pixel(cx-1, cy+6, c);
        plot_pixel(cx-2, cy+7, c); plot_pixel(cx-1, cy+7, c);
        plot_pixel(cx-1, cy+8, c);
        break;
    }
    default: break;
    }
}

/**
 * @brief Erases a note from the screen by overwriting its bounding box with
 *        background pixels.
 *
 * For rests: uses a fixed bounding box centred on head_x[0]/head_y[0].
 * For all other types: computes the min/max extent of all heads then adds
 * padding for stems, beams, flags, and accidentals before restoring bg[][].
 *
 * @param n Pointer to the Note to erase.
 */
static void erase_note_instance(const Note *n)
{
    int x, y, i, min_x, max_x, min_y, max_y;
    if (n->num_heads <= 0) return;

    if (n->note_type == NOTE_REST) {
        int cx = n->head_x[0], cy = n->head_y[0];
        min_x = cx - 6; max_x = cx + 5; min_y = cy - 11; max_y = cy + 11;
        for (y = min_y; y <= max_y; y++)
            for (x = min_x; x <= max_x; x++)
                if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT)
                    plot_pixel(x, y, bg[y][x]);
        return;
    }

    min_x = max_x = n->head_x[0];
    min_y = max_y = n->head_y[0];
    for (i = 1; i < n->num_heads; i++) {
        if (n->head_x[i] < min_x) min_x = n->head_x[i];
        if (n->head_x[i] > max_x) max_x = n->head_x[i];
        if (n->head_y[i] < min_y) min_y = n->head_y[i];
        if (n->head_y[i] > max_y) max_y = n->head_y[i];
    }
    min_x -= STEP_W;
    max_x += OVAL_W / 2 + STEM_X_OFF + FLAG_LEN + 4;
    min_y -= STEM_HEIGHT + 8;
    max_y += OVAL_H / 2 + 4;

    for (y = min_y; y <= max_y; y++)
        for (x = min_x; x <= max_x; x++)
            if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT)
                plot_pixel(x, y, bg[y][x]);
}

/**
 * @brief Redraws every note that belongs to the current page.
 *
 * Called after any operation that may have disturbed note pixels
 * (cursor movement, erase, page switch).
 */
static void redraw_all_notes(void)
{
    for (int i = 0; i < num_notes; i++)
        if (notes[i].page == cur_page)
            draw_note_instance(&notes[i], COLOR_BLACK);
}

/**
 * @brief Draws a small note glyph used in the current-note indicator at the
 *        bottom of the screen.
 *
 * Constructs a temporary Note with heads placed at (cx, cy) and calls
 * draw_note_instance().
 *
 * @param cx         Centre X of the first head.
 * @param cy         Centre Y of the first head.
 * @param nt         Note type to render.
 * @param accidental Accidental to show.
 * @param c          RGB565 colour.
 */
void draw_mini_note_glyph(int cx, int cy, int nt, int accidental, short int c)
{
    Note mini;
    int nh = note_num_heads[nt];
    mini.note_type   = nt;
    mini.accidental  = accidental;
    mini.num_heads   = nh;
    mini.head_x[0]   = cx;
    mini.head_y[0]   = cy;
    for (int i = 1; i < nh; i++) {
        mini.head_x[i] = cx + i * 9;
        mini.head_y[i] = cy;
    }
    draw_note_instance(&mini, c);
}

/**
 * @brief Redraws the bottom status bar showing the current note type,
 *        accidental, and page indicator.
 *
 * Clears the strip at y >= 215, then draws the "CURRENT:" label, the mini
 * note glyph, the page indicator, and the options tab.
 *
 * @param nt         Note type to display.
 * @param accidental Accidental to display.
 * @param cur_p      Current page number.
 * @param max_p      Total page count.
 */
void update_note_indicator(int nt, int accidental, int cur_p, int max_p)
{
    for (int y = 215; y < FB_HEIGHT; y++)
        for (int x = 50; x < (65 + FB_WIDTH); x++)
            plot_pixel(x, y, bg[y][x]);
    g_drawing_ui = 1;
    tb_draw_string(5, 222, "CURRENT:", BLACK);
    draw_mini_note_glyph(65, 230, nt, (nt == NOTE_REST) ? ACC_NONE : accidental, BLACK);
    draw_page_indicator(cur_p, max_p);
    draw_bottom_tab();
    g_drawing_ui = 0;
}

/* ═══════════════════════════════════════════════════════════════════════
   Note management helpers
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Switches to a new page: updates cur_page, rebuilds the background,
 *        redraws the toolbar and all notes for the new page, and repositions
 *        the cursor.
 *
 * @param new_page New page number. Clamped to [1, max_pages]; no-op if invalid.
 * @param cur_x    Current cursor X pixel (used to redraw cursor).
 * @param cur_y    Current cursor Y pixel (used to redraw cursor).
 */
static void switch_page(int new_page, int cur_x, int cur_y)
{
    if (new_page < 1 || new_page > max_pages) return;
    cur_page = new_page;
    build_and_draw_background();
    safe_draw_toolbar(cur_note_type);
    safe_draw_row2(cur_accidental);
    update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages);
    redraw_all_notes();
    draw_cursor_cell(cur_x, cur_y);
}

/**
 * @brief Clears all notes and redraws the sequencer from scratch.
 *
 * Sets num_notes to 0 then rebuilds the display.
 *
 * @param cur_note_type  Current note type (for toolbar redraw).
 * @param cur_accidental Current accidental (for row 2 redraw).
 * @param cur_x          Cursor X pixel.
 * @param cur_y          Cursor Y pixel.
 */
static void clear_all_notes_and_reload(int cur_note_type, int cur_accidental,
                                        int cur_x, int cur_y)
{
    num_notes = 0;
    build_and_draw_background();
    safe_draw_toolbar(cur_note_type);
    safe_draw_row2(cur_accidental);
    update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages);
    draw_cursor_cell(cur_x, cur_y);
}

/**
 * @brief Populates the head arrays of a Note for all nh heads, distributing
 *        them into consecutive columns starting at col.
 *
 * Zeroes out any unused head slots (index >= nh).
 *
 * @param n     Pointer to the Note to fill.
 * @param col   Starting grid column.
 * @param staff Staff index.
 * @param slot  Pitch slot (all heads share the same slot).
 * @param sx    Screen X of the first head.
 * @param sy    Screen Y of the first head.
 * @param nt    Note type (used to look up nh = note_num_heads[nt]).
 */
static void fill_note_heads(Note *n, int col, int staff, int slot,
                             int sx, int sy, int nt)
{
    int nh = note_num_heads[nt];
    n->num_heads = nh;
    for (int i = 0; i < nh; i++) {
        n->head_step[i]       = col + i;
        n->head_pitch_slot[i] = slot;
        n->head_x[i]          = sx + i * STEP_W;
        n->head_y[i]          = sy;
    }
    for (int i = nh; i < MAX_HEADS; i++) {
        n->head_step[i] = n->head_pitch_slot[i] = 0;
        n->head_x[i]    = n->head_y[i]          = 0;
    }
}

/**
 * @brief Searches for a note head at the given grid position.
 *
 * @param col      Column to search.
 * @param staff    Staff to restrict the search to.
 * @param slot     Pitch slot to match.
 * @param note_idx If non-NULL and found, receives the index into notes[].
 * @param head_idx If non-NULL and found, receives the head index within the note.
 * @return 1 if a matching head was found, 0 otherwise.
 */
static int find_note_head_at(int col, int staff, int slot,
                              int *note_idx, int *head_idx)
{
    for (int i = 0; i < num_notes; i++) {
        if (notes[i].staff != staff) continue;
        for (int h = 0; h < notes[i].num_heads; h++) {
            if (notes[i].head_step[h] == col &&
                notes[i].head_pitch_slot[h] == slot) {
                if (note_idx) *note_idx = i;
                if (head_idx) *head_idx = h;
                return 1;
            }
        }
    }
    return 0;
}

/**
 * @brief Moves a single note head one slot up or down (pitch edit).
 *
 * Erases the note, updates head_pitch_slot and head_y, then redraws all
 * notes. Does nothing if no head exists at the given position or the
 * new slot would be out of range.
 *
 * @param cur_col    Column of the head to move.
 * @param cur_staff  Staff of the head.
 * @param cur_slot   Current pitch slot.
 * @param delta_slot +1 = down (higher slot number), -1 = up (lower slot number).
 * @return 1 if the move succeeded, 0 if no note was found or boundary hit.
 */
static int move_note_head(int cur_col, int cur_staff, int cur_slot, int delta_slot)
{
    int note_idx, head_idx;
    if (!find_note_head_at(cur_col, cur_staff, cur_slot, &note_idx, &head_idx))
        return 0;
    Note *n = &notes[note_idx];
    int new_slot = n->head_pitch_slot[head_idx] + delta_slot;
    if (new_slot < 0 || new_slot >= SLOTS_PER_STAFF) return 0;

    erase_note_instance(n);
    n->head_pitch_slot[head_idx] = new_slot;
    int new_row = n->staff * SLOTS_PER_STAFF + new_slot;
    n->head_y[head_idx] = row_to_y(new_row, 0, 0);
    if (head_idx == 0) { n->pitch_slot = new_slot; n->screen_y = n->head_y[0]; }
    redraw_all_notes();
    return 1;
}

/**
 * @brief Returns 1 if any note head on the given staff and current page
 *        already occupies col.
 *
 * Used by place_note() to prevent overlapping notes.
 *
 * @param col   Grid column to check.
 * @param staff Staff to restrict the check to.
 * @return 1 if occupied, 0 if free.
 */
static int col_is_occupied(int col, int staff)
{
    for (int i = 0; i < num_notes; i++) {
        if (notes[i].page != cur_page || notes[i].staff != staff) continue;
        for (int h = 0; h < notes[i].num_heads; h++)
            if (notes[i].head_step[h] == col) return 1;
    }
    return 0;
}

/**
 * @brief Places a new note at the cursor position if the columns are free.
 *
 * Rejects placement if:
 *  - The note would extend past TOTAL_COLS (multi-head notes).
 *  - Any target column is already occupied on this staff/page.
 *  - notes[] is full (num_notes >= MAX_NOTES).
 *
 * On success, appends to notes[], draws the glyph, and redraws the cursor.
 *
 * @param cur_col   Starting grid column.
 * @param cur_staff Staff index.
 * @param cur_slot  Pitch slot.
 * @param cur_x     Screen X of the first head.
 * @param cur_y     Screen Y of the first head.
 * @param nt        Note type to place.
 */
static void place_note(int cur_col, int cur_staff, int cur_slot,
                        int cur_x, int cur_y, int nt)
{
    int nh = note_num_heads[nt];
    if (cur_col < 1 || cur_col + nh - 1 >= TOTAL_COLS) return;
    for (int h = 0; h < nh; h++)
        if (col_is_occupied(cur_col + h, cur_staff)) return;
    if (num_notes >= MAX_NOTES) return;

    Note *n = &notes[num_notes];
    n->step       = cur_col;
    n->staff      = cur_staff;
    n->pitch_slot = cur_slot;
    n->note_type  = nt;
    n->duration_64 = note_duration_64[nt];
    n->accidental  = (nt == NOTE_REST) ? ACC_NONE : cur_accidental;
    n->screen_x    = cur_x;
    n->screen_y    = cur_y;
    n->page        = cur_page;
    fill_note_heads(n, cur_col, cur_staff, cur_slot, cur_x, cur_y, nt);
    num_notes++;
    draw_note_instance(&notes[num_notes - 1], COLOR_BLACK);
    draw_cursor_cell(cur_x, cur_y);
}

/**
 * @brief Deletes the note whose head matches (cur_col, cur_staff, cur_slot).
 *
 * Erases the glyph, performs an O(1) swap-with-last deletion from notes[],
 * then redraws all remaining notes and the cursor.
 *
 * @param cur_col   Column to search.
 * @param cur_staff Staff to search.
 * @param cur_slot  Pitch slot to match.
 * @param cur_x     Cursor X for redraw.
 * @param cur_y     Cursor Y for redraw.
 */
static void delete_note(int cur_col, int cur_staff, int cur_slot,
                         int cur_x, int cur_y)
{
    for (int i = 0; i < num_notes; i++) {
        if (notes[i].page != cur_page || notes[i].staff != cur_staff) continue;
        for (int h = 0; h < notes[i].num_heads; h++) {
            if (notes[i].head_step[h] == cur_col &&
                notes[i].head_pitch_slot[h] == cur_slot) {
                erase_note_instance(&notes[i]);
                notes[i] = notes[num_notes - 1];
                num_notes--;
                redraw_all_notes();
                draw_cursor_cell(cur_x, cur_y);
                return;
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   PS/2 helpers
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Reads one byte from the PS/2 FIFO if data is available.
 *
 * @param ps2 Pointer to the PS/2 peripheral register.
 * @return The low byte of the PS/2 register if valid, -1 if the FIFO is empty.
 */
static int ps2_read_byte(volatile int *ps2)
{
    int v = *ps2;
    if (v & PS2_RVALID) return v & 0xFF;
    return -1;
}

/**
 * @brief Drains up to 512 bytes from the PS/2 FIFO.
 *
 * Used to discard stale input accumulated during long operations such as
 * background redraws.
 *
 * @param ps2 Pointer to the PS/2 peripheral register.
 */
static void ps2_flush(volatile int *ps2)
{
    for (int i = 0; i < 512; i++) {
        if (!(*ps2 & PS2_RVALID)) break;
        (void)(*ps2);
    }
}

/**
 * @brief Resets and enables the PS/2 keyboard.
 *
 * Sends a reset command (0xFF), waits for the BAT completion byte (0xAA),
 * then sends the enable-scanning command (0xF4) and waits for the ACK (0xFA).
 * Flushes the FIFO after each phase. Uses busy-wait loops with timeouts to
 * avoid hanging on unresponsive hardware.
 */
static void keyboard_init(void)
{
    volatile int *ps2 = (volatile int *)PS2_BASE;
    int timeout;
    ps2_flush(ps2);
    *ps2 = 0xFF;
    timeout = 3000000;
    while (timeout-- > 0) {
        int v = *ps2;
        if ((v & PS2_RVALID) && (v & 0xFF) == 0xAA) break;
    }
    ps2_flush(ps2);
    *ps2 = 0xF4;
    timeout = 2000000;
    while (timeout-- > 0) {
        int v = *ps2;
        if ((v & PS2_RVALID) && (v & 0xFF) == 0xFA) break;
    }
    ps2_flush(ps2);
}

/* ═══════════════════════════════════════════════════════════════════════
   Preloaded songs
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Injects a single note directly into notes[] without PS/2 interaction.
 *
 * Used by all preload functions to populate the note array programmatically.
 * Computes screen coordinates from col/staff/slot and fills all head arrays.
 *
 * @param col   Grid column (1-based).
 * @param staff Staff index (0–3).
 * @param slot  Pitch slot (0–10).
 * @param nt    Note type.
 * @param acc   Accidental constant.
 * @param page  Page number (1-based).
 */
static void inject_note(int col, int staff, int slot, int nt, int acc, int page)
{
    if (num_notes >= MAX_NOTES) return;
    Note *n = &notes[num_notes];
    n->step        = col;
    n->staff       = staff;
    n->pitch_slot  = slot;
    n->note_type   = nt;
    n->duration_64 = note_duration_64[nt];
    n->accidental  = acc;
    n->page        = page;
    int sx = col_to_x(col);
    int sy = row_to_y(staff * SLOTS_PER_STAFF + slot, NULL, NULL);
    n->screen_x = sx;
    n->screen_y = sy;
    fill_note_heads(n, col, staff, slot, sx, sy, nt);
    num_notes++;
}

/**
 * @brief Injects a two-head beamed note with two different pitch slots into
 *        consecutive columns col and col+1.
 *
 * Used by preload_seven_nation_army() to create beamed pairs where the two
 * notes have different pitches.
 *
 * @param col   Starting column.
 * @param staff Staff index.
 * @param slot1 Pitch slot of the first head.
 * @param slot2 Pitch slot of the second head.
 * @param acc   Accidental (applied to both heads).
 * @param page  Page number.
 * @param nt    Note type (should be a 2-head beamed type).
 */
static void inject_pair(int col, int staff, int slot1, int slot2,
                         int acc, int page, int nt)
{
    if (num_notes >= MAX_NOTES) return;
    Note *n = &notes[num_notes];
    n->step        = col;
    n->staff       = staff;
    n->pitch_slot  = slot1;
    n->note_type   = nt;
    n->duration_64 = note_duration_64[nt];
    n->accidental  = acc;
    n->page        = page;

    int sx = col_to_x(col);
    n->num_heads = 2;

    n->head_step[0]       = col;
    n->head_pitch_slot[0] = slot1;
    n->head_x[0]          = sx;
    n->head_y[0]          = row_to_y(staff * SLOTS_PER_STAFF + slot1, NULL, NULL);

    n->head_step[1]       = col + 1;
    n->head_pitch_slot[1] = slot2;
    n->head_x[1]          = sx + STEP_W;
    n->head_y[1]          = row_to_y(staff * SLOTS_PER_STAFF + slot2, NULL, NULL);

    n->screen_x = sx;
    n->screen_y = n->head_y[0];

    for (int i = 2; i < MAX_HEADS; i++) {
        n->head_step[i] = n->head_pitch_slot[i] = 0;
        n->head_x[i]    = n->head_y[i]          = 0;
    }
    num_notes++;
}

/**
 * @brief Loads Ode to Joy (Beethoven) across 2 pages as quarter notes.
 *
 * Pitch slots follow the melody; rests are inserted for gaps.
 * Sets max_pages = 2, BPM = 180.
 */
static void preload_ode_to_joy(void)
{
    num_notes = 0;
    int slots[128] = {
        2,2,1,0, 0,1,2,3, 4,4,3,2, 2,3,3,-1,
        2,2,1,0, 0,1,2,3, 4,4,3,2, 3,4,4,-1,
        3,3,2,4, 3,2,2,4, 3,2,3,4, 4,3,-1,-1,
        2,2,1,0, 0,1,2,3, 4,4,3,2, 3,4,4,-1,

        2,2,1,0, 0,1,2,3, 4,4,3,2, 2,3,3,-1,
        2,2,1,0, 0,1,2,3, 4,4,3,2, 3,4,4,-1,
        3,3,2,4, 3,2,2,4, 3,2,3,4, 4,3,-1,-1,
        2,2,1,0, 0,1,2,3, 4,4,3,2, 3,4,4,-1
    };
    for (int i = 0; i < 128; i++) {
        int is_rest   = (slots[i] == -1);
        int draw_slot = is_rest ? 4 : slots[i];
        int nt        = is_rest ? NOTE_REST : NOTE_QUARTER;
        inject_note((i % 16) + 1, (i % 64) / 16, draw_slot, nt, 0, (i / 64) + 1);
    }
    max_pages = 2;
    toolbar_state.bpm = 180;
}

/**
 * @brief Loads Für Elise (Beethoven) across 2 pages as quarter notes with
 *        accidentals.
 *
 * Sets max_pages = 2, BPM = 240.
 */
static void preload_fur_elise(void)
{
    num_notes = 0;
    int slots[128] = {
        2, 2, 2, 2,   2, 5, 3, 4,   6, -1, 10, 9,   6, 5, -1, 9,
        7, 5, 4, -1,  9, 2, 2, 2,   2, 2, 5, 3,   4, 6, -1, 10,
        9, 6, 5, -1,  9, 4, 5, 6,  -1, 5, 4, 3,   2, 7, 1, 2,
        3, 8, 2, 3,   4, 9, 3, 4,   5, 9, 2, 2,   2, 2, 2, 2,
        2, 5, 3, 4,   6, -1, 10, 9,   6, 5, -1, 9,   9, 4, 5, 6,
        -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
        -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
    };
    int accs[128] = {
        0, 1, 0, 1,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
        1, 0, 0, 0,   0, 0, 1, 0,   1, 0, 0, 0,   0, 0, 0, 0,
        0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
        0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 1,   0, 1, 0, 1,
        0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
        0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
        0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
        0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0
    };
    for (int i = 0; i < 128; i++) {
        if (slots[i] == -1) continue;
        inject_note((i % 16) + 1, (i % 64) / 16, slots[i],
                    NOTE_QUARTER, accs[i], (i / 64) + 1);
    }
    max_pages = 2;
    toolbar_state.bpm = 240;
}

/**
 * @brief Loads Seven Nation Army (The White Stripes) on 1 page using a mix
 *        of quarter notes, half notes, and beamed pairs.
 *
 * Sets max_pages = 1, BPM = 120.
 */
static void preload_seven_nation_army(void)
{
    num_notes = 0;

    inject_note( 1, 0, 2, NOTE_QUARTER,   ACC_NONE, 1);
    inject_pair( 3, 0, 2, 0,  ACC_NONE, 1, NOTE_BEAM2_8TH);
    inject_pair( 5, 0, 2, 3,  ACC_NONE, 1, NOTE_BEAM2_8TH);
    inject_note( 7, 0, 4, NOTE_HALF,      ACC_NONE, 1);
    inject_note( 8, 0, 5, NOTE_HALF,      ACC_NONE, 1);

    inject_note( 9, 0, 2, NOTE_HALF,      ACC_NONE, 1);
    inject_pair(10, 0, 2, 0,  ACC_NONE, 1, NOTE_BEAM2_8TH);
    inject_pair(12, 0, 2, 3,  ACC_NONE, 1, NOTE_BEAM2_8TH);
    inject_note(14, 0, 4, NOTE_HALF,      ACC_NONE, 1);
    inject_note(15, 0, 5, NOTE_HALF,      ACC_NONE, 1);

    inject_note(16, 0, 2, NOTE_QUARTER,   ACC_NONE, 1);
    inject_pair( 2, 1, 2, 0,  ACC_NONE, 1, NOTE_BEAM2_8TH);
    inject_pair( 4, 1, 2, 3,  ACC_NONE, 1, NOTE_BEAM2_8TH);
    inject_pair( 6, 1, 4, 3,  ACC_NONE, 1, NOTE_BEAM2_8TH);
    inject_note( 8, 1, 4, NOTE_QUARTER,   ACC_NONE, 1);
    inject_note( 9, 1, 5, NOTE_HALF,      ACC_NONE, 1);

    inject_note(10, 1, 2, NOTE_HALF,      ACC_NONE, 1);
    inject_pair(11, 1, 2, 0,  ACC_NONE, 1, NOTE_BEAM2_8TH);
    inject_pair(13, 1, 2, 3,  ACC_NONE, 1, NOTE_BEAM2_8TH);
    inject_note(15, 1, 4, NOTE_HALF,      ACC_NONE, 1);
    inject_note(16, 1, 5, NOTE_HALF,      ACC_NONE, 1);

    max_pages = 1;
    toolbar_state.bpm = 120;
}

/**
 * @brief Loads a demo composition ("Do Re Mi") that exercises every note
 *        type across 2 pages.
 *
 * Page 1: 16th-note groups, beamed pairs, single 16ths, rests with accidentals.
 * Page 2: beamed 8th pairs, quarters, halves, and whole notes.
 * Sets max_pages = 2, BPM = 90.
 */
static void preload_do_re_mi(void)
{
    num_notes = 0;
    int c, slot_cycle = 0;

    for (c = 1; c <= 13; c += 4)
        inject_note(c, 0, (slot_cycle++) % 11, NOTE_BEAM4_16TH, ACC_NONE, 1);
    for (c = 1; c <= 15; c += 2)
        inject_note(c, 1, (slot_cycle++) % 11, NOTE_BEAM2_16TH, ACC_NONE, 1);
    for (c = 1; c <= 16; c++)
        inject_note(c, 2, (slot_cycle++) % 11, NOTE_SINGLE16TH, ACC_NONE, 1);
    for (c = 1; c <= 16; c++) {
        if (c % 2 == 1) {
            inject_note(c, 3, 4, NOTE_REST, ACC_NONE, 1);
        } else {
            int acc = (c % 4 == 0) ? ACC_SHARP : ACC_FLAT;
            inject_note(c, 3, (slot_cycle++) % 11, NOTE_QUARTER, acc, 1);
        }
    }

    for (c = 1; c <= 15; c += 2)
        inject_note(c, 0, (slot_cycle++) % 11, NOTE_BEAM2_8TH, ACC_NONE, 2);
    for (c = 1; c <= 16; c++)
        inject_note(c, 1, (slot_cycle++) % 11, NOTE_QUARTER, ACC_NONE, 2);
    for (c = 1; c <= 16; c += 2)
        inject_note(c, 2, (slot_cycle++) % 11, NOTE_HALF, ACC_NONE, 2);
    for (c = 1; c <= 16; c += 4)
        inject_note(c, 3, (slot_cycle++) % 11, NOTE_WHOLE, ACC_NONE, 2);

    max_pages = 2;
    toolbar_state.bpm = 90;
}

/* ═══════════════════════════════════════════════════════════════════════
   Main
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Program entry point.
 *
 * Initialises hardware, runs the start screen, optionally loads a preloaded
 * song, then enters the main PS/2 keyboard event loop. The loop processes
 * all user input in numbered sections:
 *
 *  1. Options menu toggle (M)
 *  2. Options menu state machine (instrument selection, back to main menu)
 *  3. Note type selection (1–8)
 *  4. Audio transport (Q = play, R = restart; inner loop handles multi-page)
 *  5. Arrow key navigation and pitch editing (extended key prefix 0xE0)
 *  6. Page structure (K = add page, L = remove page)
 *  7. Accidentals (Z/X/C/V)
 *  8. Note placement (Space) and deletion (Delete)
 *  9. Tempo adjustment (- / =)
 * 10. Cursor movement (WASD)
 *
 * @return 0 (never reached on bare-metal).
 */
int main(void)
{
    volatile int *pixel_ctrl = (volatile int *)PIXEL_BUF_CTRL;
    volatile int *ps2        = (volatile int *)PS2_BASE;

    /* Point both buffer registers at the same address — single-buffered VGA */
    pixel_buffer_start = *pixel_ctrl;
    *(pixel_ctrl + 1)  = pixel_buffer_start;

    keyboard_init();

restart_main_menu:
    num_notes = 0; cur_page = 1; max_pages = 1;
    g_start_screen_active = 1;

    int got_break = 0, got_extended = 0, menu_open = 0;
    int menu_state = MENU_STATE_MAIN;
    active_page_nav = 0; active_page_struct = 0;

    draw_start_screen();

    /* ── Start screen input loop ── */
    int got_break_start = 0;
    while (g_start_screen_active) {
        int raw = ps2_read_byte(ps2); if (raw < 0) continue;
        unsigned char b = (unsigned char)raw;
        if (b == 0xE0) continue;
        if (b == KEY_BREAK) { got_break_start = 1; continue; }
        if (got_break_start) { got_break_start = 0; continue; }

        if (b == KEY_W) { g_start_selection = 1; update_start_selection(1); }
        if (b == KEY_S) { g_start_selection = 2; update_start_selection(2); }
        if (b == KEY_1) { g_start_selection = 1; g_start_screen_active = 0; }
        if (b == KEY_2) { g_start_selection = 2; g_start_screen_active = 0; }
        if (b == KEY_SPACE) g_start_screen_active = 0;
    }

    /* ── Song-select submenu (if Preload Song was chosen) ── */
    if (g_start_selection == 2) {
        draw_song_select_screen();
        g_start_screen_active = 1;
        got_break_start = 0;
        g_song_selection = 1;

        while (g_start_screen_active) {
            int raw = ps2_read_byte(ps2); if (raw < 0) continue;
            unsigned char b = (unsigned char)raw;
            if (b == 0xE0) continue;
            if (b == KEY_BREAK) { got_break_start = 1; continue; }
            if (got_break_start) { got_break_start = 0; continue; }

            if (b == KEY_W) { if (g_song_selection > 1) g_song_selection--; update_song_selection(g_song_selection); }
            if (b == KEY_S) { if (g_song_selection < 5) g_song_selection++; update_song_selection(g_song_selection); }
            if (b == KEY_1) { g_song_selection = 1; g_start_screen_active = 0; }
            if (b == KEY_2) { g_song_selection = 2; g_start_screen_active = 0; }
            if (b == KEY_3) { g_song_selection = 3; g_start_screen_active = 0; }
            if (b == KEY_4) { g_song_selection = 4; g_start_screen_active = 0; }
            if (b == KEY_5) { g_song_selection = 5; g_start_screen_active = 0; }
            if (b == KEY_SPACE) g_start_screen_active = 0;
        }

        if      (g_song_selection == 1) preload_do_re_mi();
        else if (g_song_selection == 2) preload_fur_elise();
        else if (g_song_selection == 3) preload_ode_to_joy();
        else if (g_song_selection == 4) preload_seven_nation_army();
        else if (g_song_selection == 5) goto restart_main_menu;
    }

    /* ── Initial sequencer screen ── */
    build_and_draw_background();
    safe_draw_toolbar(cur_note_type);
    safe_draw_row2(cur_accidental);
    update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages);

    int cur_col = 1, cur_row = 0, cur_staff = 0, cur_slot = 0;
    int cur_x = col_to_x(cur_col);
    int cur_y = row_to_y(cur_row, &cur_staff, &cur_slot);
    redraw_all_notes();
    draw_cursor_cell(cur_x, cur_y);

    /* ════════════════════════════════════════════════════════════════════
       Main keyboard event loop
       ════════════════════════════════════════════════════════════════════ */
    while (1) {
        int raw = ps2_read_byte(ps2); if (raw < 0) continue;
        unsigned char b = (unsigned char)raw;

        if (b == 0xE0) { got_extended = 1; continue; }
        if (b == KEY_BREAK) { got_break = 1; continue; }

        /* Handle key-release (break) events — clear held-key bitmasks */
        if (got_break) {
            if (got_extended) {
                if (b == KEY_LEFT)  active_page_nav &= ~(1 << 0);
                if (b == KEY_RIGHT) active_page_nav &= ~(1 << 1);
            }
            if (b == KEY_K) active_page_struct &= ~(1 << 0);
            if (b == KEY_L) active_page_struct &= ~(1 << 1);
            safe_draw_row2(cur_accidental);
            got_break = 0; got_extended = 0; continue;
        }

        /* 1. Options menu toggle */
        if (b == KEY_M) {
            if (menu_open) {
                menu_open  = 0;
                menu_state = MENU_STATE_MAIN;
                build_and_draw_background();
                safe_draw_toolbar(cur_note_type);
                safe_draw_row2(cur_accidental);
                update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages);
                redraw_all_notes();
                draw_cursor_cell(cur_x, cur_y);
            } else {
                menu_open  = 1;
                menu_state = MENU_STATE_MAIN;
                g_drawing_ui = 1; draw_options_menu(); g_drawing_ui = 0;
            }
            continue;
        }

        if (b == KEY_N) {
            clear_all_notes_and_reload(cur_note_type, cur_accidental, cur_x, cur_y);
            continue;
        }

        /* 2. Options menu state machine */
        if (menu_open) {
            if (menu_state == MENU_STATE_MAIN) {
                if (b == KEY_1) {
                    menu_state = MENU_STATE_INSTRUMENT;
                    g_drawing_ui = 1; draw_options_menu_instrument(); g_drawing_ui = 0;
                    continue;
                }
                if (b == KEY_2) {
                    menu_open  = 0;
                    menu_state = MENU_STATE_MAIN;
                    goto restart_main_menu;
                }
            } else if (menu_state == MENU_STATE_INSTRUMENT) {
                if (b == KEY_1) { toolbar_set_instrument(TB_INST_BEEP);      continue; }
                if (b == KEY_2) { toolbar_set_instrument(TB_INST_PIANO);     continue; }
                if (b == KEY_3) { toolbar_set_instrument(TB_INST_XYLOPHONE); continue; }
                if (b == KEY_5) {
                    menu_state = MENU_STATE_MAIN;
                    g_drawing_ui = 1; draw_options_menu(); g_drawing_ui = 0;
                    continue;
                }
            }
            continue;
        }

        /* 3. Note type selection (1–8) */
        {
            int is_note_key = (b == KEY_1 || b == KEY_2 || b == KEY_3 || b == KEY_4 ||
                               b == KEY_5 || b == KEY_6 || b == KEY_7 || b == KEY_8);
            if (is_note_key) {
                if (b == KEY_1) cur_note_type = NOTE_WHOLE;
                if (b == KEY_2) cur_note_type = NOTE_HALF;
                if (b == KEY_3) cur_note_type = NOTE_QUARTER;
                if (b == KEY_4) cur_note_type = NOTE_BEAM2_8TH;
                if (b == KEY_5) cur_note_type = NOTE_BEAM4_16TH;
                if (b == KEY_6) cur_note_type = NOTE_BEAM2_16TH;
                if (b == KEY_7) cur_note_type = NOTE_SINGLE16TH;
                if (b == KEY_8) { cur_note_type = NOTE_REST; cur_accidental = ACC_NONE; }
                safe_set_note_type(cur_note_type);
                update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages);
                continue;
            }
        }

        /* 4. Audio transport (Q = play from page 1, R = restart) */
        if (b == KEY_Q || b == KEY_R) {
            int start_p = 1;

            /* Find the last page/staff/col that contains any note */
            {
                int lp = -1, ls = -1, lc = -1;
                for (int i = 0; i < num_notes; i++) {
                    int np     = notes[i].page;
                    int ns     = notes[i].staff;
                    int nc_max = notes[i].head_step[notes[i].num_heads - 1];
                    if (np > lp || (np == lp && ns > ls) ||
                        (np == lp && ns == ls && nc_max > lc)) {
                        lp = np; ls = ns; lc = nc_max;
                    }
                }
                seq_last_note_page  = lp;
                seq_last_note_staff = ls;
                seq_last_note_col   = lc;
            }

            while (1) {
                if (cur_page != start_p) switch_page(start_p, cur_x, cur_y);
                toolbar_state.playback = TB_STATE_PLAYING;
                safe_draw_toolbar(cur_note_type);
                for (int p = start_p; p <= max_pages; p++) {
                    if (cur_page != p) switch_page(p, cur_x, cur_y);
                    seq_user_stopped = 0; seq_user_restarted = 0;
                    seq_is_playing = 1; seq_is_paused = 0;
                    play_sequence();
                    if (seq_user_restarted || seq_user_stopped) break;
                    if (seq_last_note_page >= 1 && p >= seq_last_note_page) break;
                }
                if (seq_user_restarted) { start_p = 1; continue; }
                break;
            }
            seq_is_playing = 0;
            toolbar_state.playback = TB_STATE_STOPPED;
            safe_draw_toolbar(cur_note_type);
            redraw_all_notes();
            draw_cursor_cell(cur_x, cur_y);
            continue;
        }

        /* 5. Arrow keys: page navigation and pitch editing */
        if (got_extended) {
            if (b == KEY_LEFT) {
                active_page_nav |= (1 << 0);
                switch_page(cur_page - 1, cur_x, cur_y);
                safe_draw_row2(cur_accidental);
            } else if (b == KEY_RIGHT) {
                active_page_nav |= (1 << 1);
                switch_page(cur_page + 1, cur_x, cur_y);
                safe_draw_row2(cur_accidental);
            } else if (b == KEY_UP || b == KEY_DOWN) {
                int d = (b == KEY_UP) ? -1 : 1;
                if (move_note_head(cur_col, cur_staff, cur_slot, d)) {
                    cur_row += d;
                    cur_y = row_to_y(cur_row, &cur_staff, &cur_slot);
                    draw_cursor_cell(cur_x, cur_y);
                }
            }
            got_extended = 0; continue;
        }

        /* 6. Page structure: K = add page, L = remove last page */
        if (b == KEY_K) {
            active_page_struct |= (1 << 0);
            if (max_pages < 8) max_pages++;
            update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages);
            safe_draw_row2(cur_accidental);
            continue;
        }
        if (b == KEY_L) {
            active_page_struct |= (1 << 1);
            if (max_pages > 1) {
                int i = 0;
                while (i < num_notes) {
                    if (notes[i].page == max_pages) { notes[i] = notes[num_notes - 1]; num_notes--; }
                    else i++;
                }
                max_pages--;
                if (cur_page > max_pages) switch_page(max_pages, cur_x, cur_y);
                else update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages);
            }
            safe_draw_row2(cur_accidental);
            continue;
        }

        /* 7. Accidentals: Z=none, X=sharp(toggle), C=flat(toggle), V=natural(toggle) */
        if (b == KEY_Z || b == KEY_X || b == KEY_C || b == KEY_V) {
            if (b == KEY_Z) cur_accidental = ACC_NONE;
            if (b == KEY_X) cur_accidental = (cur_accidental == ACC_SHARP)   ? ACC_NONE : ACC_SHARP;
            if (b == KEY_C) cur_accidental = (cur_accidental == ACC_FLAT)    ? ACC_NONE : ACC_FLAT;
            if (b == KEY_V) cur_accidental = (cur_accidental == ACC_NATURAL) ? ACC_NONE : ACC_NATURAL;
            safe_draw_row2(cur_accidental);
            update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages);
            continue;
        }

        /* 8. Note placement and deletion */
        if (b == KEY_SPACE) {
            if (cur_note_type == NOTE_REST) {
                int rs = SLOTS_PER_STAFF / 2;
                int rr = cur_staff * SLOTS_PER_STAFF + rs;
                int ry = row_to_y(rr, 0, 0);
                place_note(cur_col, cur_staff, rs, cur_x, ry, cur_note_type);
                redraw_all_notes();
                draw_cursor_cell(cur_x, ry);
            } else {
                place_note(cur_col, cur_staff, cur_slot, cur_x, cur_y, cur_note_type);
            }
            continue;
        }
        if (b == KEY_DELETE) {
            delete_note(cur_col, cur_staff, cur_slot, cur_x, cur_y);
            continue;
        }

        /* 9. Tempo */
        if (b == KEY_MINUS)  { toolbar_set_bpm(toolbar_state.bpm - 5); safe_draw_toolbar(cur_note_type); continue; }
        if (b == KEY_EQUALS) { toolbar_set_bpm(toolbar_state.bpm + 5); safe_draw_toolbar(cur_note_type); continue; }

        /* 10. Cursor movement (WASD) */
        if (b == KEY_W || b == KEY_A || b == KEY_S || b == KEY_D) {
            int nc = cur_col, nr = cur_row;
            if (b == KEY_W && cur_row > 0)              nr--;
            if (b == KEY_S && cur_row < TOTAL_ROWS - 1) nr++;
            if (b == KEY_A && cur_col > 1)              nc--;
            if (b == KEY_D && cur_col < TOTAL_COLS - 1) nc++;
            if (nc != cur_col || nr != cur_row) {
                erase_cursor_cell(cur_x, cur_y);
                cur_col = nc; cur_row = nr;
                cur_x = col_to_x(cur_col);
                cur_y = row_to_y(cur_row, &cur_staff, &cur_slot);
                redraw_all_notes();
                draw_cursor_cell(cur_x, cur_y);
            }
            continue;
        }

        pixel_buffer_start = *pixel_ctrl;
    }
    return 0;
}
