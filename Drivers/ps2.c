/*
 * ps2.c — PS/2 keyboard and mouse driver implementation
 *
 * Implement everything declared in ps2.h:
 *
 *   ps2_init()
 *     - Write to the PS/2 control register to enable interrupts (if ISR mode)
 *       or leave in polling mode
 *     - Send 0xF4 to mouse to enable streaming (call after keyboard init)
 *
 *   ps2_read_scancode()
 *     - Poll the PS/2 data register (PS2_BASE + 0)
 *     - Check the RVALID bit (bit 15) before reading
 *     - Return the 8-bit scan code, or 0 if no data ready
 *     - Handle 0xE0 prefix: set a flag and read the next byte as an extended code
 *     - Handle 0xF0 prefix: mark the next code as a key-release
 *
 *   ps2_get_key_event(KEY_EVENT *evt)
 *     - Call ps2_read_scancode() and translate to a KEY_EVENT
 *     - Fill evt->key (logical key enum) and evt->type (PRESS or RELEASE)
 *     - Return 1 if a complete event was decoded, 0 if still mid-sequence
 *
 *   mouse_init()
 *     - Write 0xFF (reset) then 0xF4 (enable streaming) to PS/2 data register
 *     - Wait for 0xAA acknowledge between commands
 *
 *   mouse_read_packet(MOUSE_PACKET *pkt)
 *     - Read 3 consecutive bytes from PS/2 data register
 *     - Parse byte 0 for button bits, bytes 1-2 for dx/dy with sign extension
 *     - Return 1 if a full packet was available, 0 otherwise
 *
 *   ps2_isr() [if interrupt-driven]
 *     - Read one byte from PS/2 data register
 *     - Push it into a small circular buffer
 *     - Clear the interrupt in the PS/2 control register
 */
