#include <stdlib.h>

/* ── Frame-buffer dimensions ── */
#define FB_WIDTH    320
#define FB_HEIGHT   240

/* ── Staff layout (exposed so callers can snap notes to lines) ──────────
   staff_top[s]  : y of the topmost line of staff s  (extern in background.c)
   STAFF_SPACING : pixels between adjacent lines
   LINES_PER_STAFF, NUM_STAVES : array bounds                            */
#define NUM_STAVES      4
#define LINES_PER_STAFF 5
#define STAFF_SPACING   8
#define STAFF_X0        20
#define STAFF_X1       (FB_WIDTH - 10)

/* Precomputed background: one RGB565 value per visible pixel.
   Exposed so restore_pixel (in vga_music_v2.c) can read bg[y][x].      */
extern short int bg[FB_HEIGHT][FB_WIDTH];

/* Build bg[][] procedurally (staff lines + treble clefs) AND blit to
   the frame buffer.  Call once at startup after pixel_buffer_start is
   set.                                                                   */
void build_and_draw_background(void);

/* ═══════════════════════════════════════════════════════════════════════
   Colours RGB565
   ═══════════════════════════════════════════════════════════════════════ */
#define WHITE  ((short int)0xFFFF)
#define BLACK  ((short int)0x0000)

/* Precomputed background: one RGB565 value per visible pixel (320x240).
   Stored in a flat array so bg lookup is a single array read.           */
short int bg[FB_HEIGHT][FB_WIDTH];

/* Provided by vga_music_v2.c */
extern int pixel_buffer_start;

/* ═══════════════════════════════════════════════════════════════════════
   Staff layout
   ───────────────────────────────────────────────────────────────────────
   Two staves, each with 5 horizontal lines spaced STAFF_SPACING px apart.
   STAFF_X0 / STAFF_X1 : left and right x extents of each staff.
   staff_top[] : y-coordinate of the topmost line for each staff.
   ═══════════════════════════════════════════════════════════════════════ */
#define NUM_STAVES      2
#define LINES_PER_STAFF 5
#define STAFF_SPACING   8          /* pixels between adjacent staff lines */
#define STAFF_X0        20         /* leave room for treble-clef glyph    */
#define STAFF_X1       (FB_WIDTH - 10)

/* Top line of each staff (screen y-coordinate) */
static const int staff_top[NUM_STAVES] = { 52, 148 };

/* ═══════════════════════════════════════════════════════════════════════
   Helper: write one pixel to both bg[][] and the frame buffer
   ═══════════════════════════════════════════════════════════════════════ */
