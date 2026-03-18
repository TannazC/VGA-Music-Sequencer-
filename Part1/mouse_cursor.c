#include <stdlib.h>

#define FB_WIDTH     320
#define FB_HEIGHT    240
#define ARROW_W      11
#define ARROW_H      16

#define PIXEL_BUF_CTRL  0xFF203020
#define PS2_BASE        0xFF200100
#define PS2_RVALID      0x8000

#define SPEED_DIV    2
#define DOT_R        3
#define MAX_DOTS     256

/* ── Menu bar ─────────────────────────────────────────────────────────── */
#define MENUBAR_H    22
#define BTN_Y        4
#define BTN_H        16
#define BTN_W        40
#define BTN_GAP      6
#define BTN_PLAY_X   4
#define BTN_STOP_X   (BTN_PLAY_X   + BTN_W + BTN_GAP)
#define BTN_SPD_UP_X (BTN_STOP_X   + BTN_W + BTN_GAP)
#define BTN_SPD_DN_X (BTN_SPD_UP_X + BTN_W + BTN_GAP)

/* ── Staff layout ─────────────────────────────────────────────────────── */
#define NUM_STAFFS   4
#define NUM_LINES    5
#define LINE_GAP     6
#define STAFF_LEFT   28
#define STAFF_RIGHT  (FB_WIDTH - 2)

static const int staff_top[NUM_STAFFS] = { 40, 95, 150, 195 };

/* ── Colours RGB565 ───────────────────────────────────────────────────── */
#define WHITE      ((short int)0xFFFF)
#define BLACK      ((short int)0x0000)
#define DARK_GREY  ((short int)0x4A49)
#define GREEN      ((short int)0x07E0)
#define RED        ((short int)0xF800)
#define BLUE       ((short int)0x001F)
#define ORANGE     ((short int)0xFC00)

/* ── Globals ──────────────────────────────────────────────────────────── */
int pixel_buffer_start;
int dot_x[MAX_DOTS];
int dot_y[MAX_DOTS];
int num_dots = 0;

