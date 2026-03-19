#include <stdlib.h>
#include "sheet_music_pixels.h"

/* ═══════════════════════════════════════════════════════════════════════
   VGA frame-buffer
   512-wide x 256-tall in memory, stride = y<<10 (1024 bytes/row).
   VGA doubles to 640x480. Visible region: 320 x 240.
   ═══════════════════════════════════════════════════════════════════════ */
#define FB_WIDTH    320
#define FB_HEIGHT   240

/* Arrow glyph size */
#define ARROW_W     11
#define ARROW_H     16

/* Hardware addresses */
#define PIXEL_BUF_CTRL  0xFF203020
#define PS2_BASE        0xFF200100
#define PS2_RVALID      0x8000

/* Mouse movement divisor */
#define SPEED_DIV   2

/* Click dot */
#define DOT_R       3
#define MAX_DOTS    256

/* Colours RGB565 */
#define WHITE  ((short int)0xFFFF)
#define BLACK  ((short int)0x0000)

/* ═══════════════════════════════════════════════════════════════════════
   Globals
   ═══════════════════════════════════════════════════════════════════════ */
int pixel_buffer_start;

/* Precomputed background: one RGB565 value per visible pixel (320x240).
   Stored in a flat array so bg lookup is a single array read — no
   multiply, divide, or bit-extract at runtime.                          */
static short int bg[FB_HEIGHT][FB_WIDTH];

int dot_x[MAX_DOTS];
int dot_y[MAX_DOTS];
int num_dots = 0;

/* ═══════════════════════════════════════════════════════════════════════
   Pixel write
   ═══════════════════════════════════════════════════════════════════════ */
