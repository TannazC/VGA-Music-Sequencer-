/*
 * main.c — Entry point: hardware init, main loop
 *
 * Responsibilities:
 *   Init sequence (in order):
 *     1. audio_init()            — enable audio core, flush FIFOs
 *     2. ps2_init()              — initialize keyboard; call mouse_init() if using mouse
 *     3. timer_init(DEFAULT_BPM) — set up BPM timer (does not start it yet)
 *     4. interrupts_init()       — enable timer (and PS/2) interrupts, set mtvec
 *     5. pattern_init()          — zero the note grid
 *     6. bg_draw()               — blit background image to VGA pixel buffer
 *     7. draw_grid()             — draw sequencer grid on top of background
 *     8. draw_labels()           — draw note names and step numbers
 *     9. draw_status_bar()       — draw initial BPM, waveform, and PAUSED state
 *    10. ui_init()               — place cursor at (0, 0)
 *
 *   Main loop (infinite):
 *     - Check timer_tick (set by timer ISR):
 *         If timer_tick == 1 and seq_state == SEQ_PLAYING:
 *           Call engine_step()
 *           Clear timer_tick = 0
 *     - Poll PS/2 for key event:
 *         Call ps2_get_key_event(&evt)
 *         If event ready: call ui_handle_key(evt)
 *     - Poll PS/2 for mouse packet (once mouse function is decided):
 *         Call mouse_read_packet(&pkt)
 *         If packet ready: call ui_handle_mouse(pkt)
 *
 * Notes:
 *   - No blocking calls in the main loop — everything is poll/flag based
 *   - The only side effects of the main loop are through ui.c, engine.c, and draw.c
 *   - Do not put hardware register accesses directly in main.c
 */