/* ── Low-level pixel ──────────────────────────────────────────────────── */
void plot_pixel(int x, int y, short int c)
{
    if (x < 0 || x >= FB_WIDTH)  return;
    if (y < 0 || y >= FB_HEIGHT) return;
    *(volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = c;
}

void fill_rect(int x, int y, int w, int h, short int c)
{
    int px, py;
    for (py = y; py < y + h; py++)
        for (px = x; px < x + w; px++)
            plot_pixel(px, py, c);
}

/* ── Staff-line query ─────────────────────────────────────────────────── */
int is_staff_line(int x, int y)
{
    int s, l;
    if (x < STAFF_LEFT || x > STAFF_RIGHT) return 0;
    for (s = 0; s < NUM_STAFFS; s++)
        for (l = 0; l < NUM_LINES; l++)
            if (y == staff_top[s] + l * LINE_GAP) return 1;
    return 0;
}

/* ── Background colour at (x,y) ───────────────────────────────────────── */
short int bg_color(int x, int y)
{
    if (y >= MENUBAR_H && is_staff_line(x, y)) return BLACK;
    return WHITE;
}

/* ── restore_pixel: dot > menubar > staff line > white ───────────────── */
void restore_pixel(int x, int y)
{
    int i, ddx, ddy;
    for (i = 0; i < num_dots; i++) {
        ddx = x - dot_x[i];
        ddy = y - dot_y[i];
        if (ddx*ddx + ddy*ddy <= DOT_R*DOT_R) {
            plot_pixel(x, y, BLACK);
            return;
        }
    }
    if (y < MENUBAR_H) {
        plot_pixel(x, y, DARK_GREY);
        return;
    }
    plot_pixel(x, y, bg_color(x, y));
}

/* ── 5x7 bitmap font ──────────────────────────────────────────────────── */
static const unsigned char FONT[][5] = {
/* P */ { 0x7E, 0x12, 0x12, 0x0C, 0x00 },
/* L */ { 0x7E, 0x40, 0x40, 0x40, 0x00 },
/* A */ { 0x3E, 0x48, 0x48, 0x3E, 0x00 },
/* Y */ { 0x06, 0x08, 0x70, 0x08, 0x06 },
/* S */ { 0x46, 0x4A, 0x52, 0x62, 0x00 },
/* T */ { 0x02, 0x02, 0x7E, 0x02, 0x02 },
/* O */ { 0x3C, 0x42, 0x42, 0x3C, 0x00 },
/* E */ { 0x7E, 0x4A, 0x4A, 0x42, 0x00 },
/* D */ { 0x7E, 0x42, 0x42, 0x3C, 0x00 },
/* + */ { 0x08, 0x08, 0x3E, 0x08, 0x08 },
/* - */ { 0x08, 0x08, 0x08, 0x08, 0x08 },
/*   */ { 0x00, 0x00, 0x00, 0x00, 0x00 },
};

static int char_idx(char c)
{
    switch(c){
        case 'P': return 0; case 'L': return 1; case 'A': return 2;
        case 'Y': return 3; case 'S': return 4; case 'T': return 5;
        case 'O': return 6; case 'E': return 7; case 'D': return 8;
        case '+': return 9; case '-': return 10; default: return 11;
    }
}

void draw_char(int px, int py, char ch, short int c)
{
    int col, row, idx = char_idx(ch);
    for (col = 0; col < 5; col++) {
        unsigned char bits = FONT[idx][col];
        for (row = 0; row < 7; row++)
            if ((bits >> (6 - row)) & 1)
                plot_pixel(px + col, py + row, c);
    }
}

void draw_string(int px, int py, const char *s, short int c)
{
    while (*s) { draw_char(px, py, *s, c); px += 6; s++; }
}

/* ── Menu bar ─────────────────────────────────────────────────────────── */
void draw_menubar(void)
{
    fill_rect(0, 0, FB_WIDTH, MENUBAR_H, DARK_GREY);

    fill_rect(BTN_PLAY_X,   BTN_Y, BTN_W, BTN_H, GREEN);
    draw_string(BTN_PLAY_X + 4,   BTN_Y + 5, "PLAY", WHITE);

    fill_rect(BTN_STOP_X,   BTN_Y, BTN_W, BTN_H, RED);
    draw_string(BTN_STOP_X + 4,   BTN_Y + 5, "STOP", WHITE);

    fill_rect(BTN_SPD_UP_X, BTN_Y, BTN_W, BTN_H, BLUE);
    draw_string(BTN_SPD_UP_X + 2, BTN_Y + 5, "SPD+", WHITE);

    fill_rect(BTN_SPD_DN_X, BTN_Y, BTN_W, BTN_H, ORANGE);
    draw_string(BTN_SPD_DN_X + 2, BTN_Y + 5, "SPD-", WHITE);
}

/* ── Staff lines ──────────────────────────────────────────────────────── */
void draw_staff_lines(void)
{
    int s, l, x;
    for (s = 0; s < NUM_STAFFS; s++)
        for (l = 0; l < NUM_LINES; l++)
            for (x = STAFF_LEFT; x <= STAFF_RIGHT; x++)
                plot_pixel(x, staff_top[s] + l * LINE_GAP, BLACK);
}

/* ── Full background draw ─────────────────────────────────────────────── */
void draw_background(void)
{
    int x, y;
    volatile short int *a;

    /* clear entire frame buffer to white */
    for (y = 0; y < 256; y++)
        for (x = 0; x < 512; x++) {
            a = (volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1));
            *a = WHITE;
        }

    draw_menubar();
    draw_staff_lines();
}

/* ── Arrow cursor ─────────────────────────────────────────────────────── */
static const unsigned char AX0[16] = {0,0,0,0,0,0,0,0,0,0, 0,3,3,3,3,3};
static const unsigned char AX1[16] = {0,1,2,3,4,5,6,7,8,9,10,6,6,6,6,6};

void draw_arrow(int tx, int ty, short int c)
{
    int row, col;
    for (row = 0; row < ARROW_H; row++)
        for (col = AX0[row]; col <= AX1[row]; col++)
            plot_pixel(tx + col, ty + row, c);
}

void erase_arrow(int tx, int ty)
{
    int row, col;
    for (row = 0; row < ARROW_H; row++)
        for (col = AX0[row]; col <= AX1[row]; col++)
            restore_pixel(tx + col, ty + row);
}

