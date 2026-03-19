#include "vga_music.h"
#include "ps2.h"

int ps2_read_byte(volatile int *ps2)
{
    int v = *ps2;
    if (v & PS2_RVALID) return v & 0xFF;
    return -1;
}

void ps2_send_byte(volatile int *ps2, unsigned char b)
{
    int timeout = 2000000;
    /* Flush FIFO before sending so we don't mis-read a stale 0xFA */
    while (*ps2 & PS2_RVALID) (void)(*ps2);
    *ps2 = (int)b;
    while (timeout-- > 0) {
        int v = *ps2;
        if ((v & PS2_RVALID) && (v & 0xFF) == 0xFA) break;
    }
}

void mouse_init(void)
{
    volatile int *ps2 = (volatile int *)PS2_BASE;
    int i;

    /* Hard-flush: drain anything already in the FIFO */
    for (i = 0; i < 256; i++) (void)(*ps2);
    while (*ps2 & PS2_RVALID) (void)(*ps2);

    /* Reset — ps2_send_byte flushes before writing and waits for ACK */
    ps2_send_byte(ps2, 0xFF);

    /* Wait for BAT complete (0xAA) then device ID (0x00) */
    {
        int got_aa = 0, timeout = 2000000;
        while (timeout-- > 0) {
            int v = *ps2;
            if (v & PS2_RVALID) {
                int b = v & 0xFF;
                if (b == 0xAA) got_aa = 1;
                if (got_aa && b == 0x00) break;
            }
        }
    }

    /* Final flush, then enable streaming */
    while (*ps2 & PS2_RVALID) (void)(*ps2);
    ps2_send_byte(ps2, 0xF4);
    while (*ps2 & PS2_RVALID) (void)(*ps2);
}
