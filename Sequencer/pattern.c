/*
 * pattern.c — Pattern memory implementation
 *
 * Implement everything declared in pattern.h:
 *
 *   uint8_t pattern[NUM_ROWS][NUM_STEPS]
 *     - Defined here as a global; initialized to all zeros
 *
 *   pattern_init() / pattern_clear()
 *     - memset(pattern, 0, sizeof(pattern))
 *
 *   pattern_toggle(int col, int row)
 *     - Bounds-check col in [0, NUM_STEPS-1] and row in [0, NUM_ROWS-1]
 *     - pattern[row][col] ^= 1
 *
 *   pattern_get(int col, int row)
 *     - Return pattern[row][col] (return 0 if out of bounds)
 *
 *   pattern_set(int col, int row, int val)
 *     - pattern[row][col] = val ? 1 : 0
 *
 *   pattern_get_active_rows(int col, int rows_out[], int *count)
 *     - Loop over all NUM_ROWS
 *     - For each row where pattern[row][col] == 1, append row to rows_out
 *     - Set *count to total found
 *     - Called by engine.c on every step tick before audio synthesis
 */
