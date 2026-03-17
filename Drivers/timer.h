/*
 * timer.h — Interval timer driver interface for BPM-based step clock
 *
 * Responsibilities:
 *   - Define the interval timer base address (from config.h: TIMER_BASE)
 *   - Define register offsets: STATUS (0x0), CONTROL (0x4), PERIOD_LO (0x8), PERIOD_HI (0xC)
 *   - Define control bit masks: START, STOP, CONT (continuous mode), ITO (interrupt on timeout)
 *   - Declare timer_init(bpm) — sets the period register for the given BPM
 *       period = (CLOCK_FREQ / (bpm / 60.0)) counts
 *       CLOCK_FREQ assumed 100 MHz (define in config.h)
 *   - Declare timer_set_bpm(bpm) — recalculates and reloads period without stopping
 *   - Declare timer_start() and timer_stop()
 *   - Declare timer_clear_interrupt() — clears the TO bit in STATUS register
 *   - Declare the timer ISR signature: void timer_isr(void)
 *   - Declare a volatile flag: extern volatile int timer_tick — set by ISR, cleared by engine
 *
 * Notes:
 *   - BPM range: 60–180 BPM (enforce clamp in timer_set_bpm)
 *   - The ISR should be minimal: set timer_tick = 1 and clear the interrupt
 *   - All BPM-to-step logic lives in sequencer/engine.c, not here
 */
