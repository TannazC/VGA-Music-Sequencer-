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

#define KEY_SPACE  0x29
#define KEY_DELETE 0x66
#define KEY_Q 0x15
#define KEY_E 0x24
#define KEY_T 0x2C
#define KEY_R 0x2D
#define KEY_M 0x3A
#define KEY_N 0x31

/* Page Structure Keys */
#define KEY_K 0x42 
#define KEY_L 0x4B 

#define KEY_BREAK 0xF0

/* Arrow Keys (Extended 0xE0 prefix) */
#define KEY_UP    0x75
#define KEY_DOWN  0x72
#define KEY_LEFT  0x6B
#define KEY_RIGHT 0x74

#define KEY_MINUS  0x4E
#define KEY_EQUALS 0x55

/* Global State */
int cur_page = 1;
int max_pages = 4;
volatile int g_drawing_ui = 0;

/* Row 2 Highlight Masks */
int active_page_nav = 0;    
int active_page_struct = 0; 

/* Audio transport flags */
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

#ifdef TOTAL_COLS
#undef TOTAL_COLS
#endif
#define TOTAL_COLS 17

#define SLOTS_PER_STAFF ((LINES_PER_STAFF - 1) * 2 + 3)
#define TOTAL_ROWS (NUM_STAVES * SLOTS_PER_STAFF)

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
    if (col < 0) col = 0;
    if (col >= TOTAL_COLS) col = TOTAL_COLS - 1;
    return STAFF_X0 + col * STEP_W + STEP_W / 2;
}

static int row_to_y(int row, int *staff_out, int *slot_out)
{
    int s, slot;
    if (row < 0) row = 0;
    if (row >= TOTAL_ROWS) row = TOTAL_ROWS - 1;
    s = row / SLOTS_PER_STAFF;
    slot = row % SLOTS_PER_STAFF;
    if (staff_out) *staff_out = s;
    if (slot_out)  *slot_out  = slot;
    return staff_top[s] + (slot - 1) * (STAFF_SPACING / 2);
}

/* ═══════════════════════════════════════════════════════════════════════
   Safe UI Wrappers & Pixels
   ═══════════════════════════════════════════════════════════════════════ */
