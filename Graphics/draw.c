/*
 * draw.c — High-level grid, cursor, and playhead drawing implementation
 *
 * Implement everything declared in draw.h:
 *
 *   draw_grid()
 *     - Iterate over all 8 rows and 16 columns
 *     - For each cell, call vga_draw_rect with the appropriate color
 *       based on pattern_get(col, row) from sequencer/pattern.h
 *     - Draw 1-pixel borders between cells using a darker color
 *     - Call draw_labels() after filling cells
 *
 *   draw_cell(int col, int row, int state)
 *     - Compute pixel coords: x = GRID_ORIGIN_X + col*CELL_W, y = GRID_ORIGIN_Y + row*CELL_H
 *     - Fill inner area with color for the given state
 *     - Redraw cell border
 *
 *   draw_cursor(int col, int row)
 *     - Draw a 2-pixel border in CURSOR_COLOR around the cell without filling it
 *     - Previous cursor position must be cleared by the caller before moving
 *
 *   draw_playhead(int col) / draw_clear_playhead(int col)
 *     - Redraw all 8 cells in the column with/without playhead highlight
 *     - Preserve NOTE_ON/NOTE_OFF fill; only change the background tint
 *
 *   draw_labels()
 *     - Write note names (C4–B4) in the left margin using vga_draw_string
 *     - Write step numbers (1–16) along the top margin
 *
 *   draw_status_bar()
 *     - Write "BPM: XXX", waveform name, and "PLAY"/"PAUSE" to the bottom
 *       strip of the screen using vga_draw_string
 */
