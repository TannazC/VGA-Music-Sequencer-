#include <stdlib.h>
#include "background.h"

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