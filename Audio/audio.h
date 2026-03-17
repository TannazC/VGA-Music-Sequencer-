/*
 * audio.h — Audio FIFO interface for the DE1-SoC codec
 *
 * Responsibilities:
 *   - Define AUDIO_BASE address (from config.h)
 *   - Define register offsets:
 *       AUDIO_CONTROL  0x0  (read: space in left/right FIFOs; write: clear/enable)
 *       AUDIO_FIFOSPACE 0x4 (bits [23:16] = left FIFO space, [7:0] = right FIFO space)
 *       AUDIO_LEFT     0x8  (write: push one 32-bit sample to left channel)
 *       AUDIO_RIGHT    0xC  (write: push one 32-bit sample to right channel)
 *   - Declare audio_init() — enables the audio core, clears FIFOs
 *   - Declare audio_write_samples(int32_t *buf, int num_samples)
 *       Writes num_samples from buf to both left and right channels
 *       Blocks (polls FIFO space) until all samples are written
 *   - Declare audio_fifo_space() — returns available space in left FIFO
 *   - Declare audio_clear() — flushes both FIFOs
 *
 * Notes:
 *   - Writing the same sample to both left and right produces mono output
 *   - audio_write_samples is called by engine.c once per step tick
 *   - num_samples per step = SAMPLE_RATE / (BPM / 60) — precompute in engine.c
 */
