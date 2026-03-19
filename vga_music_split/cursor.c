#include "vga_music.h"
#include "graphics.h"
#include "background.h"
#include "cursor.h"

/* Arrow glyph — left and right column bounds per row */
static const unsigned char AX0[ARROW_H] = {0,0,0,0,0,0,0,0,0,0, 0,3,3,3,3,3};
static const unsigned char AX1[ARROW_H] = {0,1,2,3,4,5,6,7,8,9,10,6,6,6,6,6};

void draw_arrow(int tx, int ty, short int c)
{
    int row, col;
    for (row = 0; row < ARROW_H; row++)
        for (col = AX0[row]; col <= AX1[row]; col++)
            plot_pixel(tx + col, ty + row, c);
}

void erase_arrow(int tx, int ty)
{
    int row, col, s;

    /* Restore pixels the arrow was drawn over */
    for (row = 0; row < ARROW_H; row++)
        for (col = AX0[row]; col <= AX1[row]; col++)
            restore_pixel(tx + col, ty + row);

    /*
     * If the arrow overlapped a treble clef, redraw the clef.
     *
     * FIX: the original code used (clef_x + 16) as the right edge,
     * which is only half the glyph width (CLEF_W = 18).  This left
     * the right portion of the clef permanently erased after the
     * cursor passed over it.  We now use the full CLEF_W.
     */
    for (s = 0; s < NUM_STAFFS; s++) {
        int clef_x = STAFF_LEFT - CLEF_W - 2;
        int clef_y = staff_top[s] - 15;

        /* AABB overlap test — both rectangles must intersect */
        if (tx          <= clef_x + CLEF_W &&
            tx + ARROW_W >= clef_x          &&
            ty          <= clef_y + CLEF_H  &&
            ty + ARROW_H >= clef_y)
        {
            draw_treble_clef(clef_x, clef_y);
        }
    }
}

/* ── Note dot ─────────────────────────────────────────────────────────── */
void draw_dot(int cx, int cy)
{
    int dx, dy;
    if (cy < MENUBAR_H) return;   /* never place notes in the menu bar */
    for (dy = -DOT_R; dy <= DOT_R; dy++)
        for (dx = -DOT_R; dx <= DOT_R; dx++)
            if (dx*dx + dy*dy <= DOT_R*DOT_R)
                plot_pixel(cx + dx, cy + dy, BLACK);
}

/* ── Button hit-test ──────────────────────────────────────────────────── */
int button_hit(int x, int y)
{
    if (y < BTN_Y || y >= BTN_Y + BTN_H) return -1;
    if (x >= BTN_PLAY_X   && x < BTN_PLAY_X   + BTN_W) return 0;
    if (x >= BTN_STOP_X   && x < BTN_STOP_X   + BTN_W) return 1;
    if (x >= BTN_SPD_UP_X && x < BTN_SPD_UP_X + BTN_W) return 2;
    if (x >= BTN_SPD_DN_X && x < BTN_SPD_DN_X + BTN_W) return 3;
    return -1;
}
