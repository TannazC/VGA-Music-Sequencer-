#include <stdlib.h>          // gives access to standard utilities; not heavily used here but commonly included
#include "background.h"      // includes screen size constants, background declarations, and helper prototypes

/* ═══════════════════════════════════════════════════════════════════════
   VGA frame-buffer notes

   The actual frame buffer in memory is larger than the visible screen.
   We only display 320 x 240, but memory is laid out as 512 x 256.

   Each row takes 1024 bytes because:
     512 pixels/row * 2 bytes/pixel = 1024 bytes

   So:
     y << 10  means multiply y by 1024 bytes
     x << 1   means multiply x by 2 bytes per pixel

   Visible drawing area:
     x = 0 to 319
     y = 0 to 239
   ═══════════════════════════════════════════════════════════════════════ */

/* Arrow cursor dimensions in pixels */
#define ARROW_W     11       // cursor is 11 pixels wide
#define ARROW_H     16       // cursor is 16 pixels tall

/* Hardware base addresses */
#define PIXEL_BUF_CTRL  0xFF203020   // VGA pixel buffer controller register base
#define PS2_BASE        0xFF200100   // PS/2 port data register base
#define PS2_RVALID      0x8000       // bit mask showing whether PS/2 data is available

/* Mouse speed control */
#define SPEED_DIV   2        // divides mouse movement so cursor moves slower and is easier to control

/* Dot settings */
#define DOT_R       3        // each click creates a filled circle of radius 3
#define MAX_DOTS    256      // cap the number of dots so arrays do not overflow

/* RGB565 colours */
#define WHITE  ((short int)0xFFFF)   // white in 16-bit RGB565 format
#define BLACK  ((short int)0x0000)   // black in 16-bit RGB565 format

/* ═══════════════════════════════════════════════════════════════════════
   Global variables
   ═══════════════════════════════════════════════════════════════════════ */

int pixel_buffer_start;      // stores the base address of the active VGA frame buffer

/* bg[][] lives in background.c and is declared extern in background.h
   It stores the original background pixel colours for restoration. */

int dot_x[MAX_DOTS];         // x-coordinate of each stored click dot
int dot_y[MAX_DOTS];         // y-coordinate of each stored click dot
int num_dots = 0;            // current number of dots already placed on screen

/* ═══════════════════════════════════════════════════════════════════════
   plot_pixel

   Draws one pixel directly into VGA memory.

   Inputs:
     x -> screen x-coordinate
     y -> screen y-coordinate
     c -> RGB565 colour

   Output:
     none

   Main job:
     convert (x, y) into a byte address in the frame buffer and store c
   ═══════════════════════════════════════════════════════════════════════ */
