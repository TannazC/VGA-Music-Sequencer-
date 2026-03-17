/*
 * config.h — Central hardware addresses and project-wide constants
 *
 * Responsibilities:
 *   - Define all DE1-SoC memory-mapped hardware base addresses:
 *       VGA_PIXEL_BASE    0xC8000000   (pixel buffer)
 *       VGA_CHAR_BASE     0xC9000000   (character buffer)
 *       PS2_BASE          0xFF200100   (PS/2 port)
 *       TIMER_BASE        0xFF202000   (interval timer)
 *       AUDIO_BASE        0xFF203040   (audio core)
 *   - Define CLOCK_FREQ 100000000 (100 MHz system clock)
 *   - Define DEFAULT_BPM 120
 *   - Define BPM_MIN 60, BPM_MAX 180, BPM_STEP 5
 *   - Define NUM_STEPS 16, NUM_ROWS 8
 *   - Define SAMPLE_RATE 8000
 *
 * Notes:
 *   - All other source files should #include "config.h" instead of hardcoding addresses
 *   - Verify base addresses against DE1-SoC_Computer_NiosV.pdf Section 2/3 before use
 *   - AUDIO_BASE may differ depending on which audio core variant is instantiated —
 *     check the .sopcinfo or the NiosV system document
 */
