/*
 * main.c  —  PS/2 keyboard + mouse input test
 *
 * Prints every keyboard event and mouse packet to the JTAG terminal
 * so you can verify scan codes, break codes, and mouse deltas before
 * wiring input into the full sequencer.
 *
 * Build:  gmake COMPILE   (uses the Makefile in this folder)
 * Flash:  gmake DE1-SoC
 * Watch:  gmake TERMINAL
 *
 * Expected output examples:
 *   KEY PRESS  : UP
 *   KEY RELEASE: UP
 *   KEY PRESS  : SPACE
 *   MOUSE  dx=+3  dy=-1  L=0 R=1 M=0   <- right-click while moving
 */

#include <stdio.h>
#include "config.h"
#include "ps2.h"

/* ── JTAG UART printf target ─────────────────────────────────────────────────
 * The riscv32 newlib stdio is already wired to the JTAG UART at link time
 * via the -Wl,--defsym=JTAG_UART_BASE flag in the Makefile, so plain
 * printf() works without any extra setup.
 */

/* ── Key name lookup ─────────────────────────────────────────────────────────
 * Returns a human-readable string for each LogicalKey value.
 */
static const char *key_name(LogicalKey k)
{
    switch (k)
    {
    case KEY_UP:
        return "UP";
    case KEY_DOWN:
        return "DOWN";
    case KEY_LEFT:
        return "LEFT";
    case KEY_RIGHT:
        return "RIGHT";
    case KEY_SPACE:
        return "SPACE";
    case KEY_ENTER:
        return "ENTER";
    case KEY_PLUS:
        return "PLUS";
    case KEY_MINUS:
        return "MINUS";
    case KEY_ESC:
        return "ESC";
    case KEY_UNKNOWN:
        return "UNKNOWN";
    default:
        return "NONE";
    }
}

int main(void)
{
    printf("=== PS/2 Input Test ===\n");
    printf("Keyboard: press any mapped key\n");
    printf("Mouse:    move or right-click\n");
    printf("=======================\n\n");

    /* Initialise both devices */
    ps2_init();
    mouse_init();

    KEY_EVENT evt;
    MOUSE_PACKET pkt;

    /* Track previous right-button state to print rising/falling edges */
    int prev_right = 0;

    while (1)
    {
        /* ── Keyboard ── */
        if (ps2_get_key_event(&evt))
        {
            if (evt.type == KEY_PRESS)
                printf("KEY PRESS  : %s\n", key_name(evt.key));
            else
                printf("KEY RELEASE: %s\n", key_name(evt.key));
        }

        /* ── Mouse ── */
        if (mouse_read_packet(&pkt))
        {
            /* Always print movement so you can see deltas */
            if (pkt.dx != 0 || pkt.dy != 0 ||
                pkt.left_btn || pkt.right_btn || pkt.middle_btn)
            {

                printf("MOUSE  dx=%+d  dy=%+d  L=%d R=%d M=%d",
                       pkt.dx, pkt.dy,
                       pkt.left_btn, pkt.right_btn, pkt.middle_btn);

                /* Annotate right-click edge */
                if (pkt.right_btn && !prev_right)
                    printf("  <- RIGHT PRESS");
                else if (!pkt.right_btn && prev_right)
                    printf("  <- RIGHT RELEASE");

                printf("\n");
            }
            prev_right = pkt.right_btn;
        }
    }

    return 0;
}