static void bg_plot(int x, int y, short int c)
{
    bg[y][x] = c;
    *(volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = c;
}

/* ═══════════════════════════════════════════════════════════════════════
   Draw staff lines
   ───────────────────────────────────────────────────────────────────────
   Draws NUM_STAVES staves of LINES_PER_STAFF horizontal black lines each,
   spaced STAFF_SPACING pixels apart, across x = [STAFF_X0, STAFF_X1).
   ═══════════════════════════════════════════════════════════════════════ */
static void draw_staves(void)
{
    int s, l, x;
    for (s = 0; s < NUM_STAVES; s++) {
        for (l = 0; l < LINES_PER_STAFF; l++) {
            int y = staff_top[s] + l * STAFF_SPACING;
            for (x = STAFF_X0; x < STAFF_X1; x++)
                bg_plot(x, y, BLACK);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   Draw vertical bar lines
   ───────────────────────────────────────────────────────────────────────
   Left and right bar lines that frame each staff (spanning only the
   height of the staff from top line to bottom line).
   ═══════════════════════════════════════════════════════════════════════ */
static void draw_barlines(void)
{
    int s, y;
    for (s = 0; s < NUM_STAVES; s++) {
        int y0 = staff_top[s];
        int y1 = staff_top[s] + (LINES_PER_STAFF - 1) * STAFF_SPACING;
        for (y = y0; y <= y1; y++) {
            bg_plot(STAFF_X0,     y, BLACK);   /* left bar  */
            bg_plot(STAFF_X1 - 1, y, BLACK);   /* right bar */
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   Treble clef pixel glyph
   ───────────────────────────────────────────────────────────────────────
   A 10 x 32 pixel-art treble clef, stored as 32 bytes (1 bit per column,
   MSB = column 0).  Drawn with its baseline on the second staff line
   (B4 line) of each staff, which is standard notation placement.
   ═══════════════════════════════════════════════════════════════════════ */
#define CLEF_W  10
#define CLEF_H  32

static const unsigned short clef_glyph[CLEF_H] = {
    /*  row  0 */ 0x030, /*    ##              */
    /*  row  1 */ 0x078, /*   ####             */
    /*  row  2 */ 0x0FC, /*  ######            */
    /*  row  3 */ 0x0CC, /*  ##  ##            */
    /*  row  4 */ 0x0CC, /*  ##  ##            */
    /*  row  5 */ 0x0F8, /*  #####             */
    /*  row  6 */ 0x1E0, /* ####               */
    /*  row  7 */ 0x3C0, /*##  ##  (curve out) */
    /*  row  8 */ 0x3C0, /*##  ##              */
    /*  row  9 */ 0x1E0, /* ####               */
    /*  row 10 */ 0x0F8, /*  #####             */
    /*  row 11 */ 0x0FC, /*  ######            */
    /*  row 12 */ 0x0CE, /*  ##  ###           */
    /*  row 13 */ 0x0C6, /*  ##   ##           */
    /*  row 14 */ 0x0C6, /*  ##   ##           */
    /*  row 15 */ 0x0EE, /*  ### ###           */
    /*  row 16 */ 0x07C, /*   #####            */
    /*  row 17 */ 0x038, /*    ###             */
    /*  row 18 */ 0x030, /*    ##              */
    /*  row 19 */ 0x030, /*    ##              */
    /*  row 20 */ 0x030, /*    ##   (stem)     */
    /*  row 21 */ 0x030, /*    ##              */
    /*  row 22 */ 0x030, /*    ##              */
    /*  row 23 */ 0x070, /*   ###              */
    /*  row 24 */ 0x0F8, /*  #####             */
    /*  row 25 */ 0x1FC, /* #######            */
    /*  row 26 */ 0x386, /*###    ##           */
    /*  row 27 */ 0x306, /*##     ##           */
    /*  row 28 */ 0x306, /*##     ##           */
    /*  row 29 */ 0x1CC, /* ###  ##            */
    /*  row 30 */ 0x0F8, /*  #####             */
    /*  row 31 */ 0x030, /*    ##              */
};

static void draw_treble_clef(int x0, int y0)
{
    int row, col;
    for (row = 0; row < CLEF_H; row++) {
        unsigned short bits = clef_glyph[row];
        for (col = 0; col < CLEF_W; col++) {
            /* MSB of a 10-bit field maps to col 0 */
            if (bits & (1 << (CLEF_W - 1 - col))) {
                int px = x0 + col;
                int py = y0 + row;
                if (px >= 0 && px < FB_WIDTH && py >= 0 && py < FB_HEIGHT)
                    bg_plot(px, py, BLACK);
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   build_and_draw_background
   ───────────────────────────────────────────────────────────────────────
   1. Wipes the full 512x256 frame buffer and bg[][] to white.
   2. Draws staff lines, bar lines, and treble clefs procedurally.
   Call once at startup after pixel_buffer_start is set.
   ═══════════════════════════════════════════════════════════════════════ */
void build_and_draw_background(void)
{
    int x, y;

    /* ── Fill entire frame buffer (including non-visible border) white ── */
    for (y = 0; y < 256; y++) {
        volatile short int *row =
            (volatile short int *)(pixel_buffer_start + (y << 10));
        for (x = 0; x < 512; x++)
            row[x] = WHITE;
    }

    /* ── Initialise bg[][] to white ── */
    for (y = 0; y < FB_HEIGHT; y++)
        for (x = 0; x < FB_WIDTH; x++)
            bg[y][x] = WHITE;

    /* ── Draw music notation elements ── */
    draw_staves();
    draw_barlines();

    /* Place treble clef: top of glyph sits one STAFF_SPACING above the
       top staff line so the curl lands on the correct pitch line.       */
    int s;
    for (s = 0; s < NUM_STAVES; s++) {
        int clef_y = staff_top[s] - STAFF_SPACING;   /* one space above top line */
        draw_treble_clef(STAFF_X0 + 1, clef_y);
    }
}
/* ═══════════════════════════════════════════════════════════════════════
   VGA frame-buffer
   512-wide x 256-tall in memory, stride = y<<10 (1024 bytes/row).
   VGA doubles to 640x480. Visible region: 320 x 240.
   FB_WIDTH and FB_HEIGHT are defined in background.h
   ═══════════════════════════════════════════════════════════════════════ */

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

/* bg[][] is defined in background.c and declared extern in background.h */

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

    /* ── Build background procedurally + draw to frame buffer ── */
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