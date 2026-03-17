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

 #include "ui.h"
#include "pattern.h"
#include "engine.h"
#include "../graphics/draw.h"
#include "../drivers/ps2.h"
#include "../config.h"

/* ── Cursor state ────────────────────────────────────────────────────────────
 * The cursor is a visual indicator of the current keyboard-selected cell.
 * It moves independently of the playhead and is always visible.
 */
static int cursor_col = 0;
static int cursor_row = 0;

/* ── Mouse state ─────────────────────────────────────────────────────────────
 * Accumulated screen position of the mouse pointer (pixel coordinates).
 * Clamped to [0, SCREEN_W-1] x [0, SCREEN_H-1].
 * right_btn_prev tracks the previous packet's button state so we only act
 * on a rising edge (button just went from 0→1) rather than every packet
 * while the button is held.
 */
static int mouse_x       = 0;
static int mouse_y       = 0;
static int right_prev    = 0;  /* previous right-button state for edge detect */

/* ── ui_init ─────────────────────────────────────────────────────────────────
 * Call once after draw_grid() so the initial cursor is visible.
 */
void ui_init(void) {
    cursor_col = 0;
    cursor_row = 0;
    mouse_x    = SCREEN_W / 2;
    mouse_y    = SCREEN_H / 2;
    right_prev = 0;
    draw_cursor(cursor_col, cursor_row);
}

/* ── ui_handle_key ───────────────────────────────────────────────────────────
 * Called from the main loop when ps2_get_key_event() returns 1.
 *
 * We only act on KEY_PRESS events.  KEY_RELEASE events are ignored here
 * because the break-code guard in ps2.c already ensures KEY_PRESS fires
 * exactly once per physical keypress — no repeat, no hold-flooding.
 *
 * Actions:
 *   Arrow keys  → move cursor; redraw old cell (restore note state) and new cursor
 *   SPACE       → toggle note at cursor position; redraw that cell
 *   ENTER       → play / pause the sequencer
 *   +/-         → BPM up / down
 *   ESC         → clear the entire pattern and redraw the grid
 */
void ui_handle_key(KEY_EVENT evt) {
    if (evt.type != KEY_PRESS)
        return;

    int prev_col = cursor_col;
    int prev_row = cursor_row;

    switch (evt.key) {

        case KEY_UP:
            if (cursor_row > 0) cursor_row--;
            break;

        case KEY_DOWN:
            if (cursor_row < NUM_ROWS - 1) cursor_row++;
            break;

        case KEY_LEFT:
            if (cursor_col > 0) cursor_col--;
            break;

        case KEY_RIGHT:
            if (cursor_col < NUM_STEPS - 1) cursor_col++;
            break;

        case KEY_SPACE:
            /* Toggle note at the current cursor cell */
            pattern_toggle(cursor_col, cursor_row);
            draw_cell(cursor_col, cursor_row,
                      pattern_get(cursor_col, cursor_row) ? NOTE_ON : NOTE_OFF);
            /* Cursor stays in place — no position change, skip redraw below */
            return;

        case KEY_ENTER:
            engine_toggle_play();
            draw_status_bar();
            return;

        case KEY_PLUS:
            engine_bpm_up();
            return;

        case KEY_MINUS:
            engine_bpm_down();
            return;

        case KEY_ESC:
            pattern_clear();
            draw_grid();
            draw_cursor(cursor_col, cursor_row);
            return;

        default:
            return;
    }

    /* Cursor moved — restore previous cell appearance, draw new cursor */
    if (cursor_col != prev_col || cursor_row != prev_row) {
        /* Redraw the cell the cursor just left (back to its note state) */
        draw_cell(prev_col, prev_row,
                  pattern_get(prev_col, prev_row) ? NOTE_ON : NOTE_OFF);
        /* Draw cursor outline on the new cell */
        draw_cursor(cursor_col, cursor_row);
    }
}

/* ── ui_handle_mouse ─────────────────────────────────────────────────────────
 * Called from the main loop when mouse_read_packet() returns 1.
 *
 * 1. Accumulate dx/dy into mouse_x/mouse_y (clamped to screen bounds).
 * 2. Right-click rising edge → convert pixel position to grid (col, row)
 *    and toggle the note at that cell.
 *
 * Grid hit-test:
 *   The sequencer grid starts at (GRID_ORIGIN_X, GRID_ORIGIN_Y) and each
 *   cell is CELL_W × CELL_H pixels (constants from draw.h).
 *   col = (mouse_x - GRID_ORIGIN_X) / CELL_W
 *   row = (mouse_y - GRID_ORIGIN_Y) / CELL_H
 *   If the computed col/row is outside [0, NUM_STEPS-1] / [0, NUM_ROWS-1]
 *   the click was outside the grid and is ignored.
 *
 * Rising-edge detection:
 *   We only act when right_btn transitions 0→1.
 *   While the button is held across multiple packets we do nothing,
 *   preventing the same cell from being toggled back and forth rapidly.
 */
void ui_handle_mouse(MOUSE_PACKET pkt) {
    /* ── 1. Update accumulated mouse position ── */
    mouse_x += pkt.dx;
    mouse_y += pkt.dy;

    /* Clamp to screen */
    if (mouse_x < 0)          mouse_x = 0;
    if (mouse_x >= SCREEN_W)  mouse_x = SCREEN_W - 1;
    if (mouse_y < 0)          mouse_y = 0;
    if (mouse_y >= SCREEN_H)  mouse_y = SCREEN_H - 1;

    /* ── 2. Right-click rising-edge detection ── */
    int right_now = pkt.right_btn;

    if (right_now && !right_prev) {
        /* Button just pressed — hit-test against the grid */
        int col = (mouse_x - GRID_ORIGIN_X) / CELL_W;
        int row = (mouse_y - GRID_ORIGIN_Y) / CELL_H;

        if (col >= 0 && col < NUM_STEPS &&
            row >= 0 && row < NUM_ROWS) {
            /* Valid cell — toggle note */
            pattern_toggle(col, row);
            draw_cell(col, row,
                      pattern_get(col, row) ? NOTE_ON : NOTE_OFF);
        }
        /* Click outside the grid is silently ignored */
    }

    right_prev = right_now;
}
