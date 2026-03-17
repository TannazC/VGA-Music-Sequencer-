/*
 * audio.c — Audio FIFO implementation
 *
 * Implement everything declared in audio.h:
 *
 *   audio_init()
 *     - Write to AUDIO_BASE + AUDIO_CONTROL to clear FIFOs and enable the core
 *     - Read back to confirm ready
 *
 *   audio_write_samples(int32_t *buf, int num_samples)
 *     - For i = 0 to num_samples-1:
 *         Poll audio_fifo_space() until at least 1 slot is available
 *         Write buf[i] to AUDIO_BASE + AUDIO_LEFT
 *         Write buf[i] to AUDIO_BASE + AUDIO_RIGHT (mono: same sample both channels)
 *
 *   audio_fifo_space()
 *     - Read AUDIO_BASE + AUDIO_FIFOSPACE
 *     - Extract and return bits [23:16] (left channel available space)
 *
 *   audio_clear()
 *     - Write the clear bit to AUDIO_BASE + AUDIO_CONTROL
 *     - Poll until FIFOs report full space (empty)
 *
 * Notes:
 *   - Avoid writing more samples than FIFO depth per step to prevent blocking
 *     the main loop for too long; cap at AUDIO_FIFO_DEPTH defined in notes.h
 */
