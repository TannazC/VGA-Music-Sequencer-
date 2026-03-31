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
#define PIXEL_BUF_CTRL 0xFF203020
#define PS2_BASE 0xFF200100
#define PS2_RVALID 0x8000

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

#define KEY_SPACE 0x29
#define KEY_DELETE 0x66
#define KEY_Q 0x15
#define KEY_E 0x24
#define KEY_T 0x2C
#define KEY_R 0x2D
#define KEY_M 0x3A
#define KEY_N 0x31
#define KEY_BREAK 0xF0
#define KEY_UP 0x75
#define KEY_DOWN 0x72
#define KEY_MINUS 0x4E
#define KEY_EQUALS 0x55

/* Page Navigation Keys */
#define KEY_COMMA 0x41  /* Previous Page (<) */
#define KEY_PERIOD 0x49 /* Next Page (>) */

int cur_page = 1;
int max_pages = 4;
volatile int g_drawing_ui = 0;

/* FIXED: Audio transport flags added back in so main() can control playback */
volatile int seq_is_playing = 0;
volatile int seq_is_paused = 0;
volatile int seq_user_stopped = 0;
volatile int seq_user_restarted = 0;

#define UI_SAFE_ZONE 46

/* ═══════════════════════════════════════════════════════════════════════
   Note types
   ═══════════════════════════════════════════════════════════════════════ */
#define NOTE_WHOLE 0
#define NOTE_HALF 1
#define NOTE_QUARTER 2
#define NOTE_BEAM2_8TH 3
#define NOTE_BEAM4_16TH 4
#define NOTE_BEAM2_16TH 5
#define NOTE_SINGLE16TH 6
#define NOTE_REST 7
#define NUM_NOTE_TYPES 8

#define ACC_NONE 0
#define ACC_SHARP 1
#define ACC_FLAT 2
#define ACC_NATURAL 3

static const int note_duration_64[NUM_NOTE_TYPES] = {
    64, 32, 16, 16, 16, 8, 4, 16};

static const int note_num_heads[NUM_NOTE_TYPES] = {
    1, 1, 1, 2, 4, 2, 1, 1};

#define SLOTS_PER_STAFF ((LINES_PER_STAFF - 1) * 2 + 3)
#define TOTAL_ROWS (NUM_STAVES * SLOTS_PER_STAFF)
#define TOTAL_COLS NUM_STEPS

#define CURSOR_COLOR ((short int)0x051F)
#define WHITE ((short int)0xFFFF)
#define BLACK ((short int)0x0000)

#define STEM_X_OFF (OVAL_W) / 2
#define STEM_HEIGHT 11
#define BEAM_THICK 2
#define FLAG_LEN 5
#define CELL_W (STEP_W - 1)
#define CELL_H (STAFF_SPACING / 2)

#define MAX_NOTES 512
#define MAX_HEADS 4

#ifndef NOTE_STRUCT_DEFINED
#define NOTE_STRUCT_DEFINED
typedef struct
{
    int step;
    int staff;
    int pitch_slot;
    int note_type;
    int duration_64;
    int accidental;
    int num_heads;
    int head_step[MAX_HEADS];
    int head_pitch_slot[MAX_HEADS];
    int screen_x;
    int screen_y;
    int head_x[MAX_HEADS];
    int head_y[MAX_HEADS];
    int page;
} Note;
#endif

Note notes[MAX_NOTES];
int num_notes = 0;

int cur_note_type = NOTE_QUARTER;
int cur_accidental = ACC_NONE;

/* ═══════════════════════════════════════════════════════════════════════
   Grid helpers
   ═══════════════════════════════════════════════════════════════════════ */
static int col_to_x(int col)
{
    if (col < 0)
        col = 0;
    if (col >= TOTAL_COLS)
        col = TOTAL_COLS - 1;
    return STAFF_X0 + col * STEP_W + STEP_W / 2;
}

static int row_to_y(int row, int *staff_out, int *slot_out)
{
    int s, slot;
    if (row < 0)
        row = 0;
    if (row >= TOTAL_ROWS)
        row = TOTAL_ROWS - 1;
    s = row / SLOTS_PER_STAFF;
    slot = row % SLOTS_PER_STAFF;
    if (staff_out)
        *staff_out = s;
    if (slot_out)
        *slot_out = slot;
    return staff_top[s] + (slot - 1) * (STAFF_SPACING / 2);
}

/* ═══════════════════════════════════════════════════════════════════════
   Safe UI Wrappers & Pixels
   ═══════════════════════════════════════════════════════════════════════ */
void plot_pixel(int x, int y, short int c)
{
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT)
        return;
    if (y < UI_SAFE_ZONE && !g_start_screen_active && !g_drawing_ui)
        return;
    *(volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = c;
}

void safe_draw_toolbar(int nt)
{
    g_drawing_ui = 1;
    draw_toolbar(nt);
    g_drawing_ui = 0;
}

void safe_draw_row2(int acc)
{
    g_drawing_ui = 1;
    draw_toolbar_row2(acc);
    g_drawing_ui = 0;
}

void safe_set_note_type(int nt)
{
    g_drawing_ui = 1;
    toolbar_set_note_type(nt);
    g_drawing_ui = 0;
}

void restore_pixel(int x, int y)
{
    int i, h;
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT)
        return;

    for (i = 0; i < num_notes; i++)
    {
        for (h = 0; h < notes[i].num_heads; h++)
        {
            int ddx = x - notes[i].head_x[h];
            int ddy = y - notes[i].head_y[h];
            int nt = notes[i].note_type;
            if (nt == NOTE_WHOLE || nt == NOTE_HALF)
            {
                int bx = ddx + OPEN_OVAL_W / 2;
                int by = ddy + OPEN_OVAL_H / 2;
                if (bx >= 0 && bx < OPEN_OVAL_W && by >= 0 && by < OPEN_OVAL_H)
                    if (OPEN_OVAL[by][bx])
                    {
                        plot_pixel(x, y, BLACK);
                        return;
                    }
            }
            else
            {
                int bx = ddx + OVAL_W / 2;
                int by = ddy + OVAL_H / 2;
                if (bx >= 0 && bx < OVAL_W && by >= 0 && by < OVAL_H)
                    if (FILLED_OVAL[by][bx])
                    {
                        plot_pixel(x, y, BLACK);
                        return;
                    }
            }
        }
    }
    plot_pixel(x, y, bg[y][x]);
}

static void draw_cursor_cell(int cx, int cy)
{
    int x, y;
    int x0 = cx - CELL_W / 2, x1 = cx + CELL_W / 2;
    int y0 = cy - CELL_H / 2, y1 = cy + CELL_H / 2;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            plot_pixel(x, y, CURSOR_COLOR);
}

