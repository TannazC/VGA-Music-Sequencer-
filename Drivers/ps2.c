#include "ps2.h"
#include "../config.h"

/* ── Helpers ─────────────────────────────────────────────────────────────────
 * Volatile pointer reads/writes to memory-mapped registers.
 */
static inline uint32_t reg_read(uint32_t base, uint32_t offset) {
    return *((volatile uint32_t *)(base + offset));
}
static inline void reg_write(uint32_t base, uint32_t offset, uint32_t val) {
    *((volatile uint32_t *)(base + offset)) = val;
}

/* Read one byte from a PS/2 port.
 * Returns the byte (0–255) if RVALID is set, or -1 if the FIFO is empty.
 */
static int ps2_read_byte(uint32_t base) {
    uint32_t data = reg_read(base, PS2_DATA_OFFSET);
    if (data & PS2_RVALID)
        return (int)(data & 0xFF);
    return -1;
}

/* Write one byte to a PS/2 port (used for mouse commands). */
static void ps2_write_byte(uint32_t base, uint8_t byte) {
    reg_write(base, PS2_DATA_OFFSET, (uint32_t)byte);
}

/* ── Keyboard state ──────────────────────────────────────────────────────────
 * We parse a small state machine to handle:
 *   - 0xE0 prefix  (extended keys: arrows)
 *   - 0xF0 prefix  (break / key-release)
 *   - 0xE0 + 0xF0  (extended key release)
 *
 * key_held[] tracks whether each LogicalKey is currently physically held.
 * This is the break-code guard: we only emit KEY_PRESS once per press and
 * KEY_RELEASE once per release, regardless of how long the key is held.
 */
static int kb_extended = 0;   /* non-zero after seeing 0xE0        */
static int kb_breaking  = 0;  /* non-zero after seeing 0xF0        */

/* One entry per LogicalKey enum value (KEY_NONE=0 … KEY_UNKNOWN=10) */
static int key_held[11] = {0};

/* Map a (extended, make_code) pair to a LogicalKey. */
static LogicalKey scancode_to_key(int extended, uint8_t sc) {
    if (extended) {
        switch (sc) {
            case SC_UP:    return KEY_UP;
            case SC_DOWN:  return KEY_DOWN;
            case SC_LEFT:  return KEY_LEFT;
            case SC_RIGHT: return KEY_RIGHT;
            default:       return KEY_UNKNOWN;
        }
    } else {
        switch (sc) {
            case SC_SPACE: return KEY_SPACE;
            case SC_ENTER: return KEY_ENTER;
            case SC_ESC:   return KEY_ESC;
            case SC_PLUS:  return KEY_PLUS;
            case SC_MINUS: return KEY_MINUS;
            default:       return KEY_UNKNOWN;
        }
    }
}

/* ── ps2_init ────────────────────────────────────────────────────────────────
 * Flush the keyboard FIFO and leave the port in polling mode.
 * We do NOT enable interrupts (RE bit stays 0); the main loop polls.
 */
void ps2_init(void) {
    /* Drain any stale bytes sitting in the keyboard FIFO */
    while (ps2_read_byte(PS2_BASE) != -1)
        ;
    kb_extended = 0;
    kb_breaking  = 0;
    int i;
    for (i = 0; i < 11; i++) key_held[i] = 0;
}

/* ── ps2_get_key_event ───────────────────────────────────────────────────────
 * Call once per main-loop iteration.
 *
 * State machine:
 *   byte == 0xE0  → set extended flag, read next byte
 *   byte == 0xF0  → set breaking flag, read next byte
 *   otherwise     → this is the make code; resolve to LogicalKey
 *
 * Break-code guard:
 *   KEY_PRESS  is only emitted if key_held[key] == 0  (not already down)
 *   KEY_RELEASE is emitted when 0xF0 sequence completes, clears key_held
 *
 * Returns 1 if *evt is filled with a usable event, 0 if nothing ready.
 */
int ps2_get_key_event(KEY_EVENT *evt) {
    int byte = ps2_read_byte(PS2_BASE);
    if (byte == -1)
        return 0;  /* FIFO empty */

    if (byte == SC_EXTEND) {
        kb_extended = 1;
        return 0;  /* wait for the actual make code */
    }
    if (byte == SC_BREAK) {
        kb_breaking = 1;
        return 0;  /* wait for the make code that follows 0xF0 */
    }

    /* We have a make code.  Resolve it. */
    LogicalKey key = scancode_to_key(kb_extended, (uint8_t)byte);

    int was_breaking  = kb_breaking;
    kb_extended = 0;
    kb_breaking  = 0;

    if (key == KEY_NONE || key == KEY_UNKNOWN)
        return 0;

    if (was_breaking) {
        /* Key released */
        key_held[key] = 0;
        evt->key  = key;
        evt->type = KEY_RELEASE;
        return 1;
    } else {
        /* Key pressed — only fire once until released (break-code guard) */
        if (key_held[key])
            return 0;  /* still held from last press; suppress repeat */
        key_held[key] = 1;
        evt->key  = key;
        evt->type = KEY_PRESS;
        return 1;
    }
}