void plot_pixel(int x, int y, short int c)
{
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;
    if (y < UI_SAFE_ZONE && !g_start_screen_active && !g_drawing_ui) return;
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
    draw_toolbar_row2(acc, active_page_nav, active_page_struct);
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
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;

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
                    if (OPEN_OVAL[by][bx]) { plot_pixel(x, y, BLACK); return; }
            }
            else
            {
                int bx = ddx + OVAL_W / 2;
                int by = ddy + OVAL_H / 2;
                if (bx >= 0 && bx < OVAL_W && by >= 0 && by < OVAL_H)
                    if (FILLED_OVAL[by][bx]) { plot_pixel(x, y, BLACK); return; }
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
   Note glyph primitives (MUST precede draw_note_instance)
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

    if (accidental == ACC_NONE) return;

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

/* ═══════════════════════════════════════════════════════════════════════
   Complex Note Rendering
   ═══════════════════════════════════════════════════════════════════════ */
static void draw_note_instance(const Note *n, short int c)
{
    int i;
    if (n->num_heads <= 0) return;
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
                if (top_i < beam_y) beam_y = top_i;
            }
            for (i = 0; i < n->num_heads; i++)
            {
                filled_oval(n->head_x[i], n->head_y[i], c);
                draw_stem_to_top(n->head_x[i], n->head_y[i], beam_y, c);
            }
            beam_bar(n->head_x[0] + STEM_X_OFF,
                     n->head_x[n->num_heads - 1] + STEM_X_OFF,
                     beam_y, BEAM_THICK, c);
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
                if (top_i < beam_y) beam_y = top_i;
            }
            for (i = 0; i < n->num_heads; i++)
            {
                filled_oval(n->head_x[i], n->head_y[i], c);
                draw_stem_to_top(n->head_x[i], n->head_y[i], beam_y + BEAM_THICK + 1, c);
            }
            beam_bar(n->head_x[0] + STEM_X_OFF, n->head_x[n->num_heads - 1] + STEM_X_OFF, beam_y, BEAM_THICK, c);
            beam_bar(n->head_x[0] + STEM_X_OFF, n->head_x[n->num_heads - 1] + STEM_X_OFF, beam_y + BEAM_THICK + 1, BEAM_THICK, c);
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
                if (top_i < beam_y) beam_y = top_i;
            }
            for (i = 0; i < n->num_heads; i++)
            {
                filled_oval(n->head_x[i], n->head_y[i], c);
                draw_stem_to_top(n->head_x[i], n->head_y[i], beam_y + BEAM_THICK + 1, c);
            }
            beam_bar(n->head_x[0] + STEM_X_OFF, n->head_x[n->num_heads - 1] + STEM_X_OFF, beam_y, BEAM_THICK, c);
            beam_bar(n->head_x[0] + STEM_X_OFF, n->head_x[n->num_heads - 1] + STEM_X_OFF, beam_y + BEAM_THICK + 1, BEAM_THICK, c);
        }
        break;
    case NOTE_SINGLE16TH:
        filled_oval(n->head_x[0], n->head_y[0], c);
        draw_stem_segment(n->head_x[0], n->head_y[0], c);
        double_flag(n->head_x[0], n->head_y[0], c);
        break;
    case NOTE_REST:
    {
        int cx = n->head_x[0], cy = n->head_y[0];
        plot_pixel(cx+(-1), cy+(-9), c); plot_pixel(cx+(0), cy+(-8), c);
        plot_pixel(cx+(0), cy+(-7), c);  plot_pixel(cx+(1), cy+(-7), c);
        plot_pixel(cx+(0), cy+(-6), c);  plot_pixel(cx+(1), cy+(-6), c); plot_pixel(cx+(2), cy+(-6), c);
        plot_pixel(cx+(0), cy+(-5), c);  plot_pixel(cx+(1), cy+(-5), c); plot_pixel(cx+(2), cy+(-5), c);
        plot_pixel(cx+(-1), cy+(-4), c); plot_pixel(cx+(0), cy+(-4), c); plot_pixel(cx+(1), cy+(-4), c); plot_pixel(cx+(2), cy+(-4), c);
        plot_pixel(cx+(-2), cy+(-3), c); plot_pixel(cx+(-1), cy+(-3), c); plot_pixel(cx+(0), cy+(-3), c); plot_pixel(cx+(1), cy+(-3), c);
        plot_pixel(cx+(-2), cy+(-2), c); plot_pixel(cx+(-1), cy+(-2), c); plot_pixel(cx+(0), cy+(-2), c); plot_pixel(cx+(1), cy+(-2), c);
        plot_pixel(cx+(-2), cy+(-1), c); plot_pixel(cx+(-1), cy+(-1), c); plot_pixel(cx+(0), cy+(-1), c);
        plot_pixel(cx+(-2), cy+(0), c);  plot_pixel(cx+(-1), cy+(0), c);  plot_pixel(cx+(0), cy+(0), c);
        plot_pixel(cx+(-1), cy+(1), c);  plot_pixel(cx+(0), cy+(1), c);
        plot_pixel(cx+(0), cy+(2), c);   plot_pixel(cx+(1), cy+(2), c);
        plot_pixel(cx+(-2), cy+(3), c);  plot_pixel(cx+(-1), cy+(3), c);  plot_pixel(cx+(0), cy+(3), c);  plot_pixel(cx+(1), cy+(3), c);  plot_pixel(cx+(2), cy+(3), c);
        plot_pixel(cx+(-3), cy+(4), c);  plot_pixel(cx+(-2), cy+(4), c);  plot_pixel(cx+(-1), cy+(4), c);  plot_pixel(cx+(0), cy+(4), c);  plot_pixel(cx+(1), cy+(4), c);  plot_pixel(cx+(2), cy+(4), c);
        plot_pixel(cx+(-3), cy+(5), c);  plot_pixel(cx+(-2), cy+(5), c);  plot_pixel(cx+(-1), cy+(5), c);
        plot_pixel(cx+(-3), cy+(6), c);  plot_pixel(cx+(-2), cy+(6), c);  plot_pixel(cx+(-1), cy+(6), c);
        plot_pixel(cx+(-2), cy+(7), c);  plot_pixel(cx+(-1), cy+(7), c);
        plot_pixel(cx+(-1), cy+(8), c);
        break;
    }
    default: break;
    }
}