static void erase_cursor_cell(int cx, int cy)
{
    int x, y;
    int x0 = cx - CELL_W / 2, x1 = cx + CELL_W / 2;
    int y0 = cy - CELL_H / 2, y1 = cy + CELL_H / 2;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
        {
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
                plot_pixel(ax + dx - OVAL_W / 2, ay + dy - OVAL_H / 2, c);
}

static void open_oval(int ax, int ay, short int c)
{
    int dx, dy;
    for (dy = 0; dy < OPEN_OVAL_H; dy++)
        for (dx = 0; dx < OPEN_OVAL_W; dx++)
            if (OPEN_OVAL[dy][dx])
                plot_pixel(ax + dx - OPEN_OVAL_W / 2, ay + dy - OPEN_OVAL_H / 2, c);
}

static void double_flag(int ax, int ay, short int c)
{
    int k;
    int sx = ax + STEM_X_OFF;
    for (k = 0; k < FLAG_LEN; k++)
    {
        plot_pixel(sx + k, ay - STEM_HEIGHT + k, c);
        plot_pixel(sx + k + 1, ay - STEM_HEIGHT + k, c);
        plot_pixel(sx + k, ay - STEM_HEIGHT + k + 1, c);
    }
    for (k = 0; k < FLAG_LEN; k++)
    {
        plot_pixel(sx + k, ay - STEM_HEIGHT + 3 + k, c);
        plot_pixel(sx + k + 1, ay - STEM_HEIGHT + 3 + k, c);
        plot_pixel(sx + k, ay - STEM_HEIGHT + 3 + k + 1, c);
    }
}

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
    int ax = cx - STEP_W / 2;

    if (accidental == ACC_NONE)
        return;

    if (accidental == ACC_SHARP)
    {
        for (y = cy - 6; y <= cy + 2; y++)
        {
            plot_pixel(ax - 1, y, c);
            plot_pixel(ax + 1, y, c);
        }
        for (x = ax - 4; x <= ax + 2; x++)
        {
            plot_pixel(x, cy - 3, c);
            plot_pixel(x + 1, cy + 1, c);
        }
        return;
    }

    if (accidental == ACC_FLAT)
    {
        for (y = cy - 6; y <= cy + 3; y++)
            plot_pixel(ax - 1, y, c);

        plot_pixel(ax, cy - 1, c);
        plot_pixel(ax + 1, cy, c);
        plot_pixel(ax + 2, cy + 1, c);
        plot_pixel(ax + 2, cy + 2, c);
        plot_pixel(ax + 1, cy + 3, c);
        plot_pixel(ax, cy + 4, c);
        return;
    }

    if (accidental == ACC_NATURAL)
    {
        for (y = cy - 6; y <= cy + 3; y++)
        {
            plot_pixel(ax - 1, y, c);
            plot_pixel(ax + 1, y - 2, c);
        }
        for (x = ax - 1; x <= ax + 2; x++)
        {
            plot_pixel(x, cy - 2, c);
            plot_pixel(x, cy + 2, c);
        }
    }
}

static void draw_stem_segment(int ax, int ay, short int c)
{
    int y;
    for (y = ay - STEM_HEIGHT; y <= ay; y++)
        plot_pixel(ax + STEM_X_OFF, y, c);
}

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
    if (x1 < x0)
    {
        int tx = x0, ty = y0;
        x0 = x1;
        y0 = y1;
        x1 = tx;
        y1 = ty;
    }
    if (x1 == x0)
    {
        beam_bar(x0, x1, y0, thick, c);
        return;
    }
    for (x = x0; x <= x1; x++)
    {
        int y = y0 + (y1 - y0) * (x - x0) / (x1 - x0);
        for (t = 0; t < thick; t++)
            plot_pixel(x, y + t, c);
    }
}

#define MAX_BEAM_DELTA 6

