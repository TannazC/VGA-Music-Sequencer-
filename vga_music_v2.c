#include <stdlib.h>
#include "background.h"

/* ═══════════════════════════════════════════════════════════════════════
   VGA frame buffer notes

   The pixel buffer is stored in memory as a 512 x 256 region, even though
   the visible VGA area is only 320 x 240.

   Addressing works like this:
     - each row is 1024 bytes apart  ->  y << 10
     - each pixel is 2 bytes         ->  x << 1

   So the address of pixel (x, y) is:
       pixel_buffer_start + (y << 10) + (x << 1)

   The VGA hardware scales this visible region to the display.
   FB_WIDTH and FB_HEIGHT come from background.h
   ═══════════════════════════════════════════════════════════════════════ */

/* Arrow cursor dimensions */
#define ARROW_W     11
#define ARROW_H     16

/* Memory-mapped hardware base addresses */
#define PIXEL_BUF_CTRL  0xFF203020   /* VGA pixel buffer controller */
#define PS2_BASE        0xFF200100   /* PS/2 port base address */
#define PS2_RVALID      0x8000       /* bit set when PS/2 data is available */

/* Mouse speed scaling
   Larger divisor = slower cursor movement on screen */
#define SPEED_DIV   2

/* Dot drawing settings */
#define DOT_R       3                /* radius of a click dot */
#define MAX_DOTS    256              /* maximum number of stored dots */

/* RGB565 colours */
#define WHITE  ((short int)0xFFFF)
#define BLACK  ((short int)0x0000)

/* ═══════════════════════════════════════════════════════════════════════
   Global variables
   ═══════════════════════════════════════════════════════════════════════ */

/* Holds the current frame buffer base address */
int pixel_buffer_start;

/* bg[][] is defined in background.c and declared extern in background.h
   It stores the background colour for every pixel */

/* Arrays storing the centre position of each placed dot */
int dot_x[MAX_DOTS];
int dot_y[MAX_DOTS];
int num_dots = 0;    /* current number of dots on the screen */

/* ═══════════════════════════════════════════════════════════════════════
   plot_pixel

   Writes one pixel to the frame buffer if the coordinates are on-screen.

   Parameters:
     x  -> x-coordinate of the pixel
     y  -> y-coordinate of the pixel
     c  -> 16-bit RGB565 colour to write

   Input:
     screen coordinates and colour

   Output:
     none

   Side effect:
     directly writes to VGA memory
   ═══════════════════════════════════════════════════════════════════════ */