static void erase_note_instance(const Note *n)
{
    int x, y, i, min_x, max_x, min_y, max_y;
    if (n->num_heads <= 0) return;
    if (n->note_type == NOTE_REST) {
        int cx = n->head_x[0], cy = n->head_y[0];
        min_x = cx - 6; max_x = cx + 5; min_y = cy - 11; max_y = cy + 11;
        for (y = min_y; y <= max_y; y++)
            for (x = min_x; x <= max_x; x++)
                if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT) plot_pixel(x, y, bg[y][x]);
        return;
    }
    min_x = max_x = n->head_x[0]; min_y = max_y = n->head_y[0];
    for (i = 1; i < n->num_heads; i++) {
        if (n->head_x[i] < min_x) min_x = n->head_x[i];
        if (n->head_x[i] > max_x) max_x = n->head_x[i];
        if (n->head_y[i] < min_y) min_y = n->head_y[i];
        if (n->head_y[i] > max_y) max_y = n->head_y[i];
    }
    min_x -= STEP_W; max_x += OVAL_W/2 + STEM_X_OFF + FLAG_LEN + 4;
    min_y -= STEM_HEIGHT + 8; max_y += OVAL_H/2 + 4;
    for (y = min_y; y <= max_y; y++)
        for (x = min_x; x <= max_x; x++)
            if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT) plot_pixel(x, y, bg[y][x]);
}

static void redraw_all_notes(void)
{
    for (int i = 0; i < num_notes; i++) {
        if (notes[i].page == cur_page) {
            draw_note_instance(&notes[i], COLOR_BLACK);
        }
    }
}

void draw_mini_note_glyph(int cx, int cy, int nt, int accidental, short int c)
{
    Note mini; int nh = note_num_heads[nt];
    mini.note_type = nt; mini.accidental = accidental; mini.num_heads = nh;
    mini.head_x[0] = cx; mini.head_y[0] = cy;
    for (int i = 1; i < nh; i++) { mini.head_x[i] = cx + i * 9; mini.head_y[i] = cy; }
    draw_note_instance(&mini, c);
}

void update_note_indicator(int nt, int accidental, int cur_p, int max_p)
{
    for (int y = 210; y < FB_HEIGHT; y++) for (int x = 0; x < FB_WIDTH; x++) plot_pixel(x, y, bg[y][x]);
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
static void switch_page(int new_page, int cur_x, int cur_y) {
    if (new_page < 1 || new_page > max_pages) return;
    cur_page = new_page;
    build_and_draw_background();
    safe_draw_toolbar(cur_note_type);
    safe_draw_row2(cur_accidental);
    update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages);
    redraw_all_notes();
    draw_cursor_cell(cur_x, cur_y);
}

static void clear_all_notes_and_reload(int cur_note_type, int cur_accidental, int cur_x, int cur_y)
{
    num_notes = 0; build_and_draw_background(); safe_draw_toolbar(cur_note_type);
    safe_draw_row2(cur_accidental);
    update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages);
    draw_cursor_cell(cur_x, cur_y);
}

static void fill_note_heads(Note *n, int col, int staff, int slot, int sx, int sy, int nt)
{
    int nh = note_num_heads[nt]; n->num_heads = nh;
    for (int i = 0; i < nh; i++) {
        n->head_step[i] = col + i; n->head_pitch_slot[i] = slot;
        n->head_x[i] = sx + i * STEP_W; n->head_y[i] = sy;
    }
    for (int i = nh; i < MAX_HEADS; i++) {
        n->head_step[i] = n->head_pitch_slot[i] = 0; n->head_x[i] = n->head_y[i] = 0;
    }
}