void plot_pixel(int x, int y, short int c)
{
    if (x < 0 || x >= FB_WIDTH)  return;     // do nothing if x is outside visible screen range
    if (y < 0 || y >= FB_HEIGHT) return;     // do nothing if y is outside visible screen range

    *(volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = c;
    // compute pixel address and write colour directly into VGA memory
}

/* ═══════════════════════════════════════════════════════════════════════
   restore_pixel

   Restores one pixel after erasing the cursor.

   Why needed:
   The cursor moves around on top of the background and click dots.
   When we erase the cursor, we must restore whatever was underneath.

   Priority:
     1. if a dot covers this pixel, redraw BLACK
     2. otherwise redraw original background from bg[][]
   ═══════════════════════════════════════════════════════════════════════ */
void restore_pixel(int x, int y)
{
    int i, ddx, ddy;                              // loop variable and distance offsets from dot centres

    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;
    // immediately stop if pixel is off-screen

    for (i = 0; i < num_dots; i++) {             // check every stored dot one by one
        ddx = x - dot_x[i];                      // horizontal distance from current dot centre
        ddy = y - dot_y[i];                      // vertical distance from current dot centre

        if (ddx * ddx + ddy * ddy <= DOT_R * DOT_R) {
            // if this pixel lies inside that dot's circular area

            plot_pixel(x, y, BLACK);             // redraw dot pixel in black
            return;                              // stop because correct pixel has been restored
        }
    }

    plot_pixel(x, y, bg[y][x]);                  // if no dot covered it, restore original background pixel
}

/* ═══════════════════════════════════════════════════════════════════════
   Arrow shape lookup tables

   Each row of the arrow is described by:
     AX0[row] = first filled column
     AX1[row] = last filled column

   So row by row, the code fills a narrow triangle / arrow shape.
   ═══════════════════════════════════════════════════════════════════════ */
static const unsigned char AX0[16] = {0,0,0,0,0,0,0,0,0,0,0,3,3,3,3,3};
// leftmost filled column for each row of the arrow

static const unsigned char AX1[16] = {0,1,2,3,4,5,6,7,8,9,10,6,6,6,6,6};
// rightmost filled column for each row of the arrow

/* ═══════════════════════════════════════════════════════════════════════
   draw_arrow

   Draws the mouse cursor at position (tx, ty).

   Interpretation:
   (tx, ty) is the top-left anchor / tip position used for the glyph.
   ═══════════════════════════════════════════════════════════════════════ */
void draw_arrow(int tx, int ty)
{
    int row, col;                                 // row and column indices for the arrow glyph

    for (row = 0; row < ARROW_H; row++)          // go through every row of the arrow
        for (col = AX0[row]; col <= AX1[row]; col++)
            // for this row, fill only the columns that belong to the shape
            plot_pixel(tx + col, ty + row, BLACK);
            // draw each arrow pixel in black relative to the cursor origin
}

/* ═══════════════════════════════════════════════════════════════════════
   erase_arrow

   Erases the cursor by restoring every pixel the arrow used to cover.

   Important:
   This does not just paint white.
   It correctly restores dots or background underneath.
   ═══════════════════════════════════════════════════════════════════════ */
void erase_arrow(int tx, int ty)
{
    int row, col;                                 // row and column indices for the arrow glyph

    for (row = 0; row < ARROW_H; row++)          // scan each row of the cursor
        for (col = AX0[row]; col <= AX1[row]; col++)
            // scan all filled columns in that row
            restore_pixel(tx + col, ty + row);
            // restore what originally belonged under that cursor pixel
}

/* ═══════════════════════════════════════════════════════════════════════
   draw_dot

   Draws a filled circular dot centred at (cx, cy).

   Method:
   Loop over a square around the centre and only draw points that satisfy
   the circle equation:
       dx^2 + dy^2 <= r^2
   ═══════════════════════════════════════════════════════════════════════ */
void draw_dot(int cx, int cy)
{
    int dx, dy;                                   // offsets from the dot centre

    for (dy = -DOT_R; dy <= DOT_R; dy++)         // scan from top of circle's bounding box to bottom
        for (dx = -DOT_R; dx <= DOT_R; dx++)     // scan from left of bounding box to right
            if (dx * dx + dy * dy <= DOT_R * DOT_R)
                // only draw pixels lying inside the circle radius
                plot_pixel(cx + dx, cy + dy, BLACK);
                // draw that circle pixel in black
}

/* ═══════════════════════════════════════════════════════════════════════
   ps2_read_byte

   Reads one byte from the PS/2 port if valid data is available.

   Returns:
     0..255  if a byte is ready
     -1      if no byte is available right now

   This makes polling easy in the main loop.
   ═══════════════════════════════════════════════════════════════════════ */
static int ps2_read_byte(volatile int *ps2)
{
    int v = *ps2;                                 // read current PS/2 register contents

    if (v & PS2_RVALID) return v & 0xFF;         // if valid-data bit is set, return just the low 8-bit data byte
    return -1;                                    // otherwise signal that no byte is ready
}

/* ═══════════════════════════════════════════════════════════════════════
   ps2_send_byte

   Sends one command byte to the PS/2 device and waits for ACK (0xFA).

   Used during mouse initialization.

   Why flush first:
   old leftover bytes in the FIFO could be mistaken for the ACK.
   ═══════════════════════════════════════════════════════════════════════ */
static void ps2_send_byte(volatile int *ps2, unsigned char b)
{
    int timeout = 2000000;                        // timeout counter so we do not wait forever if mouse does not answer

    while (*ps2 & PS2_RVALID) (void)(*ps2);
    // drain any old bytes already sitting in the receive FIFO

    *ps2 = (int)b;
    // write the command byte to the PS/2 port

    while (timeout-- > 0) {                      // keep checking until timeout expires
        int v = *ps2;                            // read current PS/2 register value
        if ((v & PS2_RVALID) && (v & 0xFF) == 0xFA) break;
        // if a valid byte arrived and it equals ACK 0xFA, stop waiting
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   mouse_init

   Fully initializes the PS/2 mouse before main loop starts.

   Steps:
     1. flush old bytes
     2. send reset command 0xFF
     3. wait for BAT success 0xAA
     4. wait for device ID 0x00
     5. flush again
     6. enable streaming with 0xF4

   Called before drawing background so the FIFO does not overflow.
   ═══════════════════════════════════════════════════════════════════════ */
void mouse_init(void)
{
    volatile int *ps2 = (volatile int *)PS2_BASE;    // pointer to memory-mapped PS/2 register
    int i;                                            // loop variable for flushing

    for (i = 0; i < 256; i++) (void)(*ps2);
    // perform a hard flush by reading many times in case FIFO already contains junk bytes

    while (*ps2 & PS2_RVALID) (void)(*ps2);
    // continue draining until no valid bytes remain

    ps2_send_byte(ps2, 0xFF);
    // send reset command to the mouse

    {
        int got_aa = 0, timeout = 2000000;
        // got_aa tracks whether BAT success byte 0xAA has appeared yet

        while (timeout-- > 0) {                  // wait for reset response sequence
            int v = *ps2;                        // read PS/2 register
            if (v & PS2_RVALID) {                // only use it if a byte is actually ready
                int b = v & 0xFF;                // isolate the received 8-bit data byte
                if (b == 0xAA) got_aa = 1;       // 0xAA means Basic Assurance Test passed
                if (got_aa && b == 0x00) break;  // after BAT, device ID 0x00 confirms standard mouse
            }
        }
    }

    while (*ps2 & PS2_RVALID) (void)(*ps2);
    // flush any extra bytes before enabling packet streaming

    ps2_send_byte(ps2, 0xF4);
    // command 0xF4 tells the mouse to start streaming movement packets

    while (*ps2 & PS2_RVALID) (void)(*ps2);
    // drain the ACK so the main loop starts with clean packet data
}

/* ═══════════════════════════════════════════════════════════════════════
   main

   Overall job:
     - set up VGA buffer
     - initialize mouse
     - draw static background
     - show cursor
     - read 3-byte PS/2 mouse packets forever
     - move cursor based on packet deltas
     - place black dots on left-click rising edge
   ═══════════════════════════════════════════════════════════════════════ */
int main(void)
{
    volatile int *pixel_ctrl = (volatile int *)PIXEL_BUF_CTRL;
    // pointer to VGA pixel buffer controller registers

    volatile int *ps2        = (volatile int *)PS2_BASE;
    // pointer to PS/2 data register used for reading live mouse bytes

    pixel_buffer_start = *pixel_ctrl;
    // read the current front-buffer base address from the controller

    *(pixel_ctrl + 1)  = pixel_buffer_start;
    // set the back buffer to the same address so this program uses single buffering

    mouse_init();
    // initialize PS/2 mouse first so incoming mouse bytes are handled correctly before long background draw

    build_and_draw_background();
    // create the white music-sheet background, draw staves/clefs, and initialize bg[][]

    int cx = FB_WIDTH  / 2;
    // start cursor x-position at horizontal centre of visible screen

    int cy = FB_HEIGHT / 2;
    // start cursor y-position at vertical centre of visible screen

    int cx_max = FB_WIDTH  - ARROW_W;
    // maximum legal x so the whole arrow still fits on-screen

    int cy_max = FB_HEIGHT - ARROW_H;
    // maximum legal y so the whole arrow still fits on-screen

    int ax = 0, ay = 0;
    // accumulators store leftover sub-pixel motion after dividing speed

    int byte_idx = 0;
    // tells us which byte of the 3-byte mouse packet we are currently collecting

    unsigned char pkt[3];
    // stores one full PS/2 mouse packet: flags, dx, dy

    int prev_left = 0;
    // remembers previous left-button state so we can detect a click edge

    draw_arrow(cx, cy);
    // draw the initial cursor at the centre of the screen

    while (1)
    {
        int raw = ps2_read_byte(ps2);
        // try to read one new byte from the PS/2 port

        if (raw < 0) continue;
        // if no byte is ready yet, skip this iteration and keep polling

        unsigned char b = (unsigned char)raw;
        // cast returned value into an 8-bit unsigned byte for packet handling

        if (byte_idx == 0 && !(b & 0x08)) continue;
        // packet synchronization:
        // the first byte of a valid PS/2 mouse packet must have bit 3 = 1
        // if not, this byte is probably stale/misaligned data, so discard it

        pkt[byte_idx++] = b;
        // store current byte into packet buffer, then move to next packet position

        if (byte_idx < 3) continue;
        // if we do not yet have all 3 bytes, keep collecting

        byte_idx = 0;
        // once 3 bytes are collected, reset index so next packet starts from byte 0

        unsigned char flags = pkt[0];
        // first byte contains buttons, sign bits, and overflow bits

        int dx = (int)pkt[1];
        // second byte is X movement delta, initially as unsigned 8-bit data

        int dy = (int)pkt[2];
        // third byte is Y movement delta, initially as unsigned 8-bit data

        if (flags & 0x10) dx |= 0xFFFFFF00;
        // if X sign bit is set, sign-extend dx so negative movement is represented properly as int

        if (flags & 0x20) dy |= 0xFFFFFF00;
        // if Y sign bit is set, sign-extend dy so negative movement is represented properly as int

        if (flags & 0xC0) {
            // bits 6 and 7 are overflow bits; if either is set, movement data is unreliable

            prev_left = (flags & 0x01);
            // still update left-button history so click state stays consistent

            continue;
            // ignore this packet and move on to the next one
        }

        ax += dx;
        // accumulate raw X motion before speed scaling

        ay += dy;
        // accumulate raw Y motion before speed scaling

        int mx = ax / SPEED_DIV;
        // convert accumulated X motion into slowed integer screen movement

        int my = ay / SPEED_DIV;
        // convert accumulated Y motion into slowed integer screen movement

        ax -= mx * SPEED_DIV;
        // keep the leftover X remainder so small motions are not lost

        ay -= my * SPEED_DIV;
        // keep the leftover Y remainder so small motions are not lost

        erase_arrow(cx, cy);
        // remove old cursor by restoring background/dots underneath it

        cx += mx;
        // move cursor horizontally by processed X movement

        cy -= my;
        // subtract Y movement because PS/2 mouse positive Y is upward, but screen positive Y is downward

        if (cx < 0)       cx = 0;
        // clamp cursor to left edge

        if (cx > cx_max)  cx = cx_max;
        // clamp cursor so arrow does not go off right edge

        if (cy < 0)       cy = 0;
        // clamp cursor to top edge

        if (cy > cy_max)  cy = cy_max;
        // clamp cursor so arrow does not go off bottom edge

        int left_now = (flags & 0x01) ? 1 : 0;
        // extract current left-button state from bit 0 of flags

        if (left_now && !prev_left) {
            // true only on rising edge: button is pressed now but was not pressed before

            if (num_dots < MAX_DOTS) {
                // only store new dot if arrays still have room

                dot_x[num_dots] = cx;
                // record x-position of the newly placed dot

                dot_y[num_dots] = cy;
                // record y-position of the newly placed dot

                num_dots++;
                // increase total dot count so future restore checks include this dot
            }

            draw_dot(cx, cy);
            // immediately draw the new dot at the cursor position
        }

        prev_left = left_now;
        // save current button state for next loop iteration

        draw_arrow(cx, cy);
        // draw cursor at its updated screen position

        pixel_buffer_start = *pixel_ctrl;
        // refresh active buffer base address, even though in single-buffer mode it should stay the same
    }

    return 0;
    // program is intended to run forever, so this line is never realistically reached
}
