#ifndef CURSOR_H
#define CURSOR_H

/*
 * Draw / erase the arrow cursor with its tip at (tx, ty).
 * erase_arrow also redraws any treble clef the cursor was covering.
 */
void draw_arrow(int tx, int ty, short int c);
void erase_arrow(int tx, int ty);

/* Draw a filled black dot (note) centred at (cx, cy). */
void draw_dot(int cx, int cy);

/*
 * Hit-test the menu buttons.
 * Returns: 0=PLAY, 1=STOP, 2=SPD+, 3=SPD-,  -1=miss
 */
int button_hit(int x, int y);

#endif /* CURSOR_H */
