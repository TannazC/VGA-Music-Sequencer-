#define TOOLBAR_IMPL
#include "toolbar.h"
#include <stdlib.h>
#include "background.h"
#include "sequencer_audio.h"
#include "sprites.h"
#include "start_menu.h"

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
#define KEY_Z      0x1A   /* accidental off     */
#define KEY_X      0x22   /* sharp toggle       */
#define KEY_C      0x21   /* flat toggle        */
#define KEY_V      0x2A   /* natural toggle     */

#define KEY_1      0x16   /* Whole note       (1 head)  */
#define KEY_2      0x1E   /* Half note        (1 head)  */
#define KEY_3      0x26   /* Quarter note     (1 head)  */
#define KEY_4      0x25   /* 2 beamed eighths (2 heads) */
#define KEY_5      0x2E   /* 4 beamed 16ths   (4 heads) */
#define KEY_6      0x36   /* 2 beamed 16ths   (2 heads) */
#define KEY_7      0x3D   /* Single 16th      (1 head)  */
#define KEY_8      0x3E   /* Rest (quarter)   (1 head)  */

#define KEY_SPACE  0x29
#define KEY_DELETE 0x66
#define KEY_Q      0x15 /* Q - start playback */
#define KEY_E      0x24 /* E - pause/resume playback */
#define KEY_T      0x2C /* T - stop playback, implement later */     
#define KEY_R      0x2D /* R - restart playback, implement later */
#define KEY_M  0x3A
#define KEY_N  0x31
#define KEY_BREAK  0xF0
#define KEY_UP     0x75
#define KEY_DOWN   0x72

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
#define NOTE_REST       7   /* quarter rest (silence)        – 16/64  1 head  */
#define NUM_NOTE_TYPES  8

#define ACC_NONE     0
#define ACC_SHARP    1
#define ACC_FLAT     2
#define ACC_NATURAL  3

/* Total duration of the whole glyph in 1/64-note units */
static const int note_duration_64[NUM_NOTE_TYPES] = {
    64, 32, 16,   /* whole, half, quarter           */
    16,           /* 2x eighth  = 16/64             */
    16,           /* 4x 16th    = 16/64             */
     8,           /* 2x 16th    =  8/64             */
     4,           /* 1x 16th    =  4/64             */
    16            /* rest       = quarter duration  */
};

