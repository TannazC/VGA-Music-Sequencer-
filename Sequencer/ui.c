/*
 * ui.c — User input handling: cursor movement, note editing, transport controls
 *
 * Responsibilities:
 *   - Maintain cursor position: cursor_col (0–15), cursor_row (0–7)
 *   - Declare and implement ui_init() — set cursor to (0, 0), draw initial cursor
 *   - Declare and implement ui_handle_key(KEY_EVENT evt):
 *       Called from main loop whenever ps2_get_key_event() returns a KEY_PRESS
 *       Switch on evt.key:
 *         ARROW_UP    → move cursor up (cursor_row--), clamp to 0
 *         ARROW_DOWN  → move cursor down (cursor_row++), clamp to NUM_ROWS-1
 *         ARROW_LEFT  → move cursor left (cursor_col--), clamp to 0
 *         ARROW_RIGHT → move cursor right (cursor_col++), clamp to NUM_STEPS-1
 *         SPACE       → call pattern_toggle(cursor_col, cursor_row),
 *                        call draw_cell(cursor_col, cursor_row, new_state)
 *         ENTER       → call engine_toggle_play(), call draw_status_bar()
 *         KEY_PLUS    → call engine_bpm_up()
 *         KEY_MINUS   → call engine_bpm_down()
 *         KEY_ESC     → call pattern_clear(), call draw_grid()
 *         (future)    → mouse click: translate (px, py) to (col, row) and toggle
 *   - After any cursor move: call draw_cursor(new_col, new_row) and
 *     redraw the previous cell without cursor outline
 *   - Declare ui_handle_mouse(MOUSE_PACKET pkt):
 *       Function TBD — will use mouse dx/dy to move cursor or click to toggle notes
 *       Stub this out now; implement once mouse function is decided
 *
 * Notes:
 *   - ui.c does not talk to hardware directly; it only calls pattern.h, engine.h, draw.h
 *   - Cursor and playhead are independent — cursor can be anywhere during playback
 */