/* ── Mouse state ─────────────────────────────────────────────────────────────
 * The PS/2 mouse sends 3-byte packets in streaming mode:
 *
 *   Byte 0:  [7]=Y overflow [6]=X overflow [5]=Y sign [4]=X sign
 *            [3]=1 (always) [2]=middle btn [1]=right btn [0]=left btn
 *   Byte 1:  X movement (magnitude; sign in byte 0 bit 4)
 *   Byte 2:  Y movement (magnitude; sign in byte 0 bit 5)
 *
 * We accumulate bytes into a 3-byte buffer and only return a packet
 * when all 3 bytes have arrived.
 */
static uint8_t mouse_buf[3];
static int     mouse_buf_idx = 0;

/* ── mouse_init ──────────────────────────────────────────────────────────────
 * Send reset + enable-streaming to the mouse port and drain the response.
 * The mouse responds:  0xFA (ack), 0xAA (BAT ok), 0x00 (device ID)
 * after reset, then 0xFA after 0xF4.
 * We poll-wait for each expected response byte with a simple spin limit.
 */
void mouse_init(void) {
    int i, byte;

    /* Flush any stale data on the mouse port */
    for (i = 0; i < 32; i++) ps2_read_byte(PS2_BASE2);

    /* Send 0xFF reset */
    ps2_write_byte(PS2_BASE2, 0xFF);
    /* Wait for 0xFA (ack) */
    for (i = 0; i < 100000; i++) {
        byte = ps2_read_byte(PS2_BASE2);
        if (byte == 0xFA) break;
    }
    /* Wait for 0xAA (self-test passed) */
    for (i = 0; i < 100000; i++) {
        byte = ps2_read_byte(PS2_BASE2);
        if (byte == 0xAA) break;
    }
    /* Drain device-ID byte (0x00) */
    for (i = 0; i < 100000; i++) {
        byte = ps2_read_byte(PS2_BASE2);
        if (byte != -1) break;
    }

    /* Send 0xF4 enable streaming */
    ps2_write_byte(PS2_BASE2, 0xF4);
    /* Wait for 0xFA ack */
    for (i = 0; i < 100000; i++) {
        byte = ps2_read_byte(PS2_BASE2);
        if (byte == 0xFA) break;
    }

    mouse_buf_idx = 0;
}

/* ── mouse_read_packet ───────────────────────────────────────────────────────
 * Call once per main-loop iteration.
 * Accumulates bytes into mouse_buf[3].  When 3 bytes have been collected,
 * decodes and fills *pkt, resets the buffer, and returns 1.
 * Returns 0 if the packet is not yet complete.
 *
 * Sync guard: byte 0 must always have bit 3 set (per PS/2 spec).
 * If it doesn't, the buffer is out of sync — we discard and re-sync.
 *
 * Right-click detection: pkt->right_btn == 1 means the right button is
 * currently pressed.  ui.c should only act on a rising edge
 * (was 0, now 1) to avoid toggling the same cell on every packet.
 */
int mouse_read_packet(MOUSE_PACKET *pkt) {
    int byte = ps2_read_byte(PS2_BASE2);
    if (byte == -1)
        return 0;

    /* Sync check: first byte must have bit 3 set */
    if (mouse_buf_idx == 0 && !(byte & 0x08)) {
        /* Out of sync — discard this byte and wait for a valid first byte */
        return 0;
    }

    mouse_buf[mouse_buf_idx++] = (uint8_t)byte;

    if (mouse_buf_idx < 3)
        return 0;  /* packet not complete yet */

    /* All 3 bytes received — decode */
    mouse_buf_idx = 0;

    uint8_t status = mouse_buf[0];
    uint8_t x_raw  = mouse_buf[1];
    uint8_t y_raw  = mouse_buf[2];

    pkt->left_btn   = (status >> 0) & 1;
    pkt->right_btn  = (status >> 1) & 1;
    pkt->middle_btn = (status >> 2) & 1;

    /* Sign-extend X: bit 4 of status is the X sign bit */
    pkt->dx = (int)x_raw - ((status & 0x10) ? 256 : 0);

    /* Sign-extend Y: bit 5 of status is the Y sign bit.
     * PS/2 Y increases upward; we negate so positive dy = down on screen
     * (matching screen pixel coordinates where y=0 is top). */
    pkt->dy = -((int)y_raw - ((status & 0x20) ? 256 : 0));

    /* Discard overflow packets (bits 6 and 7) — movements are unreliable */
    if ((status & 0x40) || (status & 0x80)) {
        pkt->dx = 0;
        pkt->dy = 0;
    }

    return 1;
}