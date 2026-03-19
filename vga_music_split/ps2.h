#ifndef PS2_H
#define PS2_H

/*
 * Read one byte from the PS/2 FIFO without blocking.
 * Returns the byte value (0-255) or -1 if the FIFO is empty.
 */
int ps2_read_byte(volatile int *ps2);

/*
 * Write one byte to the PS/2 port and wait (with timeout) for the
 * 0xFA ACK from the device.
 */
void ps2_send_byte(volatile int *ps2, unsigned char b);

/*
 * Full mouse initialisation sequence:
 *   flush FIFO → reset (0xFF) → wait for BAT (0xAA) + ID (0x00)
 *   → flush → enable streaming (0xF4) → flush
 *
 * Call this BEFORE draw_background() so the FIFO never overflows
 * while the background is being rendered.
 */
void mouse_init(void);

#endif /* PS2_H */