/* How many individual note-heads each glyph contains */
static const int note_num_heads[NUM_NOTE_TYPES] = {
    1, 1, 1, 2, 4, 2, 1, 1
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
    int accidental;   /* ACC_NONE / ACC_SHARP / ACC_FLAT / ACC_NATURAL */

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
int cur_accidental = ACC_NONE;

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

static void draw_accidental_symbol(int cx, int cy, int accidental, short int c)
{
    int x, y;

    /*
       Place the accidental midway between the current note head and the
       previous beat slot so it stays visually attached to this note without
       colliding with neighbouring heads.
    */
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
            plot_pixel(ax - 1, y, c);
            plot_pixel(ax + 1, y - 2, c);
        }
        for (x = ax - 1; x <= ax + 2; x++) {
            plot_pixel(x, cy - 2, c);
            plot_pixel(x, cy + 2, c);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   draw_note_glyph
   Draws the complete visual glyph for note type `nt` anchored at (cx,cy).
   For beamed types, additional heads are at cx + i*STEP_W (i=1,2,3).
   ═══════════════════════════════════════════════════════════════════════ */
static void draw_stem_segment(int ax, int ay, short int c)
{
    int y;
    for (y = ay - STEM_HEIGHT; y <= ay; y++)
        plot_pixel(ax + STEM_X_OFF, y, c);
}

/*
   Draw a vertical stem whose top is explicitly chosen.
   This is used for multi-head beamed groups where the beam should stay
   horizontal even if one head is edited lower, which means only that
   head's stem becomes longer.
*/
static void draw_stem_to_top(int ax, int ay, int y_top, short int c)
{
    int y0 = (y_top < ay) ? y_top : ay;
    int y1 = (y_top < ay) ? ay : y_top;
    int y;

    for (y = y0; y <= y1; y++)
        plot_pixel(ax + STEM_X_OFF, y, c);
}

static void beam_segment(int x0, int y0, int x1, int y1, int thick, short int c)
{
    int x, t;
    if (x1 < x0) {
        int tx = x0, ty = y0;
        x0 = x1; y0 = y1;
        x1 = tx; y1 = ty;
    }
    if (x1 == x0) {
        beam_bar(x0, x1, y0, thick, c);
        return;
    }
    for (x = x0; x <= x1; x++) {
        int y = y0 + (y1 - y0) * (x - x0) / (x1 - x0);
        for (t = 0; t < thick; t++)
            plot_pixel(x, y + t, c);
    }
}

#define UI_SAFE_ZONE 46
/* Constraints for angled beams */
#define MAX_BEAM_DELTA  6  /* Maximum vertical tilt for a single beam group */

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
                     beam_y,
                     BEAM_THICK,
                     c);
        }
        break;

    case NOTE_BEAM4_16TH:
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
                     beam_y,
                     BEAM_THICK,
                     c);
            beam_bar(n->head_x[0] + STEM_X_OFF,
                     n->head_x[n->num_heads - 1] + STEM_X_OFF,
                     beam_y + BEAM_THICK + 1,
                     BEAM_THICK,
                     c);
        }
        break;

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
                     beam_y,
                     BEAM_THICK,
                     c);
            beam_bar(n->head_x[0] + STEM_X_OFF,
                     n->head_x[n->num_heads - 1] + STEM_X_OFF,
                     beam_y + BEAM_THICK + 1,
                     BEAM_THICK,
                     c);
        }
        break;

    case NOTE_SINGLE16TH:
        filled_oval(n->head_x[0], n->head_y[0], c);
        draw_stem_segment(n->head_x[0], n->head_y[0], c);
        double_flag(n->head_x[0], n->head_y[0], c);
        break;

    case NOTE_REST:
    {
        /* Quarter-rest — scaled to match note symbol height (18px = STEM_HEIGHT + OVAL_H) */
        int cx = n->head_x[0];
        int cy = n->head_y[0];
        plot_pixel(cx+(-1), cy+(-9), c);
        plot_pixel(cx+(0), cy+(-8), c);
        plot_pixel(cx+(0), cy+(-7), c);
        plot_pixel(cx+(1), cy+(-7), c);
        plot_pixel(cx+(0), cy+(-6), c);
        plot_pixel(cx+(1), cy+(-6), c);
        plot_pixel(cx+(2), cy+(-6), c);
        plot_pixel(cx+(0), cy+(-5), c);
        plot_pixel(cx+(1), cy+(-5), c);
        plot_pixel(cx+(2), cy+(-5), c);
        plot_pixel(cx+(-1), cy+(-4), c);
        plot_pixel(cx+(0), cy+(-4), c);
        plot_pixel(cx+(1), cy+(-4), c);
        plot_pixel(cx+(2), cy+(-4), c);
        plot_pixel(cx+(-2), cy+(-3), c);
        plot_pixel(cx+(-1), cy+(-3), c);
        plot_pixel(cx+(0), cy+(-3), c);
        plot_pixel(cx+(1), cy+(-3), c);
        plot_pixel(cx+(-2), cy+(-2), c);
        plot_pixel(cx+(-1), cy+(-2), c);
        plot_pixel(cx+(0), cy+(-2), c);
        plot_pixel(cx+(1), cy+(-2), c);
        plot_pixel(cx+(-2), cy+(-1), c);
        plot_pixel(cx+(-1), cy+(-1), c);
        plot_pixel(cx+(0), cy+(-1), c);
        plot_pixel(cx+(-2), cy+(0), c);
        plot_pixel(cx+(-1), cy+(0), c);
        plot_pixel(cx+(0), cy+(0), c);
        plot_pixel(cx+(-1), cy+(1), c);
        plot_pixel(cx+(0), cy+(1), c);
        plot_pixel(cx+(0), cy+(2), c);
        plot_pixel(cx+(1), cy+(2), c);
        plot_pixel(cx+(-2), cy+(3), c);
        plot_pixel(cx+(-1), cy+(3), c);
        plot_pixel(cx+(0), cy+(3), c);
        plot_pixel(cx+(1), cy+(3), c);
        plot_pixel(cx+(2), cy+(3), c);
        plot_pixel(cx+(-3), cy+(4), c);
        plot_pixel(cx+(-2), cy+(4), c);
        plot_pixel(cx+(-1), cy+(4), c);
        plot_pixel(cx+(0), cy+(4), c);
        plot_pixel(cx+(1), cy+(4), c);
        plot_pixel(cx+(2), cy+(4), c);
        plot_pixel(cx+(-3), cy+(5), c);
        plot_pixel(cx+(-2), cy+(5), c);
        plot_pixel(cx+(-1), cy+(5), c);
        plot_pixel(cx+(-3), cy+(6), c);
        plot_pixel(cx+(-2), cy+(6), c);
        plot_pixel(cx+(-1), cy+(6), c);
        plot_pixel(cx+(-2), cy+(7), c);
        plot_pixel(cx+(-1), cy+(7), c);
        plot_pixel(cx+(-1), cy+(8), c);
        break;
    }

        default:
        break;
    }
}

