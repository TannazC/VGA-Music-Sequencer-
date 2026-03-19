#ifndef BACKGROUND_H
#define BACKGROUND_H

/* Draw the grey menu bar and its coloured buttons (pixel layer only). */
void draw_menubar(void);

/* Write PLAY/STOP/SPD+/SPD- labels into the character buffer. */
void draw_menubar_labels(void);

/* Draw all 4 × 5 staff lines. */
void draw_staff_lines(void);

/*
 * Draw a treble clef with its top-left corner at (ox, oy).
 * The glyph is CLEF_W × CLEF_H pixels; only black pixels are plotted
 * (white pixels are transparent so the staff lines show through).
 */
void draw_treble_clef(int ox, int oy);

/*
 * Full-screen background: clears to white, draws menu bar,
 * staff lines, treble clefs, then writes the char-buffer labels.
 */
void draw_background(void);

#endif /* BACKGROUND_H */
