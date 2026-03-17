/*
 * pattern.h — Pattern memory: the 8×16 note grid state
 *
 * Responsibilities:
 *   - Define NUM_STEPS 16 and NUM_ROWS 8 (or reference from notes.h)
 *   - Declare the pattern grid: uint8_t pattern[NUM_ROWS][NUM_STEPS]
 *     where 1 = note on, 0 = note off
 *   - Declare pattern_init() — zeros the entire grid
 *   - Declare pattern_toggle(int col, int row) — flips note on↔off at (col, row)
 *   - Declare pattern_get(int col, int row) — returns 0 or 1
 *   - Declare pattern_set(int col, int row, int val) — directly set a cell
 *   - Declare pattern_clear() — zeros all cells (same as init; used for clear-all key)
 *   - Declare pattern_get_active_rows(int col, int rows_out[], int *count)
 *     Fills rows_out[] with row indices that are ON for the given column;
 *     sets *count to the number of active rows. Used by engine.c to drive audio.
 *
 * Notes:
 *   - Row 0 = top of grid visually, or bottom, depending on draw.c convention —
 *     pick one and be consistent with notes.h NOTE_FREQ ordering
 *   - pattern[] is a global — no dynamic allocation on bare-metal
 */
