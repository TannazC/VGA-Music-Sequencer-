/*
 * synth.c — Waveform synthesis implementation
 *
 * Implement everything declared in synth.h:
 *
 *   Global state:
 *     - current_waveform: WAVE_SQUARE / WAVE_TRIANGLE / WAVE_PULSE
 *     - phase[NUM_ROWS]: float phase accumulator per row (0.0 to 1.0)
 *
 *   synth_set_waveform(int wave_type)
 *     - Set current_waveform; no other state change needed
 *
 *   synth_generate(int row, int num_samples, int32_t *buf)
 *     - Compute freq = NOTE_FREQ[row], period_samples = SAMPLE_RATE / freq
 *     - Loop num_samples times:
 *         advance phase[row] by (1.0 / period_samples)
 *         wrap phase[row] when >= 1.0
 *         compute sample value based on current_waveform and phase[row]
 *         write sample to buf[i] (left-justify for 32-bit codec format)
 *
 *   synth_silence(int num_samples, int32_t *buf)
 *     - memset(buf, 0, num_samples * sizeof(int32_t))
 *
 *   synth_mix(int rows[], int num_active, int num_samples, int32_t *buf)
 *     - Allocate a temporary buffer per active row
 *     - Call synth_generate for each active row into its temp buffer
 *     - Sum all temp buffers into buf, then divide by num_active to normalize
 *     - If num_active == 0, call synth_silence instead
 */
