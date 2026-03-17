/*
 * notes.h — Note frequency lookup table
 *
 * Responsibilities:
 *   - Define NUM_ROWS 8 (rows in the sequencer grid)
 *   - Define a float array NOTE_FREQ[NUM_ROWS] mapping each row index
 *     to its frequency in Hz, from bottom to top of the grid:
 *       Row 0 (bottom) = B4  = 493.88 Hz
 *       Row 1          = A4  = 440.00 Hz
 *       Row 2          = G4  = 392.00 Hz
 *       Row 3          = F4  = 349.23 Hz
 *       Row 4          = E4  = 329.63 Hz
 *       Row 5          = D4  = 293.66 Hz
 *       Row 6          = C4  = 261.63 Hz
 *       Row 7 (top)    = B3  = 246.94 Hz
 *     (adjust row-to-note mapping based on your grid layout preference)
 *   - Define NOTE_NAMES[NUM_ROWS] as a string array for VGA labels:
 *       {"B4", "A4", "G4", "F4", "E4", "D4", "C4", "B3"}
 *   - Define SAMPLE_RATE 8000 (Hz) — matches DE1-SoC audio codec default
 *   - Define AUDIO_FIFO_DEPTH 128 — maximum samples to write per step
 *
 * Notes:
 *   - Row-to-note ordering (low row index = high pitch or low pitch) should
 *     match the visual layout chosen in draw.c
 *   - Frequencies can be extended to a full octave or two if stretch goals allow
 */