/* Generic preview-only glyph used by the current-note indicator. */
static void draw_note_glyph(int cx, int cy, int nt, int accidental, short int c)
{
    Note preview;
    int i;

    preview.step = 0;
    preview.staff = 0;
    preview.pitch_slot = 0;
    preview.note_type = nt;
    preview.duration_64 = note_duration_64[nt];
    preview.accidental = (nt == NOTE_REST) ? ACC_NONE : accidental;
    preview.num_heads = note_num_heads[nt];
    preview.screen_x = cx;
    preview.screen_y = cy;

    for (i = 0; i < preview.num_heads; i++) {
        preview.head_step[i] = i;
        preview.head_pitch_slot[i] = 0;
        preview.head_x[i] = cx + i * STEP_W;
        preview.head_y[i] = cy;
    }
    for (i = preview.num_heads; i < MAX_HEADS; i++) {
        preview.head_step[i] = 0;
        preview.head_pitch_slot[i] = 0;
        preview.head_x[i] = 0;
        preview.head_y[i] = 0;
    }

    draw_note_instance(&preview, c);
}

/* ═══════════════════════════════════════════════════════════════════════
   Erase: restore the full bounding box (covers all types conservatively)
   ═══════════════════════════════════════════════════════════════════════ */
static void erase_note_instance(const Note *n)
{
    int x, y, i;
    int min_x, max_x, min_y, max_y;

    if (n->num_heads <= 0) return;

    /* Handle Rests with their dedicated bounding box */
    if (n->note_type == NOTE_REST) {
        int cx = n->head_x[0];
        int cy = n->head_y[0];
        min_x = cx - 6; max_x = cx + 5;
        min_y = cy - 11; max_y = cy + 11;
        for (y = min_y; y <= max_y; y++) {
            for (x = min_x; x <= max_x; x++) {
                if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT)
                    plot_pixel(x, y, bg[y][x]);
            }
        }
        return;
    }

    /* Start with the first head */
    min_x = n->head_x[0];
    max_x = n->head_x[0];
    min_y = n->head_y[0];
    max_y = n->head_y[0];

    /* Expand the box to include all heads */
    for (i = 1; i < n->num_heads; i++) {
        if (n->head_x[i] < min_x) min_x = n->head_x[i];
        if (n->head_x[i] > max_x) max_x = n->head_x[i];
        if (n->head_y[i] < min_y) min_y = n->head_y[i];
        if (n->head_y[i] > max_y) max_y = n->head_y[i];
    }

    /* Padding for Accidentals (Left) and Stems/Beams (Top) */
    /* We subtract MAX_BEAM_DELTA + MIN_STEM_HEIGHT + padding from min_y 
       to ensure we catch the highest possible angled beam. */
    int top_clearance = STEM_HEIGHT + MAX_BEAM_DELTA + 5;
    
    min_x = min_x - 12; /* Space for accidentals */
    max_x = max_x + 8;  /* Space for stems/flags */
    min_y = min_y - top_clearance;
    max_y = max_y + 6;  /* Bottom of the oval */

    for (y = min_y; y <= max_y; y++) {
        for (x = min_x; x <= max_x; x++) {
            if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT) {
                /* Optimization: Only clear pixels that are actually inside the staff area */
                if (y >= UI_SAFE_ZONE) {
                    plot_pixel(x, y, bg[y][x]);
                }
            }
        }
    }
}

