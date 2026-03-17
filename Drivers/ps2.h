#ifndef PS2_H
#define PS2_H

#include <stdint.h>

/* ── Hardware registers ───────────────────────────────────────────────────────
 * PS2_BASE  = first PS/2 port  (keyboard)
 * PS2_BASE2 = second PS/2 port (mouse, via Y-splitter on DE1-SoC)
 * Defined in config.h; fallback values here match DE1-SoC_Computer_NiosV.pdf.
 */
#ifndef PS2_BASE
#define PS2_BASE 0xFF200100
#endif
#ifndef PS2_BASE2
#define PS2_BASE2 0xFF200108
#endif

#define PS2_DATA_OFFSET 0 /* [15]=RVALID  [8]=RAVAIL  [7:0]=data byte */
#define PS2_CTRL_OFFSET 4 /* bit 0 = RE (1 = enable interrupt)         */
#define PS2_RVALID (1 << 15)

/* ── Set-2 scan-code constants ───────────────────────────────────────────────
 * All keyboard scan codes below are Set-2 make codes.
 * A key-release sequence is:  0xF0  <make_code>
 * An extended key sequence is: 0xE0 <make_code>  (release: 0xE0 0xF0 <make_code>)
 */
#define SC_BREAK 0xF0
#define SC_EXTEND 0xE0

/* Make codes */
#define SC_SPACE 0x29
#define SC_ENTER 0x5A
#define SC_ESC 0x76
#define SC_PLUS 0x4E  /* '+' / '=' key, right of 0 on US layout */
#define SC_MINUS 0x45 /* '-' key */

/* Extended make codes (sent after 0xE0 prefix) */
#define SC_UP 0x75
#define SC_DOWN 0x72
#define SC_LEFT 0x6B
#define SC_RIGHT 0x74

/* ── Logical key identifiers ─────────────────────────────────────────────────
 * ui.c operates on these; it never sees raw scan codes.
 */
typedef enum
{
    KEY_NONE = 0,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_SPACE,
    KEY_ENTER,
    KEY_PLUS,
    KEY_MINUS,
    KEY_ESC,
    KEY_UNKNOWN
} LogicalKey;

typedef enum
{
    KEY_PRESS = 0,
    KEY_RELEASE = 1
} KeyEventType;

typedef struct
{
    LogicalKey key;
    KeyEventType type;
} KEY_EVENT;

/* ── Mouse packet ────────────────────────────────────────────────────────────
 * Decoded from a standard 3-byte PS/2 mouse packet.
 * dy is sign-corrected so positive = up on screen.
 */
typedef struct
{
    int left_btn;  /* 1 while left button held */
    int right_btn; /* 1 while right button held — triggers note placement */
    int middle_btn;
    int dx; /* signed X delta, pixels */
    int dy; /* signed Y delta, positive = up */
} MOUSE_PACKET;

/* ── API ─────────────────────────────────────────────────────────────────────
 *
 * ps2_init()
 *   Initialise the keyboard port (polling mode, no interrupt).
 *   Call once before the main loop.
 *
 * ps2_get_key_event(KEY_EVENT *evt)
 *   Non-blocking poll.  Returns 1 and fills *evt when a complete event is
 *   ready; returns 0 when nothing is available.
 *
 *   Break-code guard (no key-repeat):
 *     A KEY_PRESS event for a given key is only emitted ONCE per physical
 *     press.  The driver tracks which keys are currently held and suppresses
 *     further PRESS events until the 0xF0 break sequence is received and a
 *     KEY_RELEASE is emitted.  This prevents held-key flooding entirely in
 *     software with no timers or interrupts required.
 *
 * mouse_init()
 *   Send 0xFF (reset) + 0xF4 (enable streaming) to the mouse port.
 *   Call once after ps2_init().
 *
 * mouse_read_packet(MOUSE_PACKET *pkt)
 *   Non-blocking poll.  Returns 1 and fills *pkt when a complete 3-byte
 *   packet has been received; returns 0 otherwise.
 *   Caller (ui.c) should act on pkt->right_btn == 1 to place a note.
 */
void ps2_init(void);
int ps2_get_key_event(KEY_EVENT *evt);

void mouse_init(void);
int mouse_read_packet(MOUSE_PACKET *pkt);

#endif /* PS2_H */