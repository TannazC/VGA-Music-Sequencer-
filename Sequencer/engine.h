/*
 * engine.h — Sequencer playback engine interface
 *
 * Responsibilities:
 *   - Define sequencer state enum: SEQ_STOPPED, SEQ_PLAYING
 *   - Declare engine_init(int bpm) — sets up timer and initial BPM, state = STOPPED
 *   - Declare engine_play() — sets state to PLAYING, starts timer
 *   - Declare engine_pause() — sets state to STOPPED, stops timer
 *   - Declare engine_toggle_play() — toggles between PLAYING and STOPPED
 *   - Declare engine_step() — advances playhead one column forward, wraps at NUM_STEPS
 *       Called by the main loop when timer_tick == 1
 *       Triggers audio synthesis for the new column via synth.c + audio.c
 *       Triggers VGA redraw for old and new playhead columns via draw.c
 *   - Declare engine_bpm_up() / engine_bpm_down() — increment/decrement BPM by 5,
 *       clamp to [60, 180], call timer_set_bpm()
 *   - Declare engine_get_playhead() — returns current step column index (0–15)
 *   - Declare engine_get_bpm() — returns current BPM
 *   - Declare engine_get_state() — returns SEQ_PLAYING or SEQ_STOPPED
 *
 * Notes:
 *   - engine_step() is the central tick handler; it orchestrates audio + VGA updates
 *   - The main loop in main.c should check timer_tick, call engine_step(), then clear tick
 */
