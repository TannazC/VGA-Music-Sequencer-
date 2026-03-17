/*
 * engine.c — Sequencer playback engine implementation
 *
 * Implement everything declared in engine.h:
 *
 *   Global state:
 *     - int playhead = 0          current step column (0–15)
 *     - int current_bpm           current BPM value
 *     - int seq_state             SEQ_STOPPED or SEQ_PLAYING
 *     - int32_t sample_buf[]      reusable buffer for audio samples per step
 *
 *   engine_init(int bpm)
 *     - Set current_bpm, call timer_init(bpm), set state to STOPPED
 *     - Initialize sample_buf size = SAMPLE_RATE / (bpm / 60.0), capped at AUDIO_FIFO_DEPTH
 *
 *   engine_play() / engine_pause()
 *     - Set seq_state, call timer_start() or timer_stop()
 *
 *   engine_step()
 *     - Save prev_playhead = playhead
 *     - Advance playhead = (playhead + 1) % NUM_STEPS
 *     - Call draw_clear_playhead(prev_playhead) and draw_playhead(playhead)
 *     - Call pattern_get_active_rows(playhead, rows, &count)
 *     - If count > 0: call synth_mix(rows, count, num_samples, sample_buf)
 *       Else: call synth_silence(num_samples, sample_buf)
 *     - Call audio_write_samples(sample_buf, num_samples)
 *
 *   engine_bpm_up() / engine_bpm_down()
 *     - current_bpm += or -= 5, clamp to [60, 180]
 *     - Call timer_set_bpm(current_bpm)
 *     - Recalculate num_samples per step
 *     - Call draw_status_bar() to update BPM display
 */
