/*
 * interrupts.c — Interrupt setup and ISR dispatcher implementation
 *
 * Implement everything declared in interrupts.h:
 *
 *   interrupts_init()
 *     - Use inline asm to set the mie CSR bit for the timer interrupt line
 *     - Use inline asm to set mstatus.MIE to globally enable interrupts
 *     - Reference: DE1-SoC_Computer_NiosV.pdf Listing 6 for exact CSR instructions
 *
 *   irq_handler() [marked __attribute__((interrupt))]
 *     - Read mcause to confirm it is an external interrupt (bit 31 set)
 *     - Read mip or the platform interrupt controller to identify which device fired
 *     - Dispatch to timer_isr() or ps2_isr() accordingly
 *     - Must be registered as the machine-mode trap handler (set mtvec CSR to its address)
 *
 *   enable/disable_timer_interrupt()
 *     - Set/clear the timer IRQ bit in the mie CSR
 *
 *   enable/disable_ps2_interrupt()
 *     - Set/clear the PS/2 IRQ bit in the mie CSR
 *     - Also write to the PS/2 control register (PS2_BASE + 4) to enable/disable the interrupt at source
 *
 * Notes:
 *   - If using polling instead of interrupts for PS/2, only timer interrupt setup is needed here
 *   - Keep ISRs short: set a flag, clear the interrupt source, return — no VGA or audio calls inside ISRs
 */
