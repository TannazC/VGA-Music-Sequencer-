/*
 * main.c  —  Mouse cursor + right-click dot placement test
 *
 * What it does:
 *   - Clears the screen to dark gray on startup
 *   - Draws a white crosshair cursor (5x5 cross shape) that follows the mouse
 *   - On right-click (rising edge): stamps a small red 5x5 box at that position
 *   - Dots persist on screen; cursor moves freely over them
 *
 * Build:   gmake COMPILE
 * Flash:   gmake DE1-SoC
 * Watch:   gmake TERMINAL   (optional — prints click coords to JTAG terminal)
 *
 * Wiring:
 *   - Mouse must be on PS/2 port 2 (PS2_BASE2 = 0xFF200108)
 *   - Use the Y-splitter from BA3145/3155/3165 if keyboard is also plugged in
 *   - If using only the mouse, plug it directly into either PS/2 port and
 *     change PS2_BASE2 to PS2_BASE (0xFF200100) in config.h
 */

#include <stdio.h>
#include "config.h"
#include "vga.h"
#include "ps2.h"

/* ── Cursor appearance ───────────────────────────────────────────────────────
 * A simple crosshair: a 1-pixel vertical line and 1-pixel horizontal line,
 * each 5 pixels long, centred on (mouse_x, mouse_y).
 * We also draw the single centre pixel in a contrasting color so it's
 * visible against both light and dark backgrounds.
 */
#define CURSOR_COLOR COLOR_WHITE
#define CURSOR_OUTLINE COLOR_BLACK /* 1-pixel black border around cross arms */
#define CURSOR_SIZE 5              /* arm length each side from centre */

/* ── Dot appearance ──────────────────────────────────────────────────────────
 * Right-click stamps a filled 5x5 red square centred on the click position.
 */
#define DOT_COLOR COLOR_RED
#define DOT_SIZE 5

/* ── Background color ────────────────────────────────────────────────────────*/
#define BG_COLOR RGB565(30, 30, 30) /* dark gray */

/* ── Mouse position (accumulated from deltas) ───────────────────────────────*/
static int mouse_x = SCREEN_W / 2;
static int mouse_y = SCREEN_H / 2;

/* Previous cursor position so we can erase it before redrawing */
static int prev_x = SCREEN_W / 2;
static int prev_y = SCREEN_H / 2;

/* ── Erase cursor ────────────────────────────────────────────────────────────
 * Restore the pixels the crosshair was drawn over back to BG_COLOR.
 * We only erase the cross arms — dots painted underneath will get
 * partially erased, which is acceptable for a test.
 * For the real sequencer the cursor will sit on top of the grid cells
 * and we'll redraw the cell underneath instead.
 */
static void erase_cursor(int x, int y)
{
    int i;
    /* Horizontal arm */
    for (i = x - CURSOR_SIZE; i <= x + CURSOR_SIZE; i++)
        vga_draw_pixel(i, y, BG_COLOR);
    /* Vertical arm */
    for (i = y - CURSOR_SIZE; i <= y + CURSOR_SIZE; i++)
        vga_draw_pixel(x, i, BG_COLOR);
}

/* ── Draw cursor ─────────────────────────────────────────────────────────────
 * Thin black outline cross then white inner cross, giving a bordered look
 * that's visible on any background color.
 */
static void draw_cursor(int x, int y)
{
    int i;

    /* Black outline: draw slightly larger cross first */
    for (i = x - CURSOR_SIZE - 1; i <= x + CURSOR_SIZE + 1; i++)
    {
        vga_draw_pixel(i, y - 1, CURSOR_OUTLINE);
        vga_draw_pixel(i, y + 1, CURSOR_OUTLINE);
    }
    for (i = y - CURSOR_SIZE - 1; i <= y + CURSOR_SIZE + 1; i++)
    {
        vga_draw_pixel(x - 1, i, CURSOR_OUTLINE);
        vga_draw_pixel(x + 1, i, CURSOR_OUTLINE);
    }

    /* White inner cross */
    for (i = x - CURSOR_SIZE; i <= x + CURSOR_SIZE; i++)
        vga_draw_pixel(i, y, CURSOR_COLOR);
    for (i = y - CURSOR_SIZE; i <= y + CURSOR_SIZE; i++)
        vga_draw_pixel(x, i, CURSOR_COLOR);

    /* Centre dot in a contrasting color so the hotspot is clear */
    vga_draw_pixel(x, y, COLOR_CYAN);
}

/* ── Place dot ───────────────────────────────────────────────────────────────
 * Stamp a filled DOT_SIZE x DOT_SIZE square centred on (x, y).
 */
static void place_dot(int x, int y)
{
    int half = DOT_SIZE / 2;
    vga_draw_rect(x - half, y - half, DOT_SIZE, DOT_SIZE, DOT_COLOR);
}

/* ── main ────────────────────────────────────────────────────────────────────*/
int main(void)
{
    /* Init */
    vga_clear(BG_COLOR);
    mouse_init();

    /* Draw initial cursor */
    draw_cursor(mouse_x, mouse_y);

    printf("Mouse test ready.\n");
    printf("Move mouse to see cursor. Right-click to place a red dot.\n");

    MOUSE_PACKET pkt;
    int right_prev = 0;

    while (1)
    {
        if (!mouse_read_packet(&pkt))
            continue;

        /* ── Update position ── */
        int new_x = mouse_x + pkt.dx;
        int new_y = mouse_y + pkt.dy;

        /* Clamp to screen */
        if (new_x < 0)
            new_x = 0;
        if (new_x >= SCREEN_W)
            new_x = SCREEN_W - 1;
        if (new_y < 0)
            new_y = 0;
        if (new_y >= SCREEN_H)
            new_y = SCREEN_H - 1;

        /* ── Right-click: place dot on rising edge ── */
        int right_now = pkt.right_btn;
        if (right_now && !right_prev)
        {
            place_dot(new_x, new_y);
            printf("DOT placed at (%d, %d)\n", new_x, new_y);
        }
        right_prev = right_now;

        /* ── Redraw cursor only if it moved ── */
        if (new_x != mouse_x || new_y != mouse_y)
        {
            erase_cursor(mouse_x, mouse_y);
            mouse_x = new_x;
            mouse_y = new_y;
            draw_cursor(mouse_x, mouse_y);
        }
    }

    return 0;
}