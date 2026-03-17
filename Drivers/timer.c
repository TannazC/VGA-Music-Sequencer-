/*
 * timer.c — Interval timer driver implementation
 *
 * Implement everything declared in timer.h:
 *
 *   volatile int timer_tick = 0
 *     - Global flag; set to 1 by ISR each time a step period elapses
 *     - engine.c polls this flag in the main loop and clears it after processing
 *
 *   timer_init(int bpm)
 *     - Compute period = CLOCK_FREQ / (bpm / 60): number of clock cycles per step
 *     - Write low 16 bits to TIMER_BASE + PERIOD_LO, high 16 bits to TIMER_BASE + PERIOD_HI
 *     - Write CONT | ITO to TIMER_BASE + CONTROL (continuous + interrupt on timeout)
 *     - Write START to TIMER_BASE + CONTROL to begin counting
 *
 *   timer_set_bpm(int bpm)
 *     - Clamp bpm to [60, 180]
 *     - Recompute period and write to PERIOD_LO / PERIOD_HI
 *     - Timer reloads the new period on next timeout (no need to restart)
 *
 *   timer_start() / timer_stop()
 *     - Write START or STOP bit to TIMER_BASE + CONTROL
 *
 *   timer_clear_interrupt()
 *     - Write 0 to TIMER_BASE + STATUS to clear the TO (timeout) bit
 *
 *   timer_isr()
 *     - Set timer_tick = 1
 *     - Call timer_clear_interrupt()
 *     - Must be registered in interrupts.c as the handler for the timer IRQ line
 */
