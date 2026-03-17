/*
 * synth.h — Waveform synthesis interface
 *
 * Responsibilities:
 *   - Define waveform type enum: WAVE_SQUARE, WAVE_TRIANGLE, WAVE_PULSE
 *   - Declare synth_set_waveform(wave_type) — sets the global waveform style
 *   - Declare synth_generate(row, num_samples, int32_t *buf)
 *       Fills buf with num_samples of the current waveform at the frequency
 *       corresponding to the given row (looked up from notes.h NOTE_FREQ)
 *   - Declare synth_silence(num_samples, int32_t *buf)
 *       Fills buf with zeros (used for steps with no active notes)
 *   - Declare synth_mix(rows[], num_active, num_samples, int32_t *buf)
 *       Generates and sums waveforms for multiple simultaneously active rows
 *       in a single step column; normalizes to avoid clipping
 *
 * Waveform generation approach (implement in synth.c):
 *   - Square: +AMPLITUDE for first half of period, -AMPLITUDE for second half
 *   - Triangle: linearly ramp up then down over one period
 *   - Pulse: +AMPLITUDE for first quarter, -AMPLITUDE for rest (narrower duty cycle)
 *   - Period in samples = SAMPLE_RATE / freq
 *   - Maintain a per-channel phase accumulator to avoid discontinuities
 *
 * Notes:
 *   - AMPLITUDE should be scaled so that 8 simultaneous notes don't clip
 *     (e.g., AMPLITUDE = INT16_MAX / NUM_ROWS)
 *   - The DE1-SoC codec expects 32-bit left-justified samples (upper 24 bits used)
 */