void plot_pixel(int x, int y, short int c)
{
    if (x < 0 || x >= FB_WIDTH)  return;
    if (y < 0 || y >= FB_HEIGHT) return;
    *(volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = c;
}

/* ═══════════════════════════════════════════════════════════════════════
   Background precompute + draw
   ───────────────────────────────────────────────────────────────────────
   Called ONCE at startup. Iterates over 320x240 destination pixels,
   maps each to the 630x480 source bitmap using nearest-neighbour scale,
   stores result in bg[][] AND writes to the frame buffer.

   Key optimisation: sy = fy*2 (exact integer, no divide).
   sx uses a precomputed lookup table (320 entries) so no per-pixel divide.
   ═══════════════════════════════════════════════════════════════════════ */
#define BG_BYTES_ROW  79   /* ceil(630/8) */

void build_and_draw_background(void)
{
    int x, y;

    /* Precompute sx lookup: for each dst column fx, what source column? */
    /* sx = fx * 630 / 320  — compute once, store in small array          */
    static unsigned short sx_lut[FB_WIDTH];
    for (x = 0; x < FB_WIDTH; x++)
        sx_lut[x] = (unsigned short)((x * 630) / 320);

    /* Wipe the full 512x256 frame buffer to white (kills border garbage) */
    for (y = 0; y < 256; y++) {
        volatile short int *row =
            (volatile short int *)(pixel_buffer_start + (y << 10));
        for (x = 0; x < 512; x++)
            row[x] = WHITE;
    }

    /* Render sheet music into visible 320x240, filling bg[][] too */
    for (y = 0; y < FB_HEIGHT; y++) {
        int sy = y * 2;                        /* exact: 480/240 = 2     */
        const unsigned char *src_row =
            sheet_music_bitmap + sy * BG_BYTES_ROW;

        volatile short int *dst_row =
            (volatile short int *)(pixel_buffer_start + (y << 10));

        for (x = 0; x < FB_WIDTH; x++) {
            int sx      = sx_lut[x];
            int bit_off = 7 - (sx & 7);
            short int c = (src_row[sx >> 3] >> bit_off) & 1 ? BLACK : WHITE;
            bg[y][x] = c;
            dst_row[x] = c;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   Restore pixel: checks dots first, then bg[][]
   ═══════════════════════════════════════════════════════════════════════ */
void restore_pixel(int x, int y)
{
    int i, ddx, ddy;
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;
    for (i = 0; i < num_dots; i++) {
        ddx = x - dot_x[i];
        ddy = y - dot_y[i];
        if (ddx*ddx + ddy*ddy <= DOT_R*DOT_R) {
            plot_pixel(x, y, BLACK);
            return;
        }
    }
    plot_pixel(x, y, bg[y][x]);
}

/* ═══════════════════════════════════════════════════════════════════════
   Arrow cursor (tip at tx,ty) — drawn in BLACK
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
   Dot draw (black filled circle)
   ═══════════════════════════════════════════════════════════════════════ */
void draw_dot(int cx, int cy)
{
    int dx, dy;
    for (dy = -DOT_R; dy <= DOT_R; dy++)
        for (dx = -DOT_R; dx <= DOT_R; dx++)
            if (dx*dx + dy*dy <= DOT_R*DOT_R)
                plot_pixel(cx+dx, cy+dy, BLACK);
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

/* Send one byte and wait for 0xFA ACK with timeout */
static void ps2_send_byte(volatile int *ps2, unsigned char b)
{
    int timeout = 2000000;
    /* Flush FIFO before sending so we don't misread a stale 0xFA */
    while (*ps2 & PS2_RVALID) (void)(*ps2);
    *ps2 = (int)b;
    while (timeout-- > 0) {
        int v = *ps2;
        if ((v & PS2_RVALID) && (v & 0xFF) == 0xFA) break;
    }
}

/* ─────────────────────────────────────────────────────────────────────
   Mouse init — called BEFORE draw_background so the FIFO never overflows
   ───────────────────────────────────────────────────────────────────── */
void mouse_init(void)
{
    volatile int *ps2 = (volatile int *)PS2_BASE;
    int i;

    /* Hard flush: drain anything in the FIFO */
    for (i = 0; i < 256; i++) (void)(*ps2);
    while (*ps2 & PS2_RVALID) (void)(*ps2);

    /* Send reset; ps2_send_byte flushes before writing and waits for ACK */
    ps2_send_byte(ps2, 0xFF);

    /* Wait for BAT complete (0xAA) and device ID (0x00) — up to 2M reads */
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

    /* Final flush then enable streaming */
    while (*ps2 & PS2_RVALID) (void)(*ps2);
    ps2_send_byte(ps2, 0xF4);
    /* Drain ACK */
    while (*ps2 & PS2_RVALID) (void)(*ps2);
}

/* ═══════════════════════════════════════════════════════════════════════
   Main
   ═══════════════════════════════════════════════════════════════════════ */
int main(void)
{
    volatile int *pixel_ctrl = (volatile int *)PIXEL_BUF_CTRL;
    volatile int *ps2        = (volatile int *)PS2_BASE;

    /* Single-buffer VGA init: read front buffer, set back = same address */
    pixel_buffer_start = *pixel_ctrl;
    *(pixel_ctrl + 1)  = pixel_buffer_start;

    /* ── Init mouse FIRST so FIFO doesn't overflow during background draw ── */
    mouse_init();

    /* ── Build background lookup table + draw to frame buffer ── */
    build_and_draw_background();

    /* Cursor position — clamp so entire arrow glyph stays on screen */
    int cx = FB_WIDTH  / 2;
    int cy = FB_HEIGHT / 2;
    int cx_max = FB_WIDTH  - ARROW_W;   /* 309 */
    int cy_max = FB_HEIGHT - ARROW_H;   /* 224 */

    int ax = 0, ay = 0;   /* sub-pixel accumulators */

    /* PS/2 packet state */
    int byte_idx = 0;
    unsigned char pkt[3];
    int prev_left = 0;

    /* Draw initial cursor */
    draw_arrow(cx, cy);

    /* ── Main loop ── */
    while (1)
    {
        int raw = ps2_read_byte(ps2);
        if (raw < 0) continue;

        unsigned char b = (unsigned char)raw;

        /* Packet sync: flags byte (byte 0) always has bit 3 = 1.
           If waiting for byte 0 and bit 3 is clear, it's a leftover
           delta byte — discard and keep waiting.                        */
        if (byte_idx == 0 && !(b & 0x08)) continue;

        pkt[byte_idx++] = b;
        if (byte_idx < 3) continue;
        byte_idx = 0;

        /* Decode 3-byte packet */
        unsigned char flags = pkt[0];
        int dx = (int)pkt[1];
        int dy = (int)pkt[2];

        if (flags & 0x10) dx |= 0xFFFFFF00;   /* sign-extend X */
        if (flags & 0x20) dy |= 0xFFFFFF00;   /* sign-extend Y */
        if (flags & 0xC0) { prev_left = (flags & 0x01); continue; } /* overflow */

        /* Speed divide via accumulator (no dropped sub-pixel motion) */
        ax += dx;  ay += dy;
        int mx = ax / SPEED_DIV;
        int my = ay / SPEED_DIV;
        ax -= mx * SPEED_DIV;
        ay -= my * SPEED_DIV;

        /* Erase old cursor (restores bg + any dots underneath) */
        erase_arrow(cx, cy);

        /* Move — PS/2 Y is inverted relative to screen Y */
        cx += mx;
        cy -= my;
        if (cx < 0)       cx = 0;
        if (cx > cx_max)  cx = cx_max;
        if (cy < 0)       cy = 0;
        if (cy > cy_max)  cy = cy_max;

        /* Rising-edge left click → place black dot */
        int left_now = (flags & 0x01) ? 1 : 0;
        if (left_now && !prev_left) {
            if (num_dots < MAX_DOTS) {
                dot_x[num_dots] = cx;
                dot_y[num_dots] = cy;
                num_dots++;
            }
            draw_dot(cx, cy);
        }
        prev_left = left_now;

        /* Draw cursor at new position */
        draw_arrow(cx, cy);

        /* Keep buffer pointer fresh (single-buffer, no swap) */
        pixel_buffer_start = *pixel_ctrl;
    }

    return 0;
}
