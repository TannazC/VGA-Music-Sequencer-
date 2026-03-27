#define TOOLBAR_IMPL
#include "toolbar.h"
#include <stdlib.h>
#include "background.h"
#include "sequencer_audio.h"
#include "sprites.h"

int pixel_buffer_start;

/* ═══════════════════════════════════════════════════════════════════════
   Hardware addresses
   ═══════════════════════════════════════════════════════════════════════ */
#define PIXEL_BUF_CTRL  0xFF203020
#define PS2_BASE        0xFF200100
#define PS2_RVALID      0x8000

/* ═══════════════════════════════════════════════════════════════════════
   PS/2 Set-2 scan codes
   W/A/S/D  = navigate
   1-7      = select note type  (1=whole .. 7=64th)
   Space    = place note
   Backspace= delete note
   ═══════════════════════════════════════════════════════════════════════ */
#define KEY_W      0x1D
#define KEY_A      0x1C
#define KEY_S      0x1B
#define KEY_D      0x23

#define KEY_1      0x16   /* Whole note       (1 head)  */
#define KEY_2      0x1E   /* Half note        (1 head)  */
#define KEY_3      0x26   /* Quarter note     (1 head)  */
#define KEY_4      0x25   /* 2 beamed eighths (2 heads) */
#define KEY_5      0x2E   /* 4 beamed 16ths   (4 heads) */
#define KEY_6      0x36   /* 2 beamed 16ths   (2 heads) */
#define KEY_7      0x3D   /* Single 16th      (1 head)  */

#define KEY_SPACE  0x29
#define KEY_DELETE 0x66
#define KEY_Q      0x15 /* Q - start playback */
#define KEY_E      0x24 /* E - pause/resume playback */
#define KEY_T      0x2C /* T - stop playback, implement later */     
#define KEY_R      0x2D /* R - restart playback, implement later */
#define KEY_M  0x3A
#define KEY_BREAK  0xF0

//For the Tempo
#define KEY_MINUS  0x4E
#define KEY_EQUALS 0x55

/* ═══════════════════════════════════════════════════════════════════════
   Note types  (left-to-right matches the reference image)
   ─────────────────────────────────────────────────────────────────────
   Duration in 1/64-note units.
   Beamed groups store each sub-beat in head_step[] / head_pitch_slot[]
   so the playback engine can iterate each individual beat precisely.
   ═══════════════════════════════════════════════════════════════════════ */
#define NOTE_WHOLE      0   /* open oval, no stem            – 64/64  1 head  */
#define NOTE_HALF       1   /* open oval + stem              – 32/64  1 head  */
#define NOTE_QUARTER    2   /* filled oval + stem            – 16/64  1 head  */
#define NOTE_BEAM2_8TH  3   /* 2 beamed eighths (beam group) – 16/64  2 heads */
#define NOTE_BEAM4_16TH 4   /* 4 beamed 16ths   (beam group) – 16/64  4 heads */
#define NOTE_BEAM2_16TH 5   /* 2 beamed 16ths   (beam group) –  8/64  2 heads */
#define NOTE_SINGLE16TH 6   /* single 16th flag              –  4/64  1 head  */
#define NUM_NOTE_TYPES  7

/* Total duration of the whole glyph in 1/64-note units */
static const int note_duration_64[NUM_NOTE_TYPES] = {
    64, 32, 16,   /* whole, half, quarter           */
    16,           /* 2x eighth  = 16/64             */
    16,           /* 4x 16th    = 16/64             */
     8,           /* 2x 16th    =  8/64             */
     4            /* 1x 16th    =  4/64             */
};

/* How many individual note-heads each glyph contains */
static const int note_num_heads[NUM_NOTE_TYPES] = {
    1, 1, 1, 2, 4, 2, 1
};

/* ═══════════════════════════════════════════════════════════════════════
   Grid layout
   ═══════════════════════════════════════════════════════════════════════ */
