/*
 * draw.h — High-level grid, cursor, and playhead drawing interface
 *
 * Responsibilities:
 *   - Declare draw_grid() — renders the full 8-row × 16-step sequencer grid
 *       Each cell is a rectangle; color depends on note-on/off and whether
 *       the cell is in the active playhead column
 *   - Declare draw_cell(col, row, state) — redraws a single cell
 *       state: NOTE_OFF, NOTE_ON, CURSOR, PLAYHEAD, CURSOR_ON_NOTE
 *   - Declare draw_cursor(col, row) — draws the cursor outline on the current cell
 *   - Declare draw_playhead(col) — highlights the entire active column
 *   - Declare draw_clear_playhead(col) — restores the previous playhead column
 *   - Declare draw_labels() — draws row note labels (C4, D4, ... B4) on the left
 *     and step numbers (1–16) along the top
 *   - Declare draw_status_bar() — draws the bottom HUD:
 *       current BPM value, instrument/waveform name, play/pause state
 *   - Define cell dimension constants: CELL_W, CELL_H, GRID_ORIGIN_X, GRID_ORIGIN_Y
 *   - Define color constants for each cell state (NOTE_ON_COLOR, PLAYHEAD_COLOR, etc.)
 *
 * Notes:
 *   - draw_grid() is called once at startup; thereafter only draw_cell() is used
 *     to avoid redrawing the entire screen every frame
 *   - All drawing calls go through vga.h functions
 */
