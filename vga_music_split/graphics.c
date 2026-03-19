#include "graphics.h"

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

int is_staff_line(int x, int y)
{
    int s, l;
    if (x < STAFF_LEFT || x > STAFF_RIGHT) return 0;
    for (s = 0; s < NUM_STAFFS; s++)
        for (l = 0; l < NUM_LINES; l++)
            if (y == staff_top[s] + l * LINE_GAP) return 1;
    return 0;
}

short int bg_color(int x, int y)
{
    if (y >= MENUBAR_H && is_staff_line(x, y)) return BLACK;
    return WHITE;
}

void restore_pixel(int x, int y)
{
    int i, ddx, ddy;

    /* 1. If a dot covers this pixel, keep it black */
    for (i = 0; i < num_dots; i++) {
        ddx = x - dot_x[i];
        ddy = y - dot_y[i];
        if (ddx*ddx + ddy*ddy <= DOT_R*DOT_R) {
            plot_pixel(x, y, BLACK);
            return;
        }
    }

    /* 2. Menu-bar region */
    if (y < MENUBAR_H) {
        plot_pixel(x, y, DARK_GREY);
        return;
    }

    /* 3. Staff line or plain white */
    plot_pixel(x, y, bg_color(x, y));
}
