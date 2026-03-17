/*
 * ps2.h — PS/2 keyboard and mouse driver interface
 *
 * Responsibilities:
 *   - Define the PS/2 base address register and bit masks
 *   - Declare functions for initializing the PS/2 controller
 *   - Declare functions for reading raw scan codes from the keyboard
 *   - Declare functions for reading mouse movement packets (dx, dy, buttons)
 *   - Declare a high-level event struct (KEY_EVENT) that wraps a scan code
 *     into an action: KEY_PRESS, KEY_RELEASE
 *   - Declare mappings from scan codes to logical keys:
 *       arrow keys (UP, DOWN, LEFT, RIGHT)
 *       SPACE (toggle note), ENTER (play/pause), +/- (BPM), ESC (clear)
 *   - Declare the PS/2 ISR handler signature (if using interrupt-driven input)
 *   - Declare a polling function for use in the main loop (non-interrupt mode)
 *
 * Mouse (function TBD):
 *   - Declare mouse_init() to send the 0xF4 enable-streaming command
 *   - Declare mouse_read_packet() returning a MOUSE_PACKET struct
 *     with fields: left_btn, right_btn, dx, dy
 *   - Mouse cursor position tracking will be handled in ui.c using these deltas
 *
 * Notes:
 *   - PS/2 base address to be defined in config.h (PS2_BASE)
 *   - Keyboard uses set-2 scan codes; extended codes prefixed with 0xE0
 *   - Y-splitter required on hardware to use both keyboard and mouse simultaneously
 */