/* SLOTS_PER_STAFF: 1 space above + 5 lines/4 spaces + 1 space below = 11   */
/* slot 0 = space above top line, slot 1 = top line, ..., slot 9 = bottom     */
/* line, slot 10 = space below bottom line.  row_to_y offsets by -1 slot.     */
#define SLOTS_PER_STAFF   ((LINES_PER_STAFF - 1) * 2 + 3)   /* 11 */
#define TOTAL_ROWS        (NUM_STAVES * SLOTS_PER_STAFF)     /* 44 */
#define TOTAL_COLS        NUM_STEPS                          /* 16 */

/* ═══════════════════════════════════════════════════════════════════════
   Visual constants
   ═══════════════════════════════════════════════════════════════════════ */
#define CURSOR_COLOR  ((short int)0x051F)
#define WHITE         ((short int)0xFFFF)
#define BLACK         ((short int)0x0000)


#define STEM_X_OFF    (OVAL_W)/2    /* stem at right edge of oval */
#define STEM_HEIGHT   11

/* Beamed notes: heads are spaced exactly STEP_W apart (one grid column each) */
/* So head[i] is at col+i, screen_x + i*STEP_W                               */

/* Beam bar thickness */
#define BEAM_THICK    2

/* Single flag: diagonal from stem tip */
#define FLAG_LEN      5

/* Cursor cell */
#define CELL_W        (STEP_W - 1)
#define CELL_H        (STAFF_SPACING / 2)

/* Max glyph bounding box for erase (4 heads wide + stem + flag) */
#define GLYPH_ERASE_W  (3 * STEP_W + OVAL_W/2 + FLAG_LEN + 2)
#define GLYPH_ERASE_H  (STEM_HEIGHT + OVAL_H/2 + 4)

#define MAX_NOTES      256
#define MAX_HEADS      4    /* maximum heads in one glyph */

/* ═══════════════════════════════════════════════════════════════════════
   Note struct
   ─────────────────────────────────────────────────────────────────────
   anchor: the leftmost/only head — used as the glyph's reference point.
   heads[]:  each individual beat position.
     • For single notes: num_heads=1, heads[0] = anchor.
     • For beamed groups: num_heads=N, heads[i] is at step+i, same
       pitch_slot (same horizontal row, consecutive columns).
   The playback engine reads heads[i].step / heads[i].pitch_slot for
   every sub-beat tick.
   ═══════════════════════════════════════════════════════════════════════ */
#ifndef NOTE_STRUCT_DEFINED
#define NOTE_STRUCT_DEFINED
   typedef struct {
    int step;         /* anchor column (first/only head)   */
    int staff;        /* 0 .. NUM_STAVES-1                 */
    int pitch_slot;   /* 0 (top) .. 8 (bottom) in staff    */
    int note_type;    /* NOTE_WHOLE .. NOTE_SINGLE16TH      */
    int duration_64;  /* total glyph duration in 1/64 units */

    /* Sub-beat positions (playback use) */
    int num_heads;
    int head_step[MAX_HEADS];        /* column index of each head           */
    int head_pitch_slot[MAX_HEADS];  /* pitch slot of each head (same row)  */

    /* Screen coordinates */
    int screen_x;    /* pixel x of anchor head centre */
    int screen_y;    /* pixel y of anchor head centre */
    int head_x[MAX_HEADS];   /* pixel x of each head centre */
    int head_y[MAX_HEADS];   /* pixel y of each head centre */
} Note;
#endif /* NOTE_STRUCT_DEFINED */   /* prevents redefinition in sequencer_audio.c */
Note notes[MAX_NOTES];
int  num_notes = 0;

int cur_note_type = NOTE_QUARTER;

/* ═══════════════════════════════════════════════════════════════════════
   Grid helpers
   ═══════════════════════════════════════════════════════════════════════ */
static int col_to_x(int col)
{
    if (col < 0)           col = 0;
    if (col >= TOTAL_COLS) col = TOTAL_COLS - 1;
    return STAFF_X0 + col * STEP_W + STEP_W / 2;
}

static int row_to_y(int row, int *staff_out, int *slot_out)
{
    int s, slot;
    if (row < 0)           row = 0;
    if (row >= TOTAL_ROWS) row = TOTAL_ROWS - 1;
    s    = row / SLOTS_PER_STAFF;
    slot = row % SLOTS_PER_STAFF;
    if (staff_out) *staff_out = s;
    if (slot_out)  *slot_out  = slot;
    /* Slot 0 = space above the top staff line (1 slot = STAFF_SPACING/2 px above). */
    return staff_top[s] + (slot - 1) * (STAFF_SPACING / 2);
}