void plot_pixel(int x, int y, short int c)
{
    /* Ignore any attempt to draw off-screen */
    if (x < 0 || x >= FB_WIDTH)  return;
    if (y < 0 || y >= FB_HEIGHT) return;

    *(volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = c;
}

/* ═══════════════════════════════════════════════════════════════════════
   restore_pixel

   Restores one pixel to whatever should be underneath the cursor.
   When the cursor is erased, we cannot blindly redraw the background,
   because the cursor may be sitting on top of a previously placed dot!!!!!!!!!!!!!!

   So this function checks:
     1. is this pixel inside any stored black dot?
        -> redraw BLACK
     2. otherwise
        -> redraw the original background pixel from bg[][]

   Parameters:
     x  -> x-coordinate of the pixel to restore
     y  -> y-coordinate of the pixel to restore

   Input:
     pixel location

   Output:
     none

   Side effect:
     redraws either a background pixel or a dot pixel
   ═══════════════════════════════════════════════════════════════════════ */
void restore_pixel(int x, int y)
{
    int i, ddx, ddy;

    /* Ignore invalid coordinates */
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;

    /* Check whether this pixel belongs to any stored dot */
    for (i = 0; i < num_dots; i++) {
        ddx = x - dot_x[i];
        ddy = y - dot_y[i];

        /* Inside the circle if distance^2 <= radius^2 */
        if (ddx * ddx + ddy * ddy <= DOT_R * DOT_R) {
            plot_pixel(x, y, BLACK);
            return;
        }
    }

    /* Otherwise restore the original background pixel */
    plot_pixel(x, y, bg[y][x]);
}

/* ═══════════════════════════════════════════════════════════════════════
   Arrow cursor shape

   The cursor is stored row-by-row using two lookup arrays:
     AX0[row] = first column to draw on that row
     AX1[row] = last  column to draw on that row

   So for each row, we fill pixels from AX0[row] to AX1[row].

   The cursor tip is anchored at (tx, ty).
   ═══════════════════════════════════════════════════════════════════════ */
static const unsigned char AX0[16] = {0,0,0,0,0,0,0,0,0,0,0,3,3,3,3,3};
static const unsigned char AX1[16] = {0,1,2,3,4,5,6,7,8,9,10,6,6,6,6,6};

/* ═══════════════════════════════════════════════════════════════════════
   draw_arrow

   Draws the mouse cursor as a black arrow.

   Parameters:
     tx -> x-coordinate of the arrow tip
     ty -> y-coordinate of the arrow tip

   Input:
     cursor position

   Output:
     none

   Side effect:
     writes cursor pixels onto the screen
   ═══════════════════════════════════════════════════════════════════════ */
void draw_arrow(int tx, int ty)
{
    int row, col;

    for (row = 0; row < ARROW_H; row++)
        for (col = AX0[row]; col <= AX1[row]; col++)
            plot_pixel(tx + col, ty + row, BLACK);
}

/* ═══════════════════════════════════════════════════════════════════════
   erase_arrow

   Removes the arrow cursor by restoring every pixel that the cursor
   previously covered.

   It does not just paint over the cursor. Instead, it calls restore_pixel()
   so the screen underneath is reconstructed correctly.

   Parameters:
     tx -> x-coordinate of the arrow tip
     ty -> y-coordinate of the arrow tip

   Input:
     old cursor position

   Output:
     none

   Side effect:
     restores background and/or dot pixels under the old cursor
   ═══════════════════════════════════════════════════════════════════════ */
void erase_arrow(int tx, int ty)
{
    int row, col;

    for (row = 0; row < ARROW_H; row++)
        for (col = AX0[row]; col <= AX1[row]; col++)
            restore_pixel(tx + col, ty + row);
}

/* ═══════════════════════════════════════════════════════════════════════
   draw_dot

   Draws a filled black circle centred at (cx, cy).

   The circle is made by scanning a square around the centre and drawing
   only the pixels that satisfy:
       dx^2 + dy^2 <= r^2

   Parameters:
     cx -> x-coordinate of the dot centre
     cy -> y-coordinate of the dot centre

   Input:
     centre location of the dot

   Output:
     none

   Side effect:
     draws directly onto the screen
   ═══════════════════════════════════════════════════════════════════════ */
void draw_dot(int cx, int cy)
{
    int dx, dy;

    for (dy = -DOT_R; dy <= DOT_R; dy++)
        for (dx = -DOT_R; dx <= DOT_R; dx++)
            if (dx * dx + dy * dy <= DOT_R * DOT_R)
                plot_pixel(cx + dx, cy + dy, BLACK);
}

/* ═══════════════════════════════════════════════════════════════════════
   ps2_read_byte

   Tries to read one byte from the PS/2 port.

   If data is available, returns the low 8 bits.
   If no data is available, returns -1.

   Parameters:
     ps2 -> pointer to the PS/2 data register

   Input:
     memory-mapped PS/2 register pointer

   Output:
     byte value in range 0..255, or -1 if no valid data

   Why static?
     This helper is only used inside this file, so it does not need
     external linkage.
   ═══════════════════════════════════════════════════════════════════════ */
static int ps2_read_byte(volatile int *ps2)
{
    int v = *ps2;

    if (v & PS2_RVALID) return v & 0xFF;
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════
   ps2_send_byte

   Sends one command byte to the PS/2 device and waits for the standard
   ACK response 0xFA.

   Before sending:
     the receive FIFO is flushed so that an old leftover byte is not
     mistaken for the new ACK.

   Parameters:
     ps2 -> pointer to the PS/2 data register
     b   -> command byte to send

   Input:
     PS/2 register pointer and one command byte

   Output:
     none

   Side effect:
     writes to the PS/2 device and waits for acknowledgement

   Note:
     This uses a timeout so the program does not get stuck forever if
     the device fails to respond.
   ═══════════════════════════════════════════════════════════════════════ */
static void ps2_send_byte(volatile int *ps2, unsigned char b)
{
    int timeout = 2000000;

    /* Clear any old bytes first */
    while (*ps2 & PS2_RVALID) (void)(*ps2);

    /* Write command byte */
    *ps2 = (int)b;

    /* Wait for ACK = 0xFA */
    while (timeout-- > 0) {
        int v = *ps2;
        if ((v & PS2_RVALID) && (v & 0xFF) == 0xFA) break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   mouse_init

   Initializes the PS/2 mouse.

   Sequence used here:
     1. flush any old bytes from the FIFO
     2. send reset command 0xFF
     3. wait for BAT success code 0xAA
     4. wait for device ID 0x00
     5. flush again
     6. enable streaming with command 0xF4

   This is called before drawing the background so that incoming mouse
   data does not pile up in the FIFO while the program is busy.

   Parameters:
     none

   Input:
     none directly, but communicates with the PS/2 hardware

   Output:
     none

   Side effect:
     prepares the mouse to start sending movement packets
   ═══════════════════════════════════════════════════════════════════════ */
void mouse_init(void)
{
    volatile int *ps2 = (volatile int *)PS2_BASE;
    int i;

    /* Hard flush: read repeatedly to clear anything already in the FIFO */
    for (i = 0; i < 256; i++) (void)(*ps2);
    while (*ps2 & PS2_RVALID) (void)(*ps2);

    /* Reset the mouse */
    ps2_send_byte(ps2, 0xFF);

    /* Wait for BAT complete (0xAA) followed by device ID (0x00) */
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

    /* Flush again before enabling packet streaming */
    while (*ps2 & PS2_RVALID) (void)(*ps2);

    /* Enable data reporting */
    ps2_send_byte(ps2, 0xF4);

    /* Drain any remaining ACK byte */
    while (*ps2 & PS2_RVALID) (void)(*ps2);
}

/* ═══════════════════════════════════════════════════════════════════════
   main

   High-level program flow:
     1. initialize VGA frame buffer
     2. initialize PS/2 mouse
     3. build and draw the static background
     4. place the cursor in the middle of the screen
     5. continuously read 3-byte mouse packets
     6. move the cursor based on mouse deltas
     7. on a left-click rising edge, place a black dot

   Mouse packet format:
     byte 0 -> flags / buttons / sign bits / overflow bits
     byte 1 -> X movement delta
     byte 2 -> Y movement delta

   Parameters:
     none

   Input:
     live PS/2 mouse packet stream

   Output:
     returns 0 in principle, although this program is intended to run forever

   Side effects:
     updates the VGA display continuously in real time
   ═══════════════════════════════════════════════════════════════════════ */
int main(void)
{
    volatile int *pixel_ctrl = (volatile int *)PIXEL_BUF_CTRL;
    volatile int *ps2        = (volatile int *)PS2_BASE;

    /* Single-buffer setup:
       read current front buffer address and set back buffer to same address */
    pixel_buffer_start = *pixel_ctrl;
    *(pixel_ctrl + 1)  = pixel_buffer_start;

    /* Initialize mouse first so incoming data does not overflow the FIFO
       while the background is being generated */
    mouse_init();

    /* Build background lookup table and draw it once to the frame buffer */
    build_and_draw_background();

    /* Start cursor near the centre of the visible screen */
    int cx = FB_WIDTH  / 2;
    int cy = FB_HEIGHT / 2;

    /* Clamp limits so the full arrow stays visible */
    int cx_max = FB_WIDTH  - ARROW_W;
    int cy_max = FB_HEIGHT - ARROW_H;

    /* Sub-pixel accumulators
       These preserve fractional motion when dividing mouse speed */
    int ax = 0, ay = 0;

    /* Packet assembly state for the 3-byte PS/2 mouse packet */
    int byte_idx = 0;
    unsigned char pkt[3];

    /* Used to detect a rising edge on the left mouse button */
    int prev_left = 0;

    /* Draw initial cursor */
    draw_arrow(cx, cy);

    /* Main polling loop */
    while (1)
    {
        int raw = ps2_read_byte(ps2);
        if (raw < 0) continue;   /* no new byte yet */

        unsigned char b = (unsigned char)raw;

        /* Packet synchronization:
           the first byte of a PS/2 mouse packet always has bit 3 = 1.
           If we are expecting byte 0 and bit 3 is not set, then this byte
           is probably leftover garbage or a misaligned delta byte, so skip it. */
        if (byte_idx == 0 && !(b & 0x08)) continue;

        /* Store the byte into the packet buffer */
        pkt[byte_idx++] = b;

        /* Wait until all 3 bytes have arrived */
        if (byte_idx < 3) continue;
        byte_idx = 0;

        /* Decode one full mouse packet */
        unsigned char flags = pkt[0];
        int dx = (int)pkt[1];
        int dy = (int)pkt[2];

        /* Sign-extend 8-bit movement values into full int values */
        if (flags & 0x10) dx |= 0xFFFFFF00;
        if (flags & 0x20) dy |= 0xFFFFFF00;

        /* Ignore overflow packets
           If movement overflowed, the deltas are unreliable */
        if (flags & 0xC0) {
            prev_left = (flags & 0x01);
            continue;
        }

        /* Apply speed scaling using accumulators
           This slows the cursor down without losing fine movement */
        ax += dx;
        ay += dy;

        int mx = ax / SPEED_DIV;
        int my = ay / SPEED_DIV;

        ax -= mx * SPEED_DIV;
        ay -= my * SPEED_DIV;

        /* Remove old cursor first */
        erase_arrow(cx, cy);

        /* Update cursor position
           PS/2 Y is positive upward, but screen Y increases downward,
           so Y movement must be inverted */
        cx += mx;
        cy -= my;

        /* Clamp to visible screen bounds */
        if (cx < 0)      cx = 0;
        if (cx > cx_max) cx = cx_max;
        if (cy < 0)      cy = 0;
        if (cy > cy_max) cy = cy_max;

        /* Detect left-click rising edge:
           place one dot only when button changes from not-pressed to pressed */
        int left_now = (flags & 0x01) ? 1 : 0;

        if (left_now && !prev_left) {
            if (num_dots < MAX_DOTS) {
                dot_x[num_dots] = cx;
                dot_y[num_dots] = cy;
                num_dots++;
            }
            draw_dot(cx, cy);
        }

        prev_left = left_now;

        /* Draw cursor at its new location */
        draw_arrow(cx, cy);

        /* Refresh frame buffer pointer
           In this program the front and back are the same buffer, but keeping
           this updated ensures drawing always targets the active buffer base */
        pixel_buffer_start = *pixel_ctrl;
    }

    return 0;
}
