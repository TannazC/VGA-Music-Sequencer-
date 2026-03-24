#include <stdlib.h>
#include "background.h"

/* ═══════════════════════════════════════════════════════════════════════
   Hardware addresses
   ═══════════════════════════════════════════════════════════════════════ */
#define PIXEL_BUF_CTRL  0xFF203020
#define PS2_BASE        0xFF200100
#define PS2_RVALID      0x8000

/* ═══════════════════════════════════════════════════════════════════════
   PS/2 keyboard scan codes (Set 2 make codes)
   Keyboard connected via PS/2 port on DE-series board.
   ═══════════════════════════════════════════════════════════════════════ */
#define KEY_W       0x1D   /* W     - move cursor up    */
#define KEY_A       0x1C   /* A     - move cursor left  */
#define KEY_S       0x1B   /* S     - move cursor down  */
#define KEY_D       0x23   /* D     - move cursor right */
#define KEY_SPACE   0x29   /* Space - place note        */
#define KEY_DELETE  0x66   /* Backspace - delete note   */
#define KEY_BREAK   0xF0   /* Break prefix (key-up)     */

/* ═══════════════════════════════════════════════════════════════════════
   Grid layout
   ─────────────────────────────────────────────────────────────────────
   Columns: NUM_STEPS  (16) – one per sequencer step
   Rows:    NUM_STAVES * SLOTS_PER_STAFF
            SLOTS_PER_STAFF = (LINES_PER_STAFF-1)*2 + 1 = 9
            (5 lines + 4 spaces = 9 pitch slots per staff)
   Total rows = 4 staves × 9 slots = 36 pitch rows
   ═══════════════════════════════════════════════════════════════════════ */
#define SLOTS_PER_STAFF   ((LINES_PER_STAFF - 1) * 2 + 1)   /* 9  */
#define TOTAL_ROWS        (NUM_STAVES * SLOTS_PER_STAFF)     /* 36 */
#define TOTAL_COLS        NUM_STEPS                          /* 16 */

/* ═══════════════════════════════════════════════════════════════════════
   Visual constants
   ═══════════════════════════════════════════════════════════════════════ */
#define CURSOR_COLOR  ((short int)0x051F)   /* bright blue highlight     */
#define WHITE         ((short int)0xFFFF)
#define BLACK         ((short int)0x0000)

#define NOTE_RX  4   /* note oval horizontal half-radius */
#define NOTE_RY  3   /* note oval vertical   half-radius */

#define CELL_W   (STEP_W - 1)         /* highlight cell width            */
#define CELL_H   (STAFF_SPACING / 2)  /* highlight cell height = 1 slot  */

#define MAX_NOTES  256

/* ═══════════════════════════════════════════════════════════════════════
   Globals
   ═══════════════════════════════════════════════════════════════════════ */
int pixel_buffer_start;
extern short int bg[FB_HEIGHT][FB_WIDTH];

/* ── Note struct ──
   Stores both the logical grid position (for sequencer playback)
   and the screen pixel position (for fast draw/erase).           */
typedef struct {
    int step;        /* column  0 .. TOTAL_COLS-1             */
    int staff;       /* staff   0 .. NUM_STAVES-1             */
    int pitch_slot;  /* slot    0 (top line) .. 8 (bot line)
                        within the staff                       */
    int screen_x;    /* pixel x centre of this cell           */
    int screen_y;    /* pixel y centre of this cell           */
} Note;

Note notes[MAX_NOTES];
int  num_notes = 0;

/* ═══════════════════════════════════════════════════════════════════════
   Grid → screen coordinate helpers
   ═══════════════════════════════════════════════════════════════════════ */

/* Column index → pixel x (centre of that step cell) */
static int col_to_x(int col)
{
    if (col < 0)           col = 0;
    if (col >= TOTAL_COLS) col = TOTAL_COLS - 1;
    return STAFF_X0 + col * STEP_W + STEP_W / 2;
}