/* ═══════════════════════════════════════════════════════════════════════
   Pixel helpers
   ═══════════════════════════════════════════════════════════════════════ */
void plot_pixel(int x, int y, short int c)
{
    if (x < 0 || x >= FB_WIDTH)  return;
    if (y < 0 || y >= FB_HEIGHT) return;
    *(volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = c;
}

/*
   restore_pixel: if pixel (x,y) falls inside ANY head of ANY placed note,
   paint it black; otherwise restore the background.
   We check every head of every note so beamed multi-head glyphs are
   protected correctly when the cursor moves over them.
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
                int bx = ddx + OPEN_OVAL_W/2;
                int by = ddy + OPEN_OVAL_H/2;
                if (bx >= 0 && bx < OPEN_OVAL_W && by >= 0 && by < OPEN_OVAL_H)
                    if (OPEN_OVAL[by][bx]) { plot_pixel(x, y, BLACK); return; }
            } else {
                int bx = ddx + OVAL_W/2;
                int by = ddy + OVAL_H/2;
                if (bx >= 0 && bx < OVAL_W && by >= 0 && by < OVAL_H)
                    if (FILLED_OVAL[by][bx]) { plot_pixel(x, y, BLACK); return; }
            }
        }
    }
    plot_pixel(x, y, bg[y][x]);
}

/* ═══════════════════════════════════════════════════════════════════════
   Cursor cell
   ═══════════════════════════════════════════════════════════════════════ */
static void draw_cursor_cell(int cx, int cy)
{
    int x, y;
    int x0 = cx - CELL_W/2, x1 = cx + CELL_W/2;
    int y0 = cy - CELL_H/2, y1 = cy + CELL_H/2;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            plot_pixel(x, y, CURSOR_COLOR);
}

static void erase_cursor_cell(int cx, int cy)
{
    int x, y;
    int x0 = cx - CELL_W/2, x1 = cx + CELL_W/2;
    int y0 = cy - CELL_H/2, y1 = cy + CELL_H/2;
    /* Restore every pixel in the cell directly from bg[][] – no oval test.
       Then redraw_all_notes() is called by the caller after moving so any
       stem / beam / flag that passed through here gets repainted.          */
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT)
                plot_pixel(x, y, bg[y][x]);
        }
}

/* ═══════════════════════════════════════════════════════════════════════
   Note glyph primitives
   ═══════════════════════════════════════════════════════════════════════ */

static void filled_oval(int ax, int ay, short int c)
{
    int dx, dy;
    for (dy = 0; dy < OVAL_H; dy++)
        for (dx = 0; dx < OVAL_W; dx++)
            if (FILLED_OVAL[dy][dx])
                plot_pixel(ax + dx - OVAL_W/2, ay + dy - OVAL_H/2, c);
}

static void open_oval(int ax, int ay, short int c)
{
    int dx, dy;
    for (dy = 0; dy < OPEN_OVAL_H; dy++)
        for (dx = 0; dx < OPEN_OVAL_W; dx++)
            if (OPEN_OVAL[dy][dx])
                plot_pixel(ax + dx - OPEN_OVAL_W/2, ay + dy - OPEN_OVAL_H/2, c);
}

/* Stem at right edge of oval, going straight up */
static void stem_up(int ax, int ay, short int c)
{
    int y;
    for (y = ay - STEM_HEIGHT; y <= ay; y++)
        plot_pixel(ax + STEM_X_OFF, y, c);
}