static void draw_note_instance(const Note *n, short int c)
{
    int i;

    if (n->num_heads <= 0)
        return;

    if (n->note_type != NOTE_REST)
        draw_accidental_symbol(n->head_x[0], n->head_y[0], n->accidental, c);

    switch (n->note_type)
    {
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
        if (n->num_heads == 2)
        {
            for (i = 0; i < n->num_heads; i++)
            {
                filled_oval(n->head_x[i], n->head_y[i], c);
                draw_stem_segment(n->head_x[i], n->head_y[i], c);
            }
            beam_segment(n->head_x[0] + STEM_X_OFF, n->head_y[0] - STEM_HEIGHT,
                         n->head_x[1] + STEM_X_OFF, n->head_y[1] - STEM_HEIGHT,
                         BEAM_THICK, c);
        }
        else
        {
            int beam_y = n->head_y[0] - STEM_HEIGHT;
            for (i = 1; i < n->num_heads; i++)
            {
                int top_i = n->head_y[i] - STEM_HEIGHT;
                if (top_i < beam_y)
                    beam_y = top_i;
            }
            for (i = 0; i < n->num_heads; i++)
            {
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
        if (n->num_heads == 2)
        {
            for (i = 0; i < n->num_heads; i++)
            {
                filled_oval(n->head_x[i], n->head_y[i], c);
                draw_stem_segment(n->head_x[i], n->head_y[i], c);
            }
            beam_segment(n->head_x[0] + STEM_X_OFF, n->head_y[0] - STEM_HEIGHT,
                         n->head_x[1] + STEM_X_OFF, n->head_y[1] - STEM_HEIGHT,
                         BEAM_THICK, c);
            beam_segment(n->head_x[0] + STEM_X_OFF, n->head_y[0] - STEM_HEIGHT + BEAM_THICK + 1,
                         n->head_x[1] + STEM_X_OFF, n->head_y[1] - STEM_HEIGHT + BEAM_THICK + 1,
                         BEAM_THICK, c);
        }
        else
        {
            int beam_y = n->head_y[0] - STEM_HEIGHT;
            for (i = 1; i < n->num_heads; i++)
            {
                int top_i = n->head_y[i] - STEM_HEIGHT;
                if (top_i < beam_y)
                    beam_y = top_i;
            }
            for (i = 0; i < n->num_heads; i++)
            {
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
        if (n->num_heads == 2)
        {
            for (i = 0; i < n->num_heads; i++)
            {
                filled_oval(n->head_x[i], n->head_y[i], c);
                draw_stem_segment(n->head_x[i], n->head_y[i], c);
            }
            beam_segment(n->head_x[0] + STEM_X_OFF, n->head_y[0] - STEM_HEIGHT,
                         n->head_x[1] + STEM_X_OFF, n->head_y[1] - STEM_HEIGHT,
                         BEAM_THICK, c);
            beam_segment(n->head_x[0] + STEM_X_OFF, n->head_y[0] - STEM_HEIGHT + BEAM_THICK + 1,
                         n->head_x[1] + STEM_X_OFF, n->head_y[1] - STEM_HEIGHT + BEAM_THICK + 1,
                         BEAM_THICK, c);
        }
        else
        {
            int beam_y = n->head_y[0] - STEM_HEIGHT;
            for (i = 1; i < n->num_heads; i++)
            {
                int top_i = n->head_y[i] - STEM_HEIGHT;
                if (top_i < beam_y)
                    beam_y = top_i;
            }
            for (i = 0; i < n->num_heads; i++)
            {
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
        int cx = n->head_x[0];
        int cy = n->head_y[0];
        plot_pixel(cx + (-1), cy + (-9), c);
        plot_pixel(cx + (0), cy + (-8), c);
        plot_pixel(cx + (0), cy + (-7), c);
        plot_pixel(cx + (1), cy + (-7), c);
        plot_pixel(cx + (0), cy + (-6), c);
        plot_pixel(cx + (1), cy + (-6), c);
        plot_pixel(cx + (2), cy + (-6), c);
        plot_pixel(cx + (0), cy + (-5), c);
        plot_pixel(cx + (1), cy + (-5), c);
        plot_pixel(cx + (2), cy + (-5), c);
        plot_pixel(cx + (-1), cy + (-4), c);
        plot_pixel(cx + (0), cy + (-4), c);
        plot_pixel(cx + (1), cy + (-4), c);
        plot_pixel(cx + (2), cy + (-4), c);
        plot_pixel(cx + (-2), cy + (-3), c);
        plot_pixel(cx + (-1), cy + (-3), c);
        plot_pixel(cx + (0), cy + (-3), c);
        plot_pixel(cx + (1), cy + (-3), c);
        plot_pixel(cx + (-2), cy + (-2), c);
        plot_pixel(cx + (-1), cy + (-2), c);
        plot_pixel(cx + (0), cy + (-2), c);
        plot_pixel(cx + (1), cy + (-2), c);
        plot_pixel(cx + (-2), cy + (-1), c);
        plot_pixel(cx + (-1), cy + (-1), c);
        plot_pixel(cx + (0), cy + (-1), c);
        plot_pixel(cx + (-2), cy + (0), c);
        plot_pixel(cx + (-1), cy + (0), c);
        plot_pixel(cx + (0), cy + (0), c);
        plot_pixel(cx + (-1), cy + (1), c);
        plot_pixel(cx + (0), cy + (1), c);
        plot_pixel(cx + (0), cy + (2), c);
        plot_pixel(cx + (1), cy + (2), c);
        plot_pixel(cx + (-2), cy + (3), c);
        plot_pixel(cx + (-1), cy + (3), c);
        plot_pixel(cx + (0), cy + (3), c);
        plot_pixel(cx + (1), cy + (3), c);
        plot_pixel(cx + (2), cy + (3), c);
        plot_pixel(cx + (-3), cy + (4), c);
        plot_pixel(cx + (-2), cy + (4), c);
        plot_pixel(cx + (-1), cy + (4), c);
        plot_pixel(cx + (0), cy + (4), c);
        plot_pixel(cx + (1), cy + (4), c);
        plot_pixel(cx + (2), cy + (4), c);
        plot_pixel(cx + (-3), cy + (5), c);
        plot_pixel(cx + (-2), cy + (5), c);
        plot_pixel(cx + (-1), cy + (5), c);
        plot_pixel(cx + (-3), cy + (6), c);
        plot_pixel(cx + (-2), cy + (6), c);
        plot_pixel(cx + (-1), cy + (6), c);
        plot_pixel(cx + (-2), cy + (7), c);
        plot_pixel(cx + (-1), cy + (7), c);
        plot_pixel(cx + (-1), cy + (8), c);
        break;
    }

    default:
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   Erase: restore the full bounding box
   ═══════════════════════════════════════════════════════════════════════ */
static void erase_note_instance(const Note *n)
{
    int x, y, i;
    int min_x, max_x, min_y, max_y;

    if (n->num_heads <= 0)
        return;

    if (n->note_type == NOTE_REST)
    {
        int cx = n->head_x[0];
        int cy = n->head_y[0];

        min_x = cx - 6;
        max_x = cx + 5;
        min_y = cy - 11;
        max_y = cy + 11;

        for (y = min_y; y <= max_y; y++)
            for (x = min_x; x <= max_x; x++)
                if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT)
                    plot_pixel(x, y, bg[y][x]);
        return;
    }

    min_x = n->head_x[0];
    max_x = n->head_x[0];
    min_y = n->head_y[0];
    max_y = n->head_y[0];

    for (i = 1; i < n->num_heads; i++)
    {
        if (n->head_x[i] < min_x)
            min_x = n->head_x[i];
        if (n->head_x[i] > max_x)
            max_x = n->head_x[i];
        if (n->head_y[i] < min_y)
            min_y = n->head_y[i];
        if (n->head_y[i] > max_y)
            max_y = n->head_y[i];
    }

    x = min_x - STEP_W;
    min_x = x;
    x = max_x + OVAL_W / 2 + STEM_X_OFF + FLAG_LEN + 4;
    max_x = x;
    y = min_y - STEM_HEIGHT - 8;
    min_y = y;
    y = max_y + OVAL_H / 2 + 4;
    max_y = y;

    for (y = min_y; y <= max_y; y++)
        for (x = min_x; x <= max_x; x++)
            if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT)
                plot_pixel(x, y, bg[y][x]);
}

static void redraw_all_notes(void)
{
    int i;
    for (i = 0; i < num_notes; i++)
    {
        if (notes[i].page == cur_page)
        {
            draw_note_instance(&notes[i], COLOR_BLACK);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   fill_note_heads
   ═══════════════════════════════════════════════════════════════════════ */
static void fill_note_heads(Note *n, int col, int staff, int slot,
                            int sx, int sy, int nt)
{
    int i;
    int nh = note_num_heads[nt];
    n->num_heads = nh;
    for (i = 0; i < nh; i++)
    {
        n->head_step[i] = col + i;
        n->head_pitch_slot[i] = slot;
        n->head_x[i] = sx + i * STEP_W;
        n->head_y[i] = sy;
    }
    for (i = nh; i < MAX_HEADS; i++)
    {
        n->head_step[i] = n->head_pitch_slot[i] = 0;
        n->head_x[i] = n->head_y[i] = 0;
    }
}

static int find_note_head_at(int col, int staff, int slot, int *note_idx, int *head_idx)
{
    int i, h;
    for (i = 0; i < num_notes; i++)
    {
        if (notes[i].staff != staff)
            continue;
        for (h = 0; h < notes[i].num_heads; h++)
        {
            if (notes[i].head_step[h] == col && notes[i].head_pitch_slot[h] == slot)
            {
                if (note_idx)
                    *note_idx = i;
                if (head_idx)
                    *head_idx = h;
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

    if (head_idx == 0)
    {
        n->pitch_slot = new_slot;
        n->screen_y = n->head_y[0];
    }

    redraw_all_notes();
    return 1;
}

static int col_is_occupied(int col, int staff)
{
    int i, h;
    for (i = 0; i < num_notes; i++)
    {
        if (notes[i].page != cur_page || notes[i].staff != staff)
            continue;
        for (h = 0; h < notes[i].num_heads; h++)
        {
            if (notes[i].head_step[h] == col)
                return 1;
        }
    }
    return 0;
}

static void place_note(int cur_col, int cur_staff, int cur_slot,
                       int cur_x, int cur_y, int nt)
{
    int h;
    int nh = note_num_heads[nt];

    if (cur_col < 1 || cur_col + nh - 1 >= TOTAL_COLS)
        return;

    for (h = 0; h < nh; h++)
    {
        if (col_is_occupied(cur_col + h, cur_staff))
            return;
    }

    if (num_notes >= MAX_NOTES)
        return;

    notes[num_notes].step = cur_col;
    notes[num_notes].staff = cur_staff;
    notes[num_notes].pitch_slot = cur_slot;
    notes[num_notes].note_type = nt;
    notes[num_notes].duration_64 = note_duration_64[nt];
    notes[num_notes].accidental = (nt == NOTE_REST) ? ACC_NONE : cur_accidental;
    notes[num_notes].screen_x = cur_x;
    notes[num_notes].screen_y = cur_y;
    notes[num_notes].page = cur_page;
    fill_note_heads(&notes[num_notes], cur_col, cur_staff, cur_slot,
                    cur_x, cur_y, nt);
    num_notes++;

    draw_note_instance(&notes[num_notes - 1], COLOR_BLACK);
    draw_cursor_cell(cur_x, cur_y);
}

static void delete_note(int cur_col, int cur_staff, int cur_slot,
                        int cur_x, int cur_y)
{
    int i, h;
    for (i = 0; i < num_notes; i++)
    {
        if (notes[i].page != cur_page || notes[i].staff != cur_staff)
            continue;
        for (h = 0; h < notes[i].num_heads; h++)
        {
            if (notes[i].head_step[h] == cur_col &&
                notes[i].head_pitch_slot[h] == cur_slot)
            {
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
    if (v & PS2_RVALID)
        return v & 0xFF;
    return -1;
}

static void ps2_flush(volatile int *ps2)
{
    int i;
    for (i = 0; i < 512; i++)
    {
        if (!(*ps2 & PS2_RVALID))
            break;
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
    while (timeout-- > 0)
    {
        int v = *ps2;
        if ((v & PS2_RVALID) && (v & 0xFF) == 0xAA)
            break;
    }
    ps2_flush(ps2);
    *ps2 = 0xF4;
    timeout = 2000000;
    while (timeout-- > 0)
    {
        int v = *ps2;
        if ((v & PS2_RVALID) && (v & 0xFF) == 0xFA)
            break;
    }
    ps2_flush(ps2);
}

/* ═══════════════════════════════════════════════════════════════════════
   HUD & Navigation
   ═══════════════════════════════════════════════════════════════════════ */

void draw_mini_note_glyph(int cx, int cy, int nt, int accidental, short int c)
{
    Note mini;
    int i;
    int MINI_STEP = 9;
    mini.note_type = nt;
    mini.accidental = accidental;
    mini.num_heads = note_num_heads[nt];
    mini.head_x[0] = cx;
    mini.head_y[0] = cy;
    for (i = 1; i < mini.num_heads; i++)
    {
        mini.head_x[i] = mini.head_x[0] + i * MINI_STEP;
        mini.head_y[i] = cy;
    }
    draw_note_instance(&mini, c);
}

void update_note_indicator(int nt, int accidental, int cur_p, int max_p)
{
    int x, y;
    for (y = 210; y < FB_HEIGHT; y++)
    {
        for (x = 0; x < FB_WIDTH; x++)
        {
            plot_pixel(x, y, bg[y][x]);
        }
    }

    g_drawing_ui = 1;
    tb_draw_string(5, 222, "CURRENT:", BLACK);
    draw_mini_note_glyph(65, 230, nt, (nt == NOTE_REST) ? ACC_NONE : accidental, BLACK);
    draw_page_indicator(cur_p, max_p);
    draw_bottom_tab();
    g_drawing_ui = 0;
}

void play_sequence(void);

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

static void switch_page(int new_page, int cur_x, int cur_y)
{
    if (new_page < 1 || new_page > max_pages)
        return;
    cur_page = new_page;
    build_and_draw_background();
    safe_draw_toolbar(cur_note_type);
    safe_draw_row2(cur_accidental);
    update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages);
    redraw_all_notes();
    draw_cursor_cell(cur_x, cur_y);
}

/* =======================================================================
   Preload Song Logic (Ode to Joy - 2 Full Pages!)
   ======================================================================= */
static void inject_note(int col, int staff, int slot, int nt, int page)
{
    if (num_notes >= MAX_NOTES)
        return;
    Note *n = &notes[num_notes];
    n->step = col;
    n->staff = staff;
    n->pitch_slot = slot;
    n->note_type = nt;
    n->duration_64 = note_duration_64[nt];
    n->accidental = ACC_NONE;
    n->page = page;

    int sx = col_to_x(col);
    int sy = row_to_y(staff * SLOTS_PER_STAFF + slot, NULL, NULL);

    n->screen_x = sx;
    n->screen_y = sy;

    int nh = note_num_heads[nt];
    n->num_heads = nh;
    for (int i = 0; i < nh; i++)
    {
        n->head_step[i] = col + i;
        n->head_pitch_slot[i] = slot;
        n->head_x[i] = sx + i * STEP_W;
        n->head_y[i] = sy;
    }
    for (int i = nh; i < MAX_HEADS; i++)
    {
        n->head_step[i] = 0;
        n->head_pitch_slot[i] = 0;
        n->head_x[i] = 0;
        n->head_y[i] = 0;
    }
    num_notes++;
}

/* ═══════════════════════════════════════════════════════════════════════
   Preloaded Songs
   Slot reference: 0=G5 1=F5 2=E5 3=D5 4=C5 5=B4 6=A4 7=G4 8=F4 9=E4 10=D4
   ═══════════════════════════════════════════════════════════════════════ */

/* -------------------------------------------------------
   Song 1 – Ode to Joy (Beethoven, Symphony No. 9)
   ------------------------------------------------------- */
static void preload_ode_to_joy(void)
{
    num_notes = 0;

    int song_slots[] = {
        /* PAGE 1 */
        /* Phrase 1 (Staff 0) */ 2, 2, 1, 0, 0, 1, 2, 3, 4, 4, 3, 2, 2, 3, 3, 3,
        /* Phrase 2 (Staff 1) */ 2, 2, 1, 0, 0, 1, 2, 3, 4, 4, 3, 2, 3, 4, 4, 4,
        /* Phrase 3 (Staff 2) */ 3, 3, 2, 4, 3, 2, 2, 4, 3, 2, 3, 4, 4, 3, 7, 7,
        /* Phrase 4 (Staff 3) */ 2, 2, 1, 0, 0, 1, 2, 3, 4, 4, 3, 2, 3, 4, 4, 4,
        /* PAGE 2 */
        /* Phrase 5 (Staff 0) */ 2, 2, 1, 0, 0, 1, 2, 3, 4, 4, 3, 2, 2, 3, 3, 3,
        /* Phrase 6 (Staff 1) */ 2, 2, 1, 0, 0, 1, 2, 3, 4, 4, 3, 2, 3, 4, 4, 4,
        /* Phrase 7 (Staff 2) */ 3, 3, 2, 4, 3, 2, 2, 4, 3, 2, 3, 4, 4, 3, 7, 7,
        /* Phrase 8 (Staff 3) */ 2, 2, 1, 0, 0, 1, 2, 3, 4, 4, 3, 2, 3, 4, 4, 4};

    int song_types[] = {
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_HALF, NOTE_REST,

        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_HALF, NOTE_REST,

        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_HALF, NOTE_REST,

        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_HALF, NOTE_REST,

        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_HALF, NOTE_REST,

        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_HALF, NOTE_REST,

        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_HALF, NOTE_REST,

        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_HALF, NOTE_REST};

    for (int i = 0; i < 128; i++)
    {
        int page = (i / 64) + 1;
        int staff = (i % 64) / 16;
        int col = (i % 16) + 1;
        inject_note(col, staff, song_slots[i], song_types[i], page);
    }
    toolbar_state.instrument = TB_INST_PIANO_REVERB;
    toolbar_state.bpm = 160;
}

/* -------------------------------------------------------
   Song 2 – O Canada (C. Lavallée)
   Key: G major (F# throughout). 4/4 time. BPM ~112.
   Slot map: 0=G5 1=F5 2=E5 3=D5 4=C5 5=B4 6=A4 7=G4 8=F4 9=E4 10=D4
   F# = slot 1 + ACC_SHARP.  A5 mapped down to G5 (slot 0).
   4 bars per staff (4 qtr beats each = 16 cols). 4 staves = bars 1-16.
   Dotted-quarter approximated as QUARTER; dotted-half as HALF+QUARTER.
   ------------------------------------------------------- */
static void preload_o_canada(void)
{
    num_notes = 0;

    /*
     * O Canada
     * Slot map:
     *   0=G5  1=F5  2=E5  3=D5  4=C5  5=B4  6=A4  7=G4  8=F4  9=E4  10=D4
     *
     * Accidentals:
     *   C#5 = slot 4 + ACC_SHARP
     *   F#4 = slot 8 + ACC_SHARP
     *
     * Duration handling note:
     *   Since each preload entry occupies one grid column, dotted-half is approximated
     *   as NOTE_HALF visually, and whole/final uses NOTE_WHOLE.
     */

    int slots[64] = {
        /* Line 1 */
        7, 3, 3, 7, 6, 5, 4, 3, 6, 5, 4, 4, 3, 2, 7, 7,

        /* Line 2 */
        3, 3, 4, 4, 5, 6, 5, 6, 7, 6, 5, 6, 5, 4, 7, 6,

        /* Line 3 */
        6, 7, 6, 5, 4, 5, 6, 6, 7, 5, 5, 4, 5, 3, 7, 7,

        /* Line 4 */
        7, 6, 6, 4, 4, 6, 7, 6, 5, 4, 4, 4, 6, 7, 8, 7};

    int types[64] = {
        /* Line 1 */
        NOTE_HALF, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_WHOLE, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_REST, NOTE_REST,

        /* Line 2 */
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,

        /* Line 3 */
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_HALF, NOTE_QUARTER, NOTE_REST, NOTE_REST,

        /* Line 4 */
        NOTE_QUARTER, NOTE_QUARTER, NOTE_HALF, NOTE_HALF,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_HALF, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_HALF, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_WHOLE};

    int accs[64];
    for (int i = 0; i < 64; i++)
    {
        accs[i] = ACC_NONE;
    }

    /* Line 1: B4 C#5 C#5 D5 E5 */
    accs[10] = ACC_SHARP; /* C#5 */
    accs[11] = ACC_SHARP; /* C#5 */

    /* Line 3: ... B4 B4 C#5 B4 A4 ... */
    accs[27 + 4] = ACC_SHARP; /* index 31 -> C#5 */

    /* Line 4: ... A4 B4 C#5(half) ... */
    accs[48 + 9] = ACC_SHARP; /* index 57 -> C#5 */

    /* Line 4: ... C#5 C5 A4 G4 F#4 */
    accs[48 + 10] = ACC_SHARP; /* index 58 -> C#5 */
    accs[48 + 14] = ACC_SHARP; /* index 62 -> F#4 */

    for (int i = 0; i < 64; i++)
    {
        int staff = i / 16;
        int col = (i % 16) + 1;

        inject_note(col, staff, slots[i], types[i], 1);

        if (accs[i] != ACC_NONE && num_notes > 0)
        {
            notes[num_notes - 1].accidental = accs[i];
        }
    }

    toolbar_state.instrument = TB_INST_PIANO_REVERB;
}

/* -------------------------------------------------------
   Song 3 – The Star-Spangled Banner (Francis Scott Key / J.S. Smith)
   Key: C major. 3/4 time. BPM ~96. Trumpet in B (concert pitch used here).
   Slot map: 0=G5 1=F5 2=E5 3=D5 4=C5 5=B4 6=A4 7=G4 8=F4 9=E4 10=D4
   C4 not available → D4 (slot 10) used as substitute.
   3/4: each bar = 3 quarter beats → with 16 cols per staff: ~5 bars per staff.
   ------------------------------------------------------- */
static void preload_star_spangled(void)
{
    num_notes = 0;
    max_pages = 2;

    /*
     * Star-Spangled Banner
     * Laid out to match the uploaded sheet across 2 pages.
     *
     * Slot map:
     *   0=G5  1=F5  2=E5  3=D5  4=C5  5=B4  6=A4  7=G4  8=F4  9=E4  10=D4
     *
     * Accidentals used here:
     *   F# = slot 8 + ACC_SHARP   (program range approximation)
     *
     * NOTE:
     * The sheet has 6 printed systems, so this preload uses:
     *   Page 1 -> 4 staves
     *   Page 2 -> 2 staves
     *
     * Also, because the preload format is one glyph per column, some dotted/held
     * values are approximated visually using HALF / WHOLE notes.
     */

    typedef struct
    {
        int page;
        int staff;
        int col;
        int slot;
        int type;
        int acc;
    } PreNote;

    static const PreNote song[] = {
        /* =========================
           PAGE 1, STAFF 0
           "O say can you see by the dawn's early light what so proudly we"
           ========================= */
        {1, 0, 1, 7, NOTE_QUARTER, ACC_NONE},
        {1, 0, 2, 9, NOTE_BEAM2_8TH, ACC_NONE}, /* O say */
        {1, 0, 4, 7, NOTE_QUARTER, ACC_NONE},
        {1, 0, 5, 7, NOTE_QUARTER, ACC_NONE},
        {1, 0, 6, 3, NOTE_QUARTER, ACC_NONE},
        {1, 0, 7, 5, NOTE_HALF, ACC_NONE},
        {1, 0, 9, 0, NOTE_BEAM2_8TH, ACC_NONE}, /* by the */
        {1, 0, 11, 2, NOTE_QUARTER, ACC_NONE},
        {1, 0, 12, 7, NOTE_QUARTER, ACC_NONE},
        {1, 0, 13, 8, NOTE_QUARTER, ACC_SHARP}, /* early */
        {1, 0, 14, 3, NOTE_HALF, ACC_NONE},
        {1, 0, 16, 6, NOTE_BEAM2_8TH, ACC_NONE},

        /* =========================
           PAGE 1, STAFF 1
           "hailed at the twilight's last gleaming? Whose broad stripes and bright stars thro' the"
           ========================= */
        {1, 1, 1, 0, NOTE_QUARTER, ACC_NONE},
        {1, 1, 2, 2, NOTE_QUARTER, ACC_NONE},
        {1, 1, 3, 3, NOTE_QUARTER, ACC_NONE},
        {1, 1, 4, 7, NOTE_QUARTER, ACC_NONE},
        {1, 1, 5, 7, NOTE_QUARTER, ACC_NONE},
        {1, 1, 6, 5, NOTE_QUARTER, ACC_NONE},
        {1, 1, 7, 7, NOTE_BEAM2_8TH, ACC_NONE},
        {1, 1, 9, 3, NOTE_QUARTER, ACC_NONE},
        {1, 1, 10, 7, NOTE_QUARTER, ACC_NONE},
        {1, 1, 11, 6, NOTE_QUARTER, ACC_NONE},
        {1, 1, 12, 3, NOTE_HALF, ACC_NONE},
        {1, 1, 14, 0, NOTE_BEAM2_8TH, ACC_NONE},

        /* =========================
           PAGE 1, STAFF 2
           "perilous fight o'er the ramparts we watched were so gallantly"
           ========================= */
        {1, 2, 1, 5, NOTE_QUARTER, ACC_NONE},
        {1, 2, 2, 7, NOTE_QUARTER, ACC_NONE},
        {1, 2, 3, 8, NOTE_QUARTER, ACC_SHARP},
        {1, 2, 4, 3, NOTE_HALF, ACC_NONE},
        {1, 2, 6, 3, NOTE_QUARTER, ACC_NONE},
        {1, 2, 7, 3, NOTE_QUARTER, ACC_NONE},
        {1, 2, 8, 0, NOTE_QUARTER, ACC_NONE},
        {1, 2, 9, 2, NOTE_QUARTER, ACC_NONE},
        {1, 2, 10, 5, NOTE_QUARTER, ACC_NONE},
        {1, 2, 11, 3, NOTE_HALF, ACC_NONE},
        {1, 2, 13, 6, NOTE_BEAM2_8TH, ACC_NONE},
        {1, 2, 15, 5, NOTE_QUARTER, ACC_NONE},
        {1, 2, 16, 5, NOTE_QUARTER, ACC_NONE},

        /* =========================
           PAGE 1, STAFF 3
           "streaming? And the rocket's red glare, the bombs bursting in air, gave"
           ========================= */
        {1, 3, 1, 7, NOTE_QUARTER, ACC_NONE},
        {1, 3, 2, 2, NOTE_BEAM2_8TH, ACC_NONE},
        {1, 3, 4, 2, NOTE_QUARTER, ACC_NONE},
        {1, 3, 5, 2, NOTE_QUARTER, ACC_NONE},
        {1, 3, 6, 1, NOTE_QUARTER, ACC_NONE},
        {1, 3, 7, 0, NOTE_QUARTER, ACC_NONE},
        {1, 3, 8, 5, NOTE_HALF, ACC_NONE},
        {1, 3, 10, 2, NOTE_QUARTER, ACC_NONE},
        {1, 3, 11, 3, NOTE_QUARTER, ACC_NONE},
        {1, 3, 12, 4, NOTE_QUARTER, ACC_NONE},
        {1, 3, 13, 3, NOTE_QUARTER, ACC_NONE},
        {1, 3, 14, 2, NOTE_QUARTER, ACC_NONE},
        {1, 3, 15, 1, NOTE_QUARTER, ACC_NONE},
        {1, 3, 16, 2, NOTE_HALF, ACC_NONE},

        /* =========================
           PAGE 2, STAFF 0
           "proof thro' the night that our flag was still there. O say, does that star-spangled"
           ========================= */
        {2, 0, 1, 2, NOTE_QUARTER, ACC_NONE},
        {2, 0, 2, 3, NOTE_QUARTER, ACC_NONE},
        {2, 0, 3, 5, NOTE_QUARTER, ACC_NONE},
        {2, 0, 4, 3, NOTE_HALF, ACC_NONE},
        {2, 0, 6, 7, NOTE_BEAM2_8TH, ACC_NONE},
        {2, 0, 8, 5, NOTE_QUARTER, ACC_NONE},
        {2, 0, 9, 7, NOTE_QUARTER, ACC_NONE},
        {2, 0, 10, 8, NOTE_QUARTER, ACC_SHARP},
        {2, 0, 11, 3, NOTE_HALF, ACC_NONE},
        {2, 0, 13, 3, NOTE_QUARTER, ACC_NONE},
        {2, 0, 14, 5, NOTE_QUARTER, ACC_NONE},
        {2, 0, 15, 5, NOTE_QUARTER, ACC_NONE},
        {2, 0, 16, 6, NOTE_BEAM2_8TH, ACC_NONE},

        /* =========================
           PAGE 2, STAFF 1
           "banner yet wave o'er the land of the free and the home of the brave?"
           ========================= */
        {2, 1, 1, 3, NOTE_QUARTER, ACC_NONE},
        {2, 1, 2, 2, NOTE_BEAM2_8TH, ACC_NONE},
        {2, 1, 4, 5, NOTE_BEAM2_8TH, ACC_NONE},
        {2, 1, 6, 6, NOTE_QUARTER, ACC_NONE},
        {2, 1, 7, 7, NOTE_QUARTER, ACC_NONE},
        {2, 1, 8, 3, NOTE_QUARTER, ACC_NONE},
        {2, 1, 9, 5, NOTE_BEAM2_8TH, ACC_NONE},
        {2, 1, 11, 2, NOTE_QUARTER, ACC_NONE},
        {2, 1, 12, 0, NOTE_HALF, ACC_NONE},
        {2, 1, 14, 6, NOTE_BEAM2_8TH, ACC_NONE},
        {2, 1, 16, 5, NOTE_QUARTER, ACC_NONE},
    };

    const int n = sizeof(song) / sizeof(song[0]);

    for (int i = 0; i < n; i++)
    {
        inject_note(song[i].col, song[i].staff, song[i].slot, song[i].type, song[i].page);
        if (num_notes > 0)
        {
            notes[num_notes - 1].accidental = song[i].acc;
        }
    }

    toolbar_state.instrument = TB_INST_PIANO_REVERB;
    toolbar_state.bpm = 96;
}

/* -------------------------------------------------------
   Song 4 – Ya Tab Tab wa Dalla (Egyptian folk)
   Scale (D maqam / Rast on D): D4 E4 F4 G4 A4 Bb4 C5 D5
   Slots:  10=D4, 9=E4, 8=F4, 7=G4, 6=A4, 5=Bb4(B4-flat), 4=C5, 3=D5
   Characteristic descending-ascending phrases in 4/4.
   ------------------------------------------------------- */
static void preload_ya_tab_tab(void)
{
    num_notes = 0;

    /*
     * Ya tab tab wa dalla: lively, ornamental melody.
     * Phrase A:  D5 C5 Bb4 A4  G4 F4 E4 D4  (descending scale)
     * Phrase B:  D4 E4 F4 G4   A4 Bb4 A4 G4 (ascending then resolve)
     * Phrase C:  G4 A4 Bb4 C5  D5 D5 C5 Bb4 (climax)
     * Phrase D:  A4 G4 F4 E4   D4 D4 D4 .   (final cadence)
     * Repeated across 4 staves.
     */

    /* ACC for each note: 0=none, 1=sharp, 2=flat */
    /* Bb4 = slot 5 + flat (ACC_FLAT=2) */

    int slots[] = {
        /* Staff 0 – Phrase A (descending) */
        3, 4, 5, 6, 7, 8, 9, 10, 3, 4, 5, 6, 7, 8, 9, 10,

        /* Staff 1 – Phrase B (ascending) */
        10, 9, 8, 7, 6, 5, 6, 7, 10, 9, 8, 7, 6, 5, 6, 7,

        /* Staff 2 – Phrase C (climax) */
        7, 6, 5, 4, 3, 3, 4, 5, 7, 6, 5, 4, 3, 3, 4, 5,

        /* Staff 3 – Phrase D (cadence) */
        6, 7, 8, 9, 10, 10, 10, 7, 6, 5, 6, 7, 3, 4, 10, 10};

    /* Accidentals: slot 5 needs ACC_FLAT for Bb */
    int accs[64];
    for (int i = 0; i < 64; i++)
    {
        accs[i] = ACC_NONE;
        if (slots[i] == 5)
            accs[i] = ACC_FLAT; /* Bb4 */
    }

    int types[] = {
        /* Staff 0 */
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_HALF, NOTE_REST,

        /* Staff 1 */
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_HALF, NOTE_REST,

        /* Staff 2 */
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_HALF, NOTE_REST,

        /* Staff 3 */
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_HALF, NOTE_REST, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER, NOTE_QUARTER,
        NOTE_QUARTER, NOTE_QUARTER, NOTE_WHOLE, NOTE_REST};

    for (int i = 0; i < 64; i++)
    {
        int staff = i / 16;
        int col = (i % 16) + 1;
        /* inject_note doesn't take accidental; set it after */
        inject_note(col, staff, slots[i], types[i], 1);
        /* Patch the accidental on the note we just added */
        if (accs[i] != ACC_NONE && num_notes > 0)
        {
            notes[num_notes - 1].accidental = accs[i];
        }
    }
    toolbar_state.instrument = TB_INST_PIANO_REVERB;
    toolbar_state.bpm = 132;
}

static void preload_song(void)
{
    switch (g_song_selection)
    {
    case 1:
        preload_ode_to_joy();
        break;
    case 2:
        preload_o_canada();
        break;
    case 3:
        preload_star_spangled();
        break;
    case 4:
        preload_ya_tab_tab();
        break;
    default:
        preload_ode_to_joy();
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   Main Loop
   ═══════════════════════════════════════════════════════════════════════ */
int main(void)
{
    volatile int *pixel_ctrl = (volatile int *)PIXEL_BUF_CTRL;
    volatile int *ps2 = (volatile int *)PS2_BASE;

    pixel_buffer_start = *pixel_ctrl;
    *(pixel_ctrl + 1) = pixel_buffer_start;

    keyboard_init();

restart_main_menu:
    /* Reset all sequencer state for a clean restart */
    num_notes = 0;
    cur_page = 1;
    max_pages = 1;

    draw_start_screen();
    {
        int got_break_start = 0;
        while (g_start_screen_active)
        {
            int raw = ps2_read_byte(ps2);
            if (raw < 0)
                continue;
            unsigned char b = (unsigned char)raw;
            if (b == 0xE0)
                continue;
            if (b == KEY_BREAK)
            {
                got_break_start = 1;
                continue;
            }
            if (got_break_start)
            {
                got_break_start = 0;
                continue;
            }

            if (b == KEY_W)
            {
                g_start_selection = 1;
                update_start_selection(g_start_selection);
            }
            if (b == KEY_S)
            {
                g_start_selection = 2;
                update_start_selection(g_start_selection);
            }
            if (b == KEY_1)
            {
                g_start_selection = 1;
                update_start_selection(g_start_selection);
                g_start_screen_active = 0;
            }
            if (b == KEY_2)
            {
                g_start_selection = 2;
                update_start_selection(g_start_selection);
                g_start_screen_active = 0;
            }
            if (b == KEY_SPACE)
            {
                g_start_screen_active = 0;
            }
        }
    }

    /* If user chose Preload Song, show the song-select submenu */
    if (g_start_selection == 2)
    {
        draw_song_select_screen();
        int got_break_song = 0;
        while (g_start_screen_active)
        {
            int raw = ps2_read_byte(ps2);
            if (raw < 0)
                continue;
            unsigned char b = (unsigned char)raw;
            if (b == 0xE0)
                continue;
            if (b == KEY_BREAK)
            {
                got_break_song = 1;
                continue;
            }
            if (got_break_song)
            {
                got_break_song = 0;
                continue;
            }

            if (b == KEY_W)
            {
                if (g_song_selection > 1)
                    g_song_selection--;
                update_song_selection(g_song_selection);
            }
            if (b == KEY_S)
            {
                if (g_song_selection < 4)
                    g_song_selection++;
                update_song_selection(g_song_selection);
            }
            if (b == KEY_1)
            {
                g_song_selection = 1;
                update_song_selection(g_song_selection);
                g_start_screen_active = 0;
            }
            if (b == KEY_2)
            {
                g_song_selection = 2;
                update_song_selection(g_song_selection);
                g_start_screen_active = 0;
            }
            if (b == KEY_3)
            {
                g_song_selection = 3;
                update_song_selection(g_song_selection);
                g_start_screen_active = 0;
            }
            if (b == KEY_4)
            {
                g_song_selection = 4;
                update_song_selection(g_song_selection);
                g_start_screen_active = 0;
            }
            if (b == KEY_SPACE)
            {
                g_start_screen_active = 0;
            }
            /* [5] BACK — return to main menu */
            if (b == KEY_5)
            {
                g_start_selection = 1;
                goto restart_main_menu;
            }
        }
    }

    /* Trigger the preload if the user selected option 2 */
    if (g_start_selection == 2)
    {
        preload_song();
    }

    build_and_draw_background();
    safe_draw_toolbar(cur_note_type);
    safe_draw_row2(cur_accidental);
    update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages);

    int cur_col = 1;
    int cur_row = 0;
    int cur_staff = 0;
    int cur_slot = 0;
    int cur_x = col_to_x(cur_col);
    int cur_y = row_to_y(cur_row, &cur_staff, &cur_slot);

    redraw_all_notes();
    draw_cursor_cell(cur_x, cur_y);
    pixel_buffer_start = *pixel_ctrl;

    int got_break = 0;
    int got_extended = 0;
    int menu_open = 0;
    int menu_state = MENU_STATE_MAIN; /* 0=main options, 1=instrument sub-menu */

    while (1)
    {
        int raw = ps2_read_byte(ps2);
        if (raw < 0)
            continue;

        unsigned char b = (unsigned char)raw;

        if (b == 0xE0)
        {
            got_extended = 1;
            continue;
        }
        if (b == KEY_BREAK)
        {
            got_break = 1;
            continue;
        }
        if (got_break)
        {
            got_break = 0;
            got_extended = 0;
            continue;
        }

        if (b == KEY_M)
        {
            if (menu_open)
            {
                menu_open = 0;
                menu_state = MENU_STATE_MAIN;
                /* Erase up to the taller instrument submenu bottom (MENU_Y0+175+4=239) */
                int erase_y1 = (MENU_Y0 + 179) > (MENU_Y1 + 4) ? (MENU_Y0 + 179) : (MENU_Y1 + 4);
                for (int my = MENU_Y0; my <= erase_y1; my++)
                    for (int mx = MENU_X0; mx <= MENU_X1 + 4; mx++)
                        plot_pixel(mx, my, bg[my][mx]);
                redraw_all_notes();
                draw_cursor_cell(cur_x, cur_y);
            }
            else
            {
                menu_open = 1;
                menu_state = MENU_STATE_MAIN;
                g_drawing_ui = 1;
                draw_options_menu();
                g_drawing_ui = 0;
            }
            continue;
        }

        if (b == KEY_N)
        {
            clear_all_notes_and_reload(cur_note_type, cur_accidental, cur_x, cur_y);
            menu_open = 0;
            menu_state = MENU_STATE_MAIN;
            continue;
        }

        if (menu_open)
        {
            if (menu_state == MENU_STATE_MAIN)
            {
                if (b == KEY_1)
                {
                    /* Go to instrument sub-menu */
                    menu_state = MENU_STATE_INSTRUMENT;
                    g_drawing_ui = 1;
                    draw_options_menu_instrument();
                    g_drawing_ui = 0;
                    continue;
                }
                if (b == KEY_2)
                {
                    /* Return to main menu */
                    menu_open = 0;
                    menu_state = MENU_STATE_MAIN;
                    goto restart_main_menu;
                }
            }
            else if (menu_state == MENU_STATE_INSTRUMENT)
            {
                if (b == KEY_1)
                {
                    toolbar_set_instrument(TB_INST_BEEP);
                    continue;
                }
                if (b == KEY_2)
                {
                    toolbar_set_instrument(TB_INST_PIANO);
                    continue;
                }
                if (b == KEY_3)
                {
                    toolbar_set_instrument(TB_INST_XYLOPHONE);
                    continue;
                }
                if (b == KEY_4)
                {
                    toolbar_set_instrument(TB_INST_TUBA);
                    continue;
                }
            }
            continue;
        }

        int is_note_key = (b == KEY_1 || b == KEY_2 || b == KEY_3 || b == KEY_4 ||
                           b == KEY_5 || b == KEY_6 || b == KEY_7 || b == KEY_8);
        if (is_note_key)
        {
            if (b == KEY_1)
                cur_note_type = NOTE_WHOLE;
            if (b == KEY_2)
                cur_note_type = NOTE_HALF;
            if (b == KEY_3)
                cur_note_type = NOTE_QUARTER;
            if (b == KEY_4)
                cur_note_type = NOTE_BEAM2_8TH;
            if (b == KEY_5)
                cur_note_type = NOTE_BEAM4_16TH;
            if (b == KEY_6)
                cur_note_type = NOTE_BEAM2_16TH;
            if (b == KEY_7)
                cur_note_type = NOTE_SINGLE16TH;
            if (b == KEY_8)
            {
                cur_note_type = NOTE_REST;
                cur_accidental = ACC_NONE;
                safe_draw_row2(0);
            }

            safe_set_note_type(cur_note_type);
            update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages);
            continue;
        }

        /* ── PAGE LOOPING AUDIO LOGIC ── */
        if (b == KEY_Q || b == KEY_R)
        {
            int start_p = (b == KEY_R) ? 1 : cur_page;

            while (1)
            {
                if (cur_page != start_p)
                {
                    switch_page(start_p, cur_x, cur_y);
                }

                toolbar_state.playback = TB_STATE_PLAYING;
                safe_draw_toolbar(cur_note_type);

                for (int p = start_p; p <= max_pages; p++)
                {
                    if (cur_page != p)
                    {
                        switch_page(p, cur_x, cur_y);
                    }

                    seq_user_stopped = 0;
                    seq_user_restarted = 0;
                    seq_is_playing = 1;
                    seq_is_paused = 0;

                    play_sequence();

                    if (seq_user_restarted || seq_user_stopped)
                    {
                        break;
                    }
                }

                if (seq_user_restarted)
                {
                    start_p = 1;
                    continue; /* Restart the while(1) loop from page 1 */
                }

                /* If we get here, playback finished naturally or the user pressed T (Stop) */
                break;
            }

            seq_is_playing = 0;
            toolbar_state.playback = TB_STATE_STOPPED;
            safe_draw_toolbar(cur_note_type);
            redraw_all_notes();
            draw_cursor_cell(cur_x, cur_y);
            continue;
        }

        if (b == KEY_E)
        {
            continue;
        }
        if (b == KEY_T)
        {
            continue;
        }

        if (b == KEY_COMMA)
        {
            switch_page(cur_page - 1, cur_x, cur_y);
            continue;
        }
        if (b == KEY_PERIOD)
        {
            switch_page(cur_page + 1, cur_x, cur_y);
            continue;
        }

        if (b == KEY_Z)
        {
            cur_accidental = ACC_NONE;
            safe_draw_row2(cur_accidental);
            update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages);
            continue;
        }
        if (b == KEY_X)
        {
            if (cur_note_type != NOTE_REST)
                cur_accidental = (cur_accidental == ACC_SHARP) ? ACC_NONE : ACC_SHARP;
            safe_draw_row2(cur_accidental);
            update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages);
            continue;
        }
        if (b == KEY_C)
        {
            if (cur_note_type != NOTE_REST)
                cur_accidental = (cur_accidental == ACC_FLAT) ? ACC_NONE : ACC_FLAT;
            safe_draw_row2(cur_accidental);
            update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages);
            continue;
        }
        if (b == KEY_V)
        {
            if (cur_note_type != NOTE_REST)
                cur_accidental = (cur_accidental == ACC_NATURAL) ? ACC_NONE : ACC_NATURAL;
            safe_draw_row2(cur_accidental);
            update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages);
            continue;
        }

        if (got_extended && (b == KEY_UP || b == KEY_DOWN))
        {
            int delta = (b == KEY_UP) ? -1 : 1;
            if (move_note_head(cur_col, cur_staff, cur_slot, delta))
            {
                cur_row += delta;
                cur_y = row_to_y(cur_row, &cur_staff, &cur_slot);
                draw_cursor_cell(cur_x, cur_y);
            }
            got_extended = 0;
            continue;
        }
        got_extended = 0;

        if (b == KEY_SPACE)
        {
            if (cur_note_type == NOTE_REST)
            {
                int rest_slot = SLOTS_PER_STAFF / 2;
                int rest_row = cur_staff * SLOTS_PER_STAFF + rest_slot;
                int rest_y = row_to_y(rest_row, 0, 0);
                place_note(cur_col, cur_staff, rest_slot, cur_x, rest_y, cur_note_type);
                {
                    int ex, ey;
                    for (ey = rest_y - CELL_H / 2; ey <= rest_y + CELL_H / 2; ey++)
                        for (ex = cur_x - CELL_W / 2; ex <= cur_x + CELL_W / 2; ex++)
                            if (ex >= 0 && ex < FB_WIDTH && ey >= 0 && ey < FB_HEIGHT)
                                plot_pixel(ex, ey, bg[ey][ex]);
                }
                redraw_all_notes();
                draw_cursor_cell(cur_x, cur_y);
            }
            else
            {
                place_note(cur_col, cur_staff, cur_slot, cur_x, cur_y, cur_note_type);
            }
            continue;
        }

        if (b == KEY_DELETE)
        {
            delete_note(cur_col, cur_staff, cur_slot, cur_x, cur_y);
            continue;
        }

        if (b == KEY_MINUS)
        {
            toolbar_set_bpm(toolbar_state.bpm - 5);
            continue;
        }
        if (b == KEY_EQUALS)
        {
            toolbar_set_bpm(toolbar_state.bpm + 5);
            continue;
        }

        if (b == KEY_W || b == KEY_A || b == KEY_S || b == KEY_D)
        {
            int new_col = cur_col;
            int new_row = cur_row;

            if (b == KEY_W && cur_row > 0)
                new_row--;
            if (b == KEY_S && cur_row < TOTAL_ROWS - 1)
                new_row++;
            if (b == KEY_A && cur_col > 1)
                new_col--;
            if (b == KEY_D && cur_col < TOTAL_COLS - 1)
                new_col++;

            if (new_col != cur_col || new_row != cur_row)
            {
                erase_cursor_cell(cur_x, cur_y);
                cur_col = new_col;
                cur_row = new_row;
                cur_x = col_to_x(cur_col);
                cur_y = row_to_y(cur_row, &cur_staff, &cur_slot);
                redraw_all_notes();
                draw_cursor_cell(cur_x, cur_y);
            }
        }

        pixel_buffer_start = *pixel_ctrl;
    }

    return 0;
}