/* Row index (0..TOTAL_ROWS-1) → pixel y, also fills staff/slot indices */
static int row_to_y(int row, int *staff_out, int *slot_out)
{
    int s, slot;
    if (row < 0)           row = 0;
    if (row >= TOTAL_ROWS) row = TOTAL_ROWS - 1;
    s    = row / SLOTS_PER_STAFF;
    slot = row % SLOTS_PER_STAFF;
    if (staff_out) *staff_out = s;
    if (slot_out)  *slot_out  = slot;
    return staff_top[s] + slot * (STAFF_SPACING / 2);
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

/* Restore one pixel: draw note black if inside any placed note oval,
   otherwise restore background. */
void restore_pixel(int x, int y)
{
    int i;
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;
    for (i = 0; i < num_notes; i++) {
        int ddx = x - notes[i].screen_x;
        int ddy = y - notes[i].screen_y;
        int a = ddx * NOTE_RY;
        int b = ddy * NOTE_RX;
        int r = NOTE_RX * NOTE_RY;
        if (a*a + b*b <= r*r) { plot_pixel(x, y, BLACK); return; }
    }
    plot_pixel(x, y, bg[y][x]);
}

/* ═══════════════════════════════════════════════════════════════════════
   Grid cell highlight (cursor)
   Fills the cell rectangle with CURSOR_COLOR on draw,
   or restores bg/notes on erase.
   ═══════════════════════════════════════════════════════════════════════ */
static void draw_cursor_cell(int cx, int cy)
{
    int x, y;
    int x0 = cx - CELL_W / 2,  x1 = cx + CELL_W / 2;
    int y0 = cy - CELL_H / 2,  y1 = cy + CELL_H / 2;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            plot_pixel(x, y, CURSOR_COLOR);
}

static void erase_cursor_cell(int cx, int cy)
{
    int x, y;
    int x0 = cx - CELL_W / 2,  x1 = cx + CELL_W / 2;
    int y0 = cy - CELL_H / 2,  y1 = cy + CELL_H / 2;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            restore_pixel(x, y);
}

/* ═══════════════════════════════════════════════════════════════════════
   Note head draw / erase  (filled black oval)
   ═══════════════════════════════════════════════════════════════════════ */
static void draw_note_head(int cx, int cy)
{
    int dx, dy;
    for (dy = -NOTE_RY; dy <= NOTE_RY; dy++)
        for (dx = -NOTE_RX; dx <= NOTE_RX; dx++) {
            int a = dx*NOTE_RY, b = dy*NOTE_RX, r = NOTE_RX*NOTE_RY;
            if (a*a + b*b <= r*r) plot_pixel(cx+dx, cy+dy, BLACK);
        }
}

static void erase_note_head(int cx, int cy)
{
    int dx, dy;
    for (dy = -NOTE_RY; dy <= NOTE_RY; dy++)
        for (dx = -NOTE_RX; dx <= NOTE_RX; dx++) {
            int a = dx*NOTE_RY, b = dy*NOTE_RX, r = NOTE_RX*NOTE_RY;
            if (a*a + b*b <= r*r)
                plot_pixel(cx+dx, cy+dy, bg[cy+dy][cx+dx]);
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

/* ═══════════════════════════════════════════════════════════════════════
   Keyboard init: reset → BAT → enable scanning
   ═══════════════════════════════════════════════════════════════════════ */
static void keyboard_init(void)
{
    volatile int *ps2 = (volatile int *)PS2_BASE;
    int timeout;

    ps2_flush(ps2);

    *ps2 = 0xFF;   /* Reset */
    timeout = 3000000;
    while (timeout-- > 0) {
        int v = *ps2;
        if ((v & PS2_RVALID) && (v & 0xFF) == 0xAA) break;
    }

    ps2_flush(ps2);

    *ps2 = 0xF4;   /* Enable scanning */
    timeout = 2000000;
    while (timeout-- > 0) {
        int v = *ps2;
        if ((v & PS2_RVALID) && (v & 0xFF) == 0xFA) break;
    }

    ps2_flush(ps2);
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

    keyboard_init();
    build_and_draw_background();

    /* ── Grid cursor ────────────────────────────────────────────────── */
    int cur_col   = 0;
    int cur_row   = 0;
    int cur_staff = 0;
    int cur_slot  = 0;
    int cur_x     = col_to_x(cur_col);
    int cur_y     = row_to_y(cur_row, &cur_staff, &cur_slot);

    draw_cursor_cell(cur_x, cur_y);
    pixel_buffer_start = *pixel_ctrl;

    /* ── PS/2 decode state ──────────────────────────────────────────── */
    int got_break = 0;   /* 1 after seeing 0xF0 break prefix */

    while (1)
    {
        int raw = ps2_read_byte(ps2);
        if (raw < 0) continue;

        unsigned char b = (unsigned char)raw;

        /* Ignore E0 extended prefix (arrow keys, etc.) */
        if (b == 0xE0) continue;

        /* Break prefix: next byte is the released key – skip action */
        if (b == KEY_BREAK) { got_break = 1; continue; }
        if (got_break)      { got_break = 0; continue; }

        /* ── Decode make code ────────────────────────────────────────── */
        {
            int new_col = cur_col;
            int new_row = cur_row;
            int moved = 0, place = 0, del = 0;

            if (b == KEY_A && cur_col > 0)              { new_col--; moved = 1; }
            if (b == KEY_D && cur_col < TOTAL_COLS - 1) { new_col++; moved = 1; }
            if (b == KEY_W && cur_row > 0)              { new_row--; moved = 1; }
            if (b == KEY_S && cur_row < TOTAL_ROWS - 1) { new_row++; moved = 1; }
            if (b == KEY_SPACE)  place = 1;
            if (b == KEY_DELETE) del   = 1;

            /* ── Move cursor ─────────────────────────────────────────── */
            if (moved) {
                erase_cursor_cell(cur_x, cur_y);

                cur_col = new_col;
                cur_row = new_row;
                cur_x   = col_to_x(cur_col);
                cur_y   = row_to_y(cur_row, &cur_staff, &cur_slot);

                draw_cursor_cell(cur_x, cur_y);
            }

            /* ── Place note ──────────────────────────────────────────── */
            if (place && num_notes < MAX_NOTES) {
                int i, dup = 0;
                for (i = 0; i < num_notes; i++) {
                    if (notes[i].step == cur_col &&
                        notes[i].staff == cur_staff &&
                        notes[i].pitch_slot == cur_slot) { dup = 1; break; }
                }
                if (!dup) {
                    notes[num_notes].step       = cur_col;
                    notes[num_notes].staff      = cur_staff;
                    notes[num_notes].pitch_slot = cur_slot;
                    notes[num_notes].screen_x   = cur_x;
                    notes[num_notes].screen_y   = cur_y;
                    num_notes++;
                    draw_note_head(cur_x, cur_y);
                    /* Redraw cursor so it's visible over the note */
                    draw_cursor_cell(cur_x, cur_y);
                }
            }

            /* ── Delete note at cursor ───────────────────────────────── */
            if (del) {
                int i;
                for (i = 0; i < num_notes; i++) {
                    if (notes[i].step == cur_col &&
                        notes[i].staff == cur_staff &&
                        notes[i].pitch_slot == cur_slot) {
                        erase_note_head(notes[i].screen_x, notes[i].screen_y);
                        notes[i] = notes[num_notes - 1];
                        num_notes--;
                        /* Redraw all notes (one shifted due to swap) */
                        for (i = 0; i < num_notes; i++)
                            draw_note_head(notes[i].screen_x, notes[i].screen_y);
                        draw_cursor_cell(cur_x, cur_y);
                        break;
                    }
                }
            }
        }

        pixel_buffer_start = *pixel_ctrl;
    }

    return 0;
}