static int find_note_head_at(int col, int staff, int slot, int *note_idx, int *head_idx)
{
    for (int i = 0; i < num_notes; i++) {
        if (notes[i].staff != staff) continue;
        for (int h = 0; h < notes[i].num_heads; h++) {
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
    if (!find_note_head_at(cur_col, cur_staff, cur_slot, &note_idx, &head_idx)) return 0;
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

static int col_is_occupied(int col, int staff)
{
    for (int i = 0; i < num_notes; i++) {
        if (notes[i].page != cur_page || notes[i].staff != staff) continue;
        for (int h = 0; h < notes[i].num_heads; h++) if (notes[i].head_step[h] == col) return 1;
    }
    return 0;
}

static void place_note(int cur_col, int cur_staff, int cur_slot, int cur_x, int cur_y, int nt)
{
    int nh = note_num_heads[nt];
    if (cur_col < 1 || cur_col + nh - 1 >= TOTAL_COLS) return;
    for (int h = 0; h < nh; h++) if (col_is_occupied(cur_col + h, cur_staff)) return;
    if (num_notes >= MAX_NOTES) return;

    Note *n = &notes[num_notes];
    n->step = cur_col; n->staff = cur_staff; n->pitch_slot = cur_slot; n->note_type = nt;
    n->duration_64 = note_duration_64[nt]; n->accidental = (nt == NOTE_REST) ? ACC_NONE : cur_accidental;
    n->screen_x = cur_x; n->screen_y = cur_y; n->page = cur_page;
    fill_note_heads(n, cur_col, cur_staff, cur_slot, cur_x, cur_y, nt);
    num_notes++;
    draw_note_instance(&notes[num_notes - 1], COLOR_BLACK);
    draw_cursor_cell(cur_x, cur_y);
}

static void delete_note(int cur_col, int cur_staff, int cur_slot, int cur_x, int cur_y)
{
    for (int i = 0; i < num_notes; i++) {
        if (notes[i].page != cur_page || notes[i].staff != cur_staff) continue;
        for (int h = 0; h < notes[i].num_heads; h++) {
            if (notes[i].head_step[h] == cur_col && notes[i].head_pitch_slot[h] == cur_slot) {
                erase_note_instance(&notes[i]);
                
                notes[i] = notes[num_notes - 1]; num_notes--;
                redraw_all_notes(); draw_cursor_cell(cur_x, cur_y);
                return;
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   PS/2 Helpers
   ═══════════════════════════════════════════════════════════════════════ */
static int ps2_read_byte(volatile int *ps2)
{
    int v = *ps2;
    if (v & PS2_RVALID) return v & 0xFF;
    return -1;
}

static void ps2_flush(volatile int *ps2)
{
    for (int i = 0; i < 512; i++) { if (!(*ps2 & PS2_RVALID)) break; (void)(*ps2); }
}

static void keyboard_init(void)
{
    volatile int *ps2 = (volatile int *)PS2_BASE;
    int timeout; ps2_flush(ps2); *ps2 = 0xFF; timeout = 3000000;
    while (timeout-- > 0) { int v = *ps2; if ((v & PS2_RVALID) && (v & 0xFF) == 0xAA) break; }
    ps2_flush(ps2); *ps2 = 0xF4; timeout = 2000000;
    while (timeout-- > 0) { int v = *ps2; if ((v & PS2_RVALID) && (v & 0xFF) == 0xFA) break; }
    ps2_flush(ps2);
}

/* =======================================================================
   Preload Song Logic
   ======================================================================= */
static void inject_note(int col, int staff, int slot, int nt, int acc, int page) {
    if (num_notes >= MAX_NOTES) return;
    Note *n = &notes[num_notes];
    n->step = col; n->staff = staff; n->pitch_slot = slot; n->note_type = nt;
    n->duration_64 = note_duration_64[nt]; n->accidental = acc; n->page = page;
    int sx = col_to_x(col); int sy = row_to_y(staff * SLOTS_PER_STAFF + slot, NULL, NULL);
    n->screen_x = sx; n->screen_y = sy;
    fill_note_heads(n, col, staff, slot, sx, sy, nt);
    num_notes++;
}

static void preload_ode_to_joy(void) {
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
        int is_rest = (slots[i] == -1);
        int draw_slot = is_rest ? 4 : slots[i]; 
        int nt = is_rest ? NOTE_REST : NOTE_QUARTER;
        inject_note((i%16)+1, (i%64)/16, draw_slot, nt, 0, (i/64)+1);
    }
    max_pages = 2; toolbar_state.bpm = 180;
}

static void preload_helwa_ya_baladi(void) {
    num_notes = 0;
    int slots[64] = { 
        0,0,3,3, 4,5,6,7, 7,6,5,4, 5,6,7,-1, 
        4,4,0,0, 1,2,3,4, 4,3,2,1, 2,3,4,-1, 
        0,0,3,3, 4,5,6,7, 7,6,5,4, 5,6,7,-1, 
        4,4,0,0, 1,2,3,4, 4,3,2,1, 2,3,4,-1 
    };
    int accs[64]  = { 
        0,0,0,0, 0,2,0,0, 0,0,2,0, 2,0,0,0, 
        0,0,0,0, 0,2,0,0, 0,0,2,0, 2,0,0,0, 
        0,0,0,0, 0,2,0,0, 0,0,2,0, 2,0,0,0, 
        0,0,0,0, 0,2,0,0, 0,0,2,0, 2,0,0,0 
    };
    for (int i = 0; i < 64; i++) {
        int is_rest = (slots[i] == -1);
        int draw_slot = is_rest ? 4 : slots[i]; 
        int nt = is_rest ? NOTE_REST : NOTE_QUARTER;
        inject_note((i%16)+1, i/16, draw_slot, nt, accs[i], 1);
    }
    max_pages = 1; toolbar_state.bpm = 140;
}

static void preload_fur_elise(void) {
    num_notes = 0;
    int slots[64] = { 
        2, 2, 2, 2,   2, 5, 3, 4,   6, -1, 10, 9,   6, 5, -1, 9,
        7, 5, 4, -1,  9, 2, 2, 2,   2, 2, 5, 3,   4, 6, -1, 10,
        9, 6, 5, -1,  9, 4, 5, 6,  -1,-1,-1,-1,  -1,-1,-1,-1,
       -1,-1,-1,-1,  -1,-1,-1,-1,  -1,-1,-1,-1,  -1,-1,-1,-1
    };
    int accs[64] = { 
        0, 1, 0, 1,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
        1, 0, 0, 0,   0, 0, 1, 0,   1, 0, 0, 0,   0, 0, 0, 0,
        0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
        0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0
    };

    for (int i = 0; i < 64; i++) {
        if (slots[i] == -1) continue;
        inject_note((i%16)+1, i/16, slots[i], NOTE_QUARTER, accs[i], 1);
    }
    max_pages = 1; 
    toolbar_state.bpm = 240; 
}

static void preload_do_re_mi(void) {
    num_notes = 0;
    int slots[64] = { 
        4,3,2,4, 2,4,2,-1, 3,1,2,3, 1,3,1,-1, 
        2,0,1,2, 0,2,0,-1, 1,6,0,1, 6,1,6,-1, 
        0,4,3,2, 1,0,-1,-1, 6,5,4,-1, -1,-1,-1,-1, 
        4,3,2,1, 0,6,5,4, 4,-1,-1,-1, -1,-1,-1,-1 
    };
    for (int i = 0; i < 64; i++) {
        int is_rest = (slots[i] == -1);
        int draw_slot = is_rest ? 4 : slots[i];
        int nt = is_rest ? NOTE_REST : NOTE_QUARTER;
        inject_note((i%16)+1, i/16, draw_slot, nt, 0, 1);
    }
    max_pages = 1; toolbar_state.bpm = 200;
}

/* ═══════════════════════════════════════════════════════════════════════
   Main Loop
   ═══════════════════════════════════════════════════════════════════════ */
int main(void)
{
    volatile int *pixel_ctrl = (volatile int *)PIXEL_BUF_CTRL;
    volatile int *ps2 = (volatile int *)PS2_BASE;
    pixel_buffer_start = *pixel_ctrl; *(pixel_ctrl + 1) = pixel_buffer_start;
    keyboard_init();

restart_main_menu:
    num_notes = 0; cur_page = 1; max_pages = 1; g_start_screen_active = 1;
    
    int got_break = 0, got_extended = 0, menu_open = 0, menu_state = MENU_STATE_MAIN;
    active_page_nav = 0; active_page_struct = 0;

    draw_start_screen();
    
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

    if (g_start_selection == 2) {
        draw_song_select_screen(); g_start_screen_active = 1;
        got_break_start = 0;
        g_song_selection = 1; 
        
        while (g_start_screen_active) {
            int raw = ps2_read_byte(ps2); if (raw < 0) continue;
            unsigned char b = (unsigned char)raw;
            
            if (b == 0xE0) continue;
            if (b == KEY_BREAK) { got_break_start = 1; continue; }
            if (got_break_start) { got_break_start = 0; continue; }
            
            if (b == KEY_W) { 
                if (g_song_selection > 1) g_song_selection--; 
                update_song_selection(g_song_selection); 
            }
            if (b == KEY_S) { 
                if (g_song_selection < 5) g_song_selection++; 
                update_song_selection(g_song_selection); 
            }

            if (b == KEY_1) { g_song_selection = 1; g_start_screen_active = 0; }
            if (b == KEY_2) { g_song_selection = 2; g_start_screen_active = 0; }
            if (b == KEY_3) { g_song_selection = 3; g_start_screen_active = 0; }
            if (b == KEY_4) { g_song_selection = 4; g_start_screen_active = 0; }
            if (b == KEY_5) { g_song_selection = 5; g_start_screen_active = 0; }
            
            if (b == KEY_SPACE) g_start_screen_active = 0;
        }
        
        if (g_song_selection == 1) { preload_ode_to_joy(); }
        else if (g_song_selection == 2) { preload_helwa_ya_baladi(); }
        else if (g_song_selection == 3) { preload_fur_elise(); }
        else if (g_song_selection == 4) { preload_do_re_mi(); }
        else if (g_song_selection == 5) { goto restart_main_menu; }
    }

    build_and_draw_background(); safe_draw_toolbar(cur_note_type);
    safe_draw_row2(cur_accidental); update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages);

    int cur_col = 1, cur_row = 0, cur_staff = 0, cur_slot = 0;
    int cur_x = col_to_x(cur_col), cur_y = row_to_y(cur_row, &cur_staff, &cur_slot);
    redraw_all_notes(); draw_cursor_cell(cur_x, cur_y);

    while (1) {
        int raw = ps2_read_byte(ps2); if (raw < 0) continue;
        unsigned char b = (unsigned char)raw;

        if (b == 0xE0) { got_extended = 1; continue; }
        if (b == KEY_BREAK) { got_break = 1; continue; }
        
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

        /* 1. Toggle Menu Logic */
        if (b == KEY_M) {
            if (menu_open) {
                menu_open = 0;
                menu_state = MENU_STATE_MAIN;
                build_and_draw_background(); safe_draw_toolbar(cur_note_type);
                safe_draw_row2(cur_accidental); update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages);
                redraw_all_notes(); draw_cursor_cell(cur_x, cur_y);
            } else {
                menu_open = 1;
                menu_state = MENU_STATE_MAIN;
                g_drawing_ui = 1; draw_options_menu(); g_drawing_ui = 0;
            }
            continue;
        }

        if (b == KEY_N) { clear_all_notes_and_reload(cur_note_type, cur_accidental, cur_x, cur_y); continue; }

        /* 2. Two-Tier Menu State Machine */
        if (menu_open) {
            if (menu_state == MENU_STATE_MAIN) {
                if (b == KEY_1) {
                    menu_state = MENU_STATE_INSTRUMENT;
                    g_drawing_ui = 1; draw_options_menu_instrument(); g_drawing_ui = 0;
                    continue;
                }
                if (b == KEY_2) {
                    menu_open = 0;
                    menu_state = MENU_STATE_MAIN;
                    goto restart_main_menu;
                }
            } else if (menu_state == MENU_STATE_INSTRUMENT) {
                if (b == KEY_1) { toolbar_set_instrument(TB_INST_BEEP); continue; }
                if (b == KEY_2) { toolbar_set_instrument(TB_INST_PIANO); continue; }
                if (b == KEY_3) { toolbar_set_instrument(TB_INST_PIANO_REVERB); continue; }
                if (b == KEY_5) {
                    menu_state = MENU_STATE_MAIN;
                    g_drawing_ui = 1; draw_options_menu(); g_drawing_ui = 0;
                    continue;
                }
            }
            continue;
        }

        /* 3. Note Selection (Explicit checking prevents swallowing WASD) */
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
            safe_set_note_type(cur_note_type); update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages);
            continue;
        }

        /* 4. Audio Controls */
        if (b == KEY_Q || b == KEY_R) {
            int start_p = (b == KEY_R) ? 1 : cur_page;
            while (1) {
                if (cur_page != start_p) switch_page(start_p, cur_x, cur_y);
                toolbar_state.playback = TB_STATE_PLAYING; safe_draw_toolbar(cur_note_type);
                for (int p = start_p; p <= max_pages; p++) {
                    if (cur_page != p) switch_page(p, cur_x, cur_y);
                    seq_user_stopped = 0; seq_user_restarted = 0; seq_is_playing = 1; seq_is_paused = 0;
                    play_sequence();
                    if (seq_user_restarted || seq_user_stopped) break;
                }
                if (seq_user_restarted) { start_p = 1; continue; }
                break;
            }
            seq_is_playing = 0; toolbar_state.playback = TB_STATE_STOPPED;
            safe_draw_toolbar(cur_note_type); redraw_all_notes(); draw_cursor_cell(cur_x, cur_y);
            continue;
        }

        /* 5. Navigation & Pitch Editing (Arrows) */
        if (got_extended) {
            if (b == KEY_LEFT) { active_page_nav |= (1 << 0); switch_page(cur_page - 1, cur_x, cur_y); safe_draw_row2(cur_accidental); }
            else if (b == KEY_RIGHT) { active_page_nav |= (1 << 1); switch_page(cur_page + 1, cur_x, cur_y); safe_draw_row2(cur_accidental); }
            else if (b == KEY_UP || b == KEY_DOWN) {
                int d = (b == KEY_UP) ? -1 : 1;
                if (move_note_head(cur_col, cur_staff, cur_slot, d)) {
                    cur_row += d; cur_y = row_to_y(cur_row, &cur_staff, &cur_slot); draw_cursor_cell(cur_x, cur_y);
                }
            }
            got_extended = 0; continue;
        }

        /* 6. Page Structure (K/L) */
        if (b == KEY_K) { active_page_struct |= (1 << 0); if (max_pages < 8) max_pages++; update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages); safe_draw_row2(cur_accidental); continue; }
        if (b == KEY_L) { active_page_struct |= (1 << 1); if (max_pages > 1) { int i = 0; while (i < num_notes) { if (notes[i].page == max_pages) { notes[i] = notes[num_notes - 1]; num_notes--; } else i++; } max_pages--; if (cur_page > max_pages) switch_page(max_pages, cur_x, cur_y); else update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages); } safe_draw_row2(cur_accidental); continue; }

        /* 7. Accidentals (ZXCV) */
        if (b == KEY_Z || b == KEY_X || b == KEY_C || b == KEY_V) {
            if (b == KEY_Z) cur_accidental = ACC_NONE;
            if (b == KEY_X) cur_accidental = (cur_accidental == ACC_SHARP) ? ACC_NONE : ACC_SHARP;
            if (b == KEY_C) cur_accidental = (cur_accidental == ACC_FLAT) ? ACC_NONE : ACC_FLAT;
            if (b == KEY_V) cur_accidental = (cur_accidental == ACC_NATURAL) ? ACC_NONE : ACC_NATURAL;
            safe_draw_row2(cur_accidental); update_note_indicator(cur_note_type, cur_accidental, cur_page, max_pages);
            continue;
        }

        /* 8. Placement & Deletion */
        if (b == KEY_SPACE) {
            if (cur_note_type == NOTE_REST) {
                int rs = SLOTS_PER_STAFF / 2; int rr = cur_staff * SLOTS_PER_STAFF + rs; int ry = row_to_y(rr, 0, 0);
                place_note(cur_col, cur_staff, rs, cur_x, ry, cur_note_type); redraw_all_notes(); draw_cursor_cell(cur_x, ry);
            } else place_note(cur_col, cur_staff, cur_slot, cur_x, cur_y, cur_note_type);
            continue;
        }
        if (b == KEY_DELETE) { delete_note(cur_col, cur_staff, cur_slot, cur_x, cur_y); continue; }

        /* 9. Tempo */
        if (b == KEY_MINUS) { toolbar_set_bpm(toolbar_state.bpm - 5); safe_draw_toolbar(cur_note_type); continue; }
        if (b == KEY_EQUALS) { toolbar_set_bpm(toolbar_state.bpm + 5); safe_draw_toolbar(cur_note_type); continue; }

        /* 10. Cursor Movement (WASD) */
        if (b == KEY_W || b == KEY_A || b == KEY_S || b == KEY_D) {
            int nc = cur_col, nr = cur_row;
            if (b == KEY_W && cur_row > 0) nr--; 
            if (b == KEY_S && cur_row < TOTAL_ROWS - 1) nr++;
            if (b == KEY_A && cur_col > 1) nc--; 
            if (b == KEY_D && cur_col < TOTAL_COLS - 1) nc++;
            
            if (nc != cur_col || nr != cur_row) {
                erase_cursor_cell(cur_x, cur_y); cur_col = nc; cur_row = nr;
                cur_x = col_to_x(cur_col); cur_y = row_to_y(cur_row, &cur_staff, &cur_slot);
                redraw_all_notes(); draw_cursor_cell(cur_x, cur_y);
            }
            continue;
        }
        pixel_buffer_start = *pixel_ctrl;
    }
    return 0;
}