/* ── Note dot ─────────────────────────────────────────────────────────── */
void draw_dot(int cx, int cy)
{
    int dx, dy;
    if (cy < MENUBAR_H) return;
    for (dy = -DOT_R; dy <= DOT_R; dy++)
        for (dx = -DOT_R; dx <= DOT_R; dx++)
            if (dx*dx + dy*dy <= DOT_R*DOT_R)
                plot_pixel(cx+dx, cy+dy, BLACK);
}

/* ── Button hit test ──────────────────────────────────────────────────── */
int button_hit(int x, int y)
{
    if (y < BTN_Y || y >= BTN_Y + BTN_H) return -1;
    if (x >= BTN_PLAY_X   && x < BTN_PLAY_X   + BTN_W) return 0;
    if (x >= BTN_STOP_X   && x < BTN_STOP_X   + BTN_W) return 1;
    if (x >= BTN_SPD_UP_X && x < BTN_SPD_UP_X + BTN_W) return 2;
    if (x >= BTN_SPD_DN_X && x < BTN_SPD_DN_X + BTN_W) return 3;
    return -1;
}

/* ── PS/2 helpers ─────────────────────────────────────────────────────── */
static int ps2_read_byte(volatile int *ps2)
{
    int v = *ps2;
    if (v & PS2_RVALID) return v & 0xFF;
    return -1;
}

static void ps2_send_byte(volatile int *ps2, unsigned char b)
{
    int timeout = 2000000;
    *ps2 = (int)b;
    while (timeout-- > 0) {
        int v = *ps2;
        if ((v & PS2_RVALID) && (v & 0xFF) == 0xFA) break;
    }
}

void mouse_init(void)
{
    volatile int *ps2 = (volatile int *)PS2_BASE;
    int drain;
    while (*ps2 & PS2_RVALID) (void)(*ps2);
    ps2_send_byte(ps2, 0xFF);
    for (drain = 0; drain < 500000; drain++) (void)(*ps2);
    while (*ps2 & PS2_RVALID) (void)(*ps2);
    ps2_send_byte(ps2, 0xF4);
    while (*ps2 & PS2_RVALID) (void)(*ps2);
}

/* ── Main ─────────────────────────────────────────────────────────────── */
int main(void)
{
    volatile int *pixel_ctrl = (volatile int *)PIXEL_BUF_CTRL;
    volatile int *ps2        = (volatile int *)PS2_BASE;

    pixel_buffer_start = *pixel_ctrl;
    *(pixel_ctrl + 1)  = pixel_buffer_start;

    draw_background();
    mouse_init();

    int cx = FB_WIDTH  / 2;
    int cy = FB_HEIGHT / 2;
    int cx_max = FB_WIDTH  - ARROW_W;
    int cy_max = FB_HEIGHT - ARROW_H;
    int ax = 0, ay = 0;
    int byte_idx = 0;
    unsigned char pkt[3];
    int prev_left = 0;

    draw_arrow(cx, cy, BLACK);

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
        if (flags & 0xC0) { prev_left = (flags & 0x01); continue; }

        ax += dx; ay += dy;
        int mx = ax / SPEED_DIV;
        int my = ay / SPEED_DIV;
        ax -= mx * SPEED_DIV;
        ay -= my * SPEED_DIV;

        erase_arrow(cx, cy);

        cx += mx;
        cy -= my;

        if (cx < 0)      cx = 0;
        if (cx > cx_max) cx = cx_max;
        if (cy < 0)      cy = 0;
        if (cy > cy_max) cy = cy_max;

        int left_now = (flags & 0x01) ? 1 : 0;
        if (left_now && !prev_left) {
            int btn = button_hit(cx, cy);
            if (btn >= 0) {
                /* TODO: Play=0, Stop=1, Speed+=2, Speed-=3 */
            } else if (cy >= MENUBAR_H && num_dots < MAX_DOTS) {
                dot_x[num_dots] = cx;
                dot_y[num_dots] = cy;
                num_dots++;
                draw_dot(cx, cy);
            }
        }
        prev_left = left_now;

        draw_arrow(cx, cy, BLACK);

        pixel_buffer_start = *pixel_ctrl;
    }

    return 0;
}