/*
   Single flag: diagonal stroke from stem tip curving right-downward.
   Two beams-wide stroke for visibility.
*/
//COmmented out since not used and flagged in cpulator
/*static void single_flag(int ax, int ay, short int c)
{
    int k;
    int sx = ax + STEM_X_OFF;
    int sy = ay - STEM_HEIGHT;
    for (k = 0; k < FLAG_LEN; k++) {
        plot_pixel(sx + k,     sy + k,     c);
        plot_pixel(sx + k + 1, sy + k,     c);
        plot_pixel(sx + k,     sy + k + 1, c);
    }
}
*/
/*
   Double flag: two stacked diagonals (for 16th single note).
*/
static void double_flag(int ax, int ay, short int c)
{
    int k;
    int sx = ax + STEM_X_OFF;
    /* First flag at stem tip */
    for (k = 0; k < FLAG_LEN; k++) {
        plot_pixel(sx + k,     ay - STEM_HEIGHT + k,     c);
        plot_pixel(sx + k + 1, ay - STEM_HEIGHT + k,     c);
        plot_pixel(sx + k,     ay - STEM_HEIGHT + k + 1, c);
    }
    /* Second flag 3px below first */
    for (k = 0; k < FLAG_LEN; k++) {
        plot_pixel(sx + k,     ay - STEM_HEIGHT + 3 + k,     c);
        plot_pixel(sx + k + 1, ay - STEM_HEIGHT + 3 + k,     c);
        plot_pixel(sx + k,     ay - STEM_HEIGHT + 3 + k + 1, c);
    }
}

/*
   Horizontal beam bar.
   x0 = left stem x,  x1 = right stem x,  y_top = top of beam,
   thick = bar thickness in pixels.
*/
static void beam_bar(int x0, int x1, int y_top, int thick, short int c)
{
    int x, t;
    for (t = 0; t < thick; t++)
        for (x = x0; x <= x1; x++)
            plot_pixel(x, y_top + t, c);
}

/* ═══════════════════════════════════════════════════════════════════════
   draw_note_glyph
   Draws the complete visual glyph for note type `nt` anchored at (cx,cy).
   For beamed types, additional heads are at cx + i*STEP_W (i=1,2,3).
   ═══════════════════════════════════════════════════════════════════════ */
static void draw_note_glyph(int cx, int cy, int nt, short int c)
{
    int i;
    /* x of each stem (right edge of each head) */
    int stem_top_y = cy - STEM_HEIGHT;

    switch (nt) {

    /* ── Whole: open oval, no stem ── */
    case NOTE_WHOLE:
        open_oval(cx, cy, c);
        break;

    /* ── Half: open oval + stem ── */
    case NOTE_HALF:
        open_oval(cx, cy, c);
        stem_up(cx, cy, c);
        break;

    /* ── Quarter: filled oval + stem ── */
    case NOTE_QUARTER:
        filled_oval(cx, cy, c);
        stem_up(cx, cy, c);
        break;

    /* ── 2 beamed eighths: 2 heads + 2 stems + 1 beam bar ── */
    case NOTE_BEAM2_8TH:
        for (i = 0; i < 2; i++) {
            filled_oval(cx + i * STEP_W, cy, c);
            stem_up(cx + i * STEP_W, cy, c);
        }
        /* beam across stem tops */
        beam_bar(cx + STEM_X_OFF, cx + STEP_W + STEM_X_OFF,
                 stem_top_y, BEAM_THICK, c);
        break;

    /* ── 4 beamed 16ths: 4 heads + 4 stems + 2 beam bars ── */
    case NOTE_BEAM4_16TH:
        for (i = 0; i < 4; i++) {
            filled_oval(cx + i * STEP_W, cy, c);
            stem_up(cx + i * STEP_W, cy, c);
        }
        /* two beam bars, second one 3px below first */
        beam_bar(cx + STEM_X_OFF, cx + 3 * STEP_W + STEM_X_OFF,
                 stem_top_y, BEAM_THICK, c);
        beam_bar(cx + STEM_X_OFF, cx + 3 * STEP_W + STEM_X_OFF,
                 stem_top_y + BEAM_THICK + 1, BEAM_THICK, c);
        break;

    /* ── 2 beamed 16ths: 2 heads + 2 stems + 2 beam bars ── */
    case NOTE_BEAM2_16TH:
        for (i = 0; i < 2; i++) {
            filled_oval(cx + i * STEP_W, cy, c);
            stem_up(cx + i * STEP_W, cy, c);
        }
        beam_bar(cx + STEM_X_OFF, cx + STEP_W + STEM_X_OFF,
                 stem_top_y, BEAM_THICK, c);
        beam_bar(cx + STEM_X_OFF, cx + STEP_W + STEM_X_OFF,
                 stem_top_y + BEAM_THICK + 1, BEAM_THICK, c);
        break;

    /* ── Single 16th: filled oval + stem + 2 flags ── */
    case NOTE_SINGLE16TH:
        filled_oval(cx, cy, c);
        stem_up(cx, cy, c);
        double_flag(cx, cy, c);
        break;

    default: break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   Erase: restore the full bounding box (covers all types conservatively)
   ═══════════════════════════════════════════════════════════════════════ */
static void erase_note_glyph(int cx, int cy)
{
    int x, y;
    int x0 = cx - OVAL_W/2;
    int x1 = cx + GLYPH_ERASE_W;
    int y0 = cy - GLYPH_ERASE_H;
    int y1 = cy + OVAL_H/2 + 2;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT)
                plot_pixel(x, y, bg[y][x]);
}

static void redraw_all_notes(void)
{
    int i;
    for (i = 0; i < num_notes; i++)
        draw_note_glyph(notes[i].screen_x, notes[i].screen_y,
                        notes[i].note_type, BLACK);
}

/* ═══════════════════════════════════════════════════════════════════════
   fill_note_heads
   Populate the head_step[], head_pitch_slot[], head_x[], head_y[] arrays
   for a note of type `nt` placed at (col, staff, slot, screen_x, screen_y).
   Beamed notes occupy consecutive columns at the same pitch row.
   ═══════════════════════════════════════════════════════════════════════ */
static void fill_note_heads(Note *n, int col, int staff, int slot,
                             int sx, int sy, int nt)
{
    int i;
    int nh = note_num_heads[nt];
    n->num_heads = nh;
    for (i = 0; i < nh; i++) {
        n->head_step[i]       = col + i;           /* consecutive columns   */
        n->head_pitch_slot[i] = slot;              /* same pitch row        */
        n->head_x[i]          = sx + i * STEP_W;  /* screen x of head i    */
        n->head_y[i]          = sy;                /* same y for all heads  */
    }
    /* Pad unused slots */
    for (i = nh; i < MAX_HEADS; i++) {
        n->head_step[i] = n->head_pitch_slot[i] = 0;
        n->head_x[i] = n->head_y[i] = 0;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   col_is_occupied
   Returns 1 if column `col` on (staff, slot) is already claimed by any
   head of any existing note.  Used to block overlapping placements.
   ═══════════════════════════════════════════════════════════════════════ */
static int col_is_occupied(int col, int staff)
{
    int i, h;
    for (i = 0; i < num_notes; i++) {
        if (notes[i].staff != staff) continue;
        for (h = 0; h < notes[i].num_heads; h++) {
            if (notes[i].head_step[h] == col) return 1;
        }
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
   place_note
   Blocks placement if:
     (a) the cursor column is already occupied by any head of any note, OR
     (b) any of the new note's heads would land on an occupied column.
   ═══════════════════════════════════════════════════════════════════════ */
static void place_note(int cur_col, int cur_staff, int cur_slot,
                       int cur_x, int cur_y, int nt)
{
    int h;
    int nh = note_num_heads[nt];

    /* All heads must fit within the grid columns 0..TOTAL_COLS-1 */
    if (cur_col < 2 || cur_col + nh - 1 >= TOTAL_COLS) return;  /* cols 0-1 = treble clef */

    /* Check every column the new glyph would occupy */
    for (h = 0; h < nh; h++) {
        if (col_is_occupied(cur_col + h, cur_staff)) return;
    }

    if (num_notes >= MAX_NOTES) return;

    notes[num_notes].step        = cur_col;
    notes[num_notes].staff       = cur_staff;
    notes[num_notes].pitch_slot  = cur_slot;
    notes[num_notes].note_type   = nt;
    notes[num_notes].duration_64 = note_duration_64[nt];
    notes[num_notes].screen_x    = cur_x;
    notes[num_notes].screen_y    = cur_y;
    fill_note_heads(&notes[num_notes], cur_col, cur_staff, cur_slot,
                    cur_x, cur_y, nt);
    num_notes++;

    draw_note_glyph(cur_x, cur_y, nt, BLACK);
    draw_cursor_cell(cur_x, cur_y);
}

/* ═══════════════════════════════════════════════════════════════════════
   delete_note
   Deletes whatever note owns the cursor column on this staff/slot.
   Checks all heads so beamed notes can be deleted from any of their
   occupied columns, not just the anchor.
   ═══════════════════════════════════════════════════════════════════════ */
static void delete_note(int cur_col, int cur_staff, int cur_slot,
                        int cur_x, int cur_y)
{
    int i, h;
    for (i = 0; i < num_notes; i++) {
        if (notes[i].staff != cur_staff || notes[i].pitch_slot != cur_slot)
            continue;
        for (h = 0; h < notes[i].num_heads; h++) {
            if (notes[i].head_step[h] == cur_col) {
                erase_note_glyph(notes[i].screen_x, notes[i].screen_y);
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
static int ps2_read_byte(volatile int *ps2)
{
    int v = *ps2;
    if (v & PS2_RVALID) return v & 0xFF;
    return -1;
}

static void ps2_flush(volatile int *ps2)
{
    int i;
    for (i = 0; i < 512; i++) {
        if (!(*ps2 & PS2_RVALID)) break;
        (void)(*ps2);
    }
}

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
/* Global playback control flags for sequencer_audio.c to read */
volatile int seq_is_playing = 0;
volatile int seq_is_paused = 0;

/* This allows play_sequence() to check for keys while it runs */
void poll_playback_keys(void)
{
    volatile int *ps2 = (volatile int *)PS2_BASE;
    static int got_break_local = 0; // Static to remember state across calls

    while (1) {
        int raw = ps2_read_byte(ps2);
        if (raw < 0) break; // Buffer is empty, go back to playing music

        unsigned char b = (unsigned char)raw;

        if (b == 0xE0) continue;
        if (b == KEY_BREAK) { got_break_local = 1; continue; }
        if (got_break_local) { got_break_local = 0; continue; } // Ignore key release

        /* Toggle pause state */
        if (b == KEY_E) {
            seq_is_paused = !seq_is_paused; 
            // Optional: update toolbar to show paused state if you have one
        }
        
        /* Signal the loop to break */
        if (b == KEY_T) {
            seq_is_playing = 0; 
        }
    }
}
/* Forward declaration for play_sequence defined in sequencer_audio.c */
void play_sequence(void);

/* ═══════════════════════════════════════════════════════════════════════
   Main
   ═══════════════════════════════════════════════════════════════════════ */
int main(void)
{
    volatile int *pixel_ctrl = (volatile int *)PIXEL_BUF_CTRL;
    volatile int *ps2        = (volatile int *)PS2_BASE;

    pixel_buffer_start = *pixel_ctrl;
    *(pixel_ctrl + 1)  = pixel_buffer_start;
    
    keyboard_init();
    build_and_draw_background();
    draw_toolbar(cur_note_type);

    int cur_col   = 2;   /* cols 0-1 overlap treble clef; start at 2 */
    int cur_row   = 0;
    int cur_staff = 0;
    int cur_slot  = 0;
    int cur_x     = col_to_x(cur_col);
    int cur_y     = row_to_y(cur_row, &cur_staff, &cur_slot);

    draw_cursor_cell(cur_x, cur_y);
    pixel_buffer_start = *pixel_ctrl;

    int got_break = 0;

    int menu_open = 0;
    draw_bottom_tab();

    while (1)
    {
        int raw = ps2_read_byte(ps2);
        if (raw < 0) continue;

        unsigned char b = (unsigned char)raw;

        if (b == 0xE0)        continue;
        if (b == KEY_BREAK) { got_break = 1; continue; }
        if (got_break)      { got_break = 0; continue; }

        /* ── M: Toggle Options Menu ── */
        if (b == KEY_M) {
            if (menu_open) {
                menu_open = 0;
                /* Wipe the menu away by redrawing the background & notes */
                build_and_draw_background();
                draw_toolbar(cur_note_type);
                draw_bottom_tab();
                redraw_all_notes();
                draw_cursor_cell(cur_x, cur_y);
            } else {
                menu_open = 1;
                draw_options_menu();
            }
            continue;
        }

        /* Block all other keyboard inputs if the menu is open */
        if (menu_open) continue;

        /* ── 1-7: select note type ── */
        if (b == KEY_1){cur_note_type = NOTE_WHOLE;
                        toolbar_set_note_type(cur_note_type); continue; }
        if (b == KEY_2){cur_note_type = NOTE_HALF;
                        toolbar_set_note_type(cur_note_type); continue; }
        if (b == KEY_3){cur_note_type = NOTE_QUARTER;
                        toolbar_set_note_type(cur_note_type); continue; }
        if (b == KEY_4) { cur_note_type = NOTE_BEAM2_8TH;
                        toolbar_set_note_type(cur_note_type); continue; }
        if (b == KEY_5) { cur_note_type = NOTE_BEAM4_16TH;
            toolbar_set_note_type(cur_note_type); continue; }
        if (b == KEY_6) { cur_note_type = NOTE_BEAM2_16TH; 
                        toolbar_set_note_type(cur_note_type); continue; }
        if (b == KEY_7) { cur_note_type = NOTE_SINGLE16TH;
                        toolbar_set_note_type(cur_note_type); continue;}

        /* Q: play sequence */
        if (b == KEY_Q) {
            seq_is_playing = 1;
            seq_is_paused = 0;
            toolbar_set_playback(TB_STATE_PLAYING);
            play_sequence(); // This will now check the globals internally
            toolbar_set_playback(TB_STATE_STOPPED);
            redraw_all_notes();
            draw_cursor_cell(cur_x, cur_y);
            continue;
        }

        /* E & T: Handled inside poll_playback_keys now */
        if (b == KEY_E) { continue; } // Only does something if already playing
        if (b == KEY_T) { continue; } // Only does something if already playing

        /* R: restart playback */
        if (b == KEY_R) {
            seq_is_playing = 1;
            seq_is_paused = 0;
            toolbar_set_playback(TB_STATE_PLAYING);
            play_sequence();
            toolbar_set_playback(TB_STATE_STOPPED);
            redraw_all_notes();
            draw_cursor_cell(cur_x, cur_y);
            continue;
        }

        /* ── Space: place ── */
        if (b == KEY_SPACE)
            place_note(cur_col, cur_staff, cur_slot, cur_x, cur_y, cur_note_type);

        /* ── Backspace: delete ── */
        if (b == KEY_DELETE)
            delete_note(cur_col, cur_staff, cur_slot, cur_x, cur_y);

        /* ── - / +: Adjust Tempo ── */
        if (b == KEY_MINUS) {
            toolbar_set_bpm(toolbar_state.bpm - 5);
            continue;
        }
        if (b == KEY_EQUALS) {
            toolbar_set_bpm(toolbar_state.bpm + 5);
            continue;
        }
        
        /* ── W/A/S/D: navigate ── */
        if (b == KEY_W || b == KEY_A || b == KEY_S || b == KEY_D) {
            int new_col = cur_col;
            int new_row = cur_row;

            if (b == KEY_W && cur_row > 0)              new_row--;
            if (b == KEY_S && cur_row < TOTAL_ROWS - 1) new_row++;
            if (b == KEY_A && cur_col > 2)              new_col--;  /* col 0-1 blocked */
            if (b == KEY_D && cur_col < TOTAL_COLS - 1) new_col++;

            if (new_col != cur_col || new_row != cur_row) {
                erase_cursor_cell(cur_x, cur_y);
                cur_col = new_col;
                cur_row = new_row;
                cur_x   = col_to_x(cur_col);
                cur_y   = row_to_y(cur_row, &cur_staff, &cur_slot);
                /* Repaint all notes so stems/beams under old cursor survive */
                redraw_all_notes();
                draw_cursor_cell(cur_x, cur_y);
            }
        }

        pixel_buffer_start = *pixel_ctrl;
    }

    return 0;
}