static void redraw_all_notes(void)
{
    int i;
    for (i = 0; i < num_notes; i++)
        draw_note_instance(&notes[i], BLACK);
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


static int find_note_head_at(int col, int staff, int slot, int *note_idx, int *head_idx)
{
    int i, h;
    for (i = 0; i < num_notes; i++) {
        if (notes[i].staff != staff) continue;
        for (h = 0; h < notes[i].num_heads; h++) {
            if (notes[i].head_step[h] == col && notes[i].head_pitch_slot[h] == slot) {
                if (note_idx) *note_idx = i;
                if (head_idx) *head_idx = h;
                return 1;
            }
        }
    }
    return 0;
}

static int move_note_head(int cur_col, int cur_staff, int cur_slot, int delta_slot)
{
    int note_idx, head_idx;
    int new_slot, new_row;
    Note *n;

    if (!find_note_head_at(cur_col, cur_staff, cur_slot, &note_idx, &head_idx))
        return 0;

    n = &notes[note_idx];
    new_slot = n->head_pitch_slot[head_idx] + delta_slot;
    if (new_slot < 0 || new_slot >= SLOTS_PER_STAFF)
        return 0;

    erase_note_instance(n);

    n->head_pitch_slot[head_idx] = new_slot;
    new_row = n->staff * SLOTS_PER_STAFF + new_slot;
    n->head_y[head_idx] = row_to_y(new_row, 0, 0);

    if (head_idx == 0) {
        n->pitch_slot = new_slot;
        n->screen_y = n->head_y[0];
    }

    redraw_all_notes();
    return 1;
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
    notes[num_notes].accidental  = (nt == NOTE_REST) ? ACC_NONE : cur_accidental;
    notes[num_notes].screen_x    = cur_x;
    notes[num_notes].screen_y    = cur_y;
    fill_note_heads(&notes[num_notes], cur_col, cur_staff, cur_slot,
                    cur_x, cur_y, nt);
    num_notes++;

    draw_note_glyph(cur_x, cur_y, nt, (nt == NOTE_REST) ? ACC_NONE : cur_accidental, BLACK);
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
        if (notes[i].staff != cur_staff)
            continue;
        for (h = 0; h < notes[i].num_heads; h++) {
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

/* =======================================================================
   Dynamic Current Note Indicator
   ======================================================================= */
static const char *accidental_label(int accidental)
{
    if (accidental == ACC_SHARP)   return "ACC: #";
    if (accidental == ACC_FLAT)    return "ACC: b";
    if (accidental == ACC_NATURAL) return "ACC: natural";
    return "ACC: off";
}
void draw_mini_note_glyph(int cx, int cy, int nt, int accidental, short int c)
{
    Note mini;
    int i;
    int MINI_STEP = 12;   /* Increased from 6 to 9 to prevent the "bus" look */
    int MINI_STEM = 8;   /* Slightly longer stem for better proportions */

    mini.note_type = nt;
    mini.accidental = accidental;
    mini.num_heads = note_num_heads[nt];
    
    mini.head_x[0] = cx; /* Adjusted to align with the "CURRENT:" text */
    mini.head_y[0] = cy;

    for (i = 1; i < mini.num_heads; i++) {
        mini.head_x[i] = mini.head_x[0] + i * MINI_STEP;
        mini.head_y[i] = cy;
    }

    /* We use a horizontal beam for the preview to keep it clean */
    draw_note_instance(&mini, c); 
}

void update_note_indicator(int nt, int accidental, int cur_p, int max_p) {
    int x, y;
    /* 1. Clear the entire bottom strip before redrawing. */
    for (y = 210; y < FB_HEIGHT; y++) {
        for (x = 0; x < FB_WIDTH; x++) {
            plot_pixel(x, y, bg[y][x]);
        }
    }

    /* 2. Draw "CURRENT NOTE:" label */
    tb_draw_string(5, 225, "CURRENT NOTE:", COLOR_BLACK);
    
    /* 3. Draw note glyph (handled in main.c or specialized function) */
    draw_mini_note_glyph(90, 228, nt, (nt == NOTE_REST) ? 0 : accidental, COLOR_BLACK); // Smaller glyph for the indicator
    /* 4. Update the page indicator in the same strip */
    draw_page_indicator(cur_p, max_p);
    draw_bottom_tab(); // Redraw the bottom toolbar to ensure it stays visible
}
/* Forward declaration for play_sequence defined in sequencer_audio.c */
void play_sequence(void);

static void clear_all_notes_and_reload(int cur_note_type, int cur_accidental,
                                       int cur_x, int cur_y)
{
    int i, h;

    for (i = 0; i < MAX_NOTES; i++) {
        notes[i].step = 0;
        notes[i].staff = 0;
        notes[i].pitch_slot = 0;
        notes[i].note_type = NOTE_QUARTER;
        notes[i].duration_64 = 0;
        notes[i].accidental = ACC_NONE;
        notes[i].num_heads = 0;
        notes[i].screen_x = 0;
        notes[i].screen_y = 0;
        for (h = 0; h < MAX_HEADS; h++) {
            notes[i].head_step[h] = 0;
            notes[i].head_pitch_slot[h] = 0;
            notes[i].head_x[h] = 0;
            notes[i].head_y[h] = 0;
        }
    }

    num_notes = 0;

    build_and_draw_background();
    draw_toolbar(cur_note_type);
    draw_cursor_cell(cur_x, cur_y);
}

/* ═══════════════════════════════════════════════════════════════════════
   Main
   ═══════════════════════════════════════════════════════════════════════ */
int main(void) {
    volatile int *pixel_ctrl = (volatile int *)PIXEL_BUF_CTRL;
    volatile int *ps2         = (volatile int *)PS2_BASE;

    pixel_buffer_start = *pixel_ctrl;
    *(pixel_ctrl + 1)  = pixel_buffer_start;
    
    keyboard_init();

    draw_start_screen();
    while (g_start_screen_active) {
        int raw = ps2_read_byte(ps2);
        if (raw < 0) continue;
        unsigned char b = (unsigned char)raw;
        if (b == 0xE0) continue;
        static int got_break_start = 0;
        if (b == KEY_BREAK) { got_break_start = 1; continue; }
        if (got_break_start) { got_break_start = 0; continue; }

        if (b == KEY_W) { g_start_selection = 1; update_start_selection(1); }
        if (b == KEY_S) { g_start_selection = 2; update_start_selection(2); }
        if (b == KEY_SPACE || b == KEY_1 || b == KEY_2) g_start_screen_active = 0;
    }

    build_and_draw_background();
    draw_toolbar(cur_note_type);
    draw_toolbar_row2(cur_accidental);
    update_note_indicator(cur_note_type, cur_accidental, 1, 1);

    int cur_col = 2, cur_row = 0, cur_staff, cur_slot;
    int cur_x = col_to_x(cur_col);
    int cur_y = row_to_y(cur_row, &cur_staff, &cur_slot);
    draw_cursor_cell(cur_x, cur_y);

    int got_break = 0, got_extended = 0, menu_open = 0;

    while (1) {
        int raw = ps2_read_byte(ps2);
        if (raw < 0) continue;
        unsigned char b = (unsigned char)raw;

        if (b == 0xE0) { got_extended = 1; continue; }
        if (b == KEY_BREAK) { got_break = 1; continue; }
        if (got_break) { got_break = 0; got_extended = 0; continue; }

        /* ── Q/E/T/R: Playback Controls (Restored) ── */
        if (b == KEY_Q) {
            if (!seq_is_playing) {
                seq_is_playing = 1; seq_is_paused = 0;
                toolbar_state.playback = TB_STATE_PLAYING;
                draw_toolbar(cur_note_type);
                play_sequence(); /* This function blocks in sequencer_audio.c until seq_is_playing is 0 */
                toolbar_state.playback = TB_STATE_STOPPED;
                draw_toolbar(cur_note_type);
            }
            continue;
        }
        if (b == KEY_E) {
            seq_is_paused = !seq_is_paused;
            toolbar_state.playback = seq_is_paused ? TB_STATE_PAUSED : TB_STATE_PLAYING;
            draw_toolbar(cur_note_type);
            continue;
        }
        if (b == KEY_T) {
            seq_is_playing = 0; /* sequencer_audio.c checks this in its loop */
            continue;
        }

        /* ── Navigation, Selection & Editing ── */
        if (b == KEY_M) {
            if (menu_open) {
                menu_open = 0;
                for (int my = MENU_Y0; my <= MENU_Y1 + 4; my++)
                    for (int mx = MENU_X0; mx <= MENU_X1 + 4; mx++)
                        plot_pixel(mx, my, bg[my][mx]);
                redraw_all_notes(); draw_cursor_cell(cur_x, cur_y);
            } else {
                menu_open = 1; draw_options_menu();
            }
            continue;
        }

        /* ── N: clear all placed notes and reload the staves ── */
        if (b == KEY_N) {
            clear_all_notes_and_reload(cur_note_type, cur_accidental, cur_x, cur_y);
            menu_open = 0;
            
            draw_toolbar_row2(cur_accidental);
    
            /* These live at the bottom now */
            update_note_indicator(cur_note_type, cur_accidental, 1, 1);
            draw_page_indicator(1, 1);
          
            continue;
        }

        if (menu_open) {
            if (b == KEY_1) toolbar_set_instrument(TB_INST_BEEP);
            if (b == KEY_2) toolbar_set_instrument(TB_INST_PIANO);
            if (b == KEY_3) toolbar_set_instrument(TB_INST_PIANO_REVERB);
            continue;
        }

        /* Note selection 1-8: FIX: Use explicit check to avoid swallowing WASD codes */
        int is_note_key = (b == KEY_1 || b == KEY_2 || b == KEY_3 || b == KEY_4 || 
                           b == KEY_5 || b == KEY_6 || b == KEY_7 || b == KEY_8);
        
        if (is_note_key) {
            if (b == KEY_1) cur_note_type = NOTE_WHOLE;
            else if (b == KEY_2) cur_note_type = NOTE_HALF;
            else if (b == KEY_3) cur_note_type = NOTE_QUARTER;
            else if (b == KEY_4) cur_note_type = NOTE_BEAM2_8TH;
            else if (b == KEY_5) cur_note_type = NOTE_BEAM4_16TH;
            else if (b == KEY_6) cur_note_type = NOTE_BEAM2_16TH;
            else if (b == KEY_7) cur_note_type = NOTE_SINGLE16TH;
            else if (b == KEY_8) { cur_note_type = NOTE_REST; cur_accidental = 0; draw_toolbar_row2(0); }
            
            toolbar_set_note_type(cur_note_type);
            update_note_indicator(cur_note_type, cur_accidental, 1, 1);
            continue;
        }

        /* Accidental selection Z/X/C/V */
        if (b == KEY_Z || b == KEY_X || b == KEY_C || b == KEY_V) {
            if (b == KEY_Z) cur_accidental = 0;
            else if (b == KEY_X) cur_accidental = 1;
            else if (b == KEY_C) cur_accidental = 2;
            else if (b == KEY_V) cur_accidental = 3;
            draw_toolbar_row2(cur_accidental);
            update_note_indicator(cur_note_type, cur_accidental, 1, 1);
            continue;
        }

        /* Arrow Keys: Pitch Adjustment */
        if (got_extended && (b == KEY_UP || b == KEY_DOWN)) {
            int delta = (b == KEY_UP) ? -1 : 1;
            if (move_note_head(cur_col, cur_staff, cur_slot, delta)) {
                cur_row += delta;
                cur_y = row_to_y(cur_row, &cur_staff, &cur_slot);
                draw_cursor_cell(cur_x, cur_y);
            }
            got_extended = 0; continue;
        }

        /* Navigation: W/A/S/D */
        if (b == KEY_W || b == KEY_A || b == KEY_S || b == KEY_D) {
            erase_cursor_cell(cur_x, cur_y);
            if (b == KEY_W && cur_row > 0) cur_row--;
            if (b == KEY_S && cur_row < TOTAL_ROWS - 1) cur_row++;
            if (b == KEY_A && cur_col > 2) cur_col--;
            if (b == KEY_D && cur_col < TOTAL_COLS - 1) cur_col++;
            cur_x = col_to_x(cur_col);
            cur_y = row_to_y(cur_row, &cur_staff, &cur_slot);
            redraw_all_notes(); draw_cursor_cell(cur_x, cur_y);
        }

        if (b == KEY_SPACE) place_note(cur_col, cur_staff, cur_slot, cur_x, cur_y, cur_note_type);
        if (b == KEY_DELETE) delete_note(cur_col, cur_staff, cur_slot, cur_x, cur_y);
        if (b == KEY_MINUS) toolbar_set_bpm(toolbar_state.bpm - 5);
        if (b == KEY_EQUALS) toolbar_set_bpm(toolbar_state.bpm + 5);

        pixel_buffer_start = *pixel_ctrl;
    }
    return 0;
}