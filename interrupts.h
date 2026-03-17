/*
 * interrupts.h — Interrupt enable/disable and ISR registration interface
 *
 * Responsibilities:
 *   - Declare interrupts_init() — enables global interrupts using inline assembly
 *       (csrsi mstatus, 0x8 for Nios V, or use the asm wrapper from DE1-SoC_Computer_NiosV.pdf Listing 6)
 *   - Declare the top-level IRQ dispatcher: void __attribute__((interrupt)) irq_handler(void)
 *       This is the single entry point for all interrupts on Nios V
 *       It reads the mcause/mip CSR to identify the source, then dispatches to:
 *         - timer_isr()   if timer interrupt pending
 *         - ps2_isr()     if PS/2 interrupt pending (if using interrupt-driven PS/2)
 *   - Declare enable_timer_interrupt() and disable_timer_interrupt()
 *   - Declare enable_ps2_interrupt() and disable_ps2_interrupt()
 *
 * Notes:
 *   - Nios V uses a different interrupt model than Nios II — do NOT use Nios II HAL
 *     alt_irq_register(); instead follow the Nios V CSR approach in the course document
 *   - Listing 6 in DE1-SoC_Computer_NiosV.pdf shows the exact asm sequence to enable
 *     interrupts; copy it verbatim and adapt for timer and PS/2 IRQ lines
 *   - IRQ numbers: timer = IRQ0, PS/2 = IRQ7 (verify in DE1-SoC_Computer_NiosV.pdf)
 */
