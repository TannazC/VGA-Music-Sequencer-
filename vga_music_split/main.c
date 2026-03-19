#include <stdlib.h>
#include "vga_music.h"
#include "graphics.h"
#include "background.h"
#include "cursor.h"
#include "ps2.h"

/* ── Global definitions ───────────────────────────────────────────────── */
int pixel_buffer_start;

int dot_x[MAX_DOTS];
int dot_y[MAX_DOTS];
int num_dots = 0;

/* Playback state */
int playing     = 0;
int play_idx    = 0;
int speed_level = DEFAULT_SPEED;
int play_timer  = 0;
int note_phase  = 0;
int note_freq   = 0;
int note_samples= 0;

const int speed_ticks[SPEED_LEVELS] = { 48000, 36000, 24000, 16000, 10000 };

/*
 * Note frequencies (Hz) indexed by semitone above C4.
 * C4=0, D4=2, E4=4, F4=5, G4=7, A4=9, B4=11,
 * C5=12, D5=14, E5=16, F5=17, G5=19
 */
const int note_freq_hz[] = {
    /* C4  */ 262, /* C#4 */ 277, /* D4  */ 294, /* D#4 */ 311,
    /* E4  */ 330, /* F4  */ 349, /* F#4 */ 370, /* G4  */ 392,
    /* G#4 */ 415, /* A4  */ 440, /* A#4 */ 466, /* B4  */ 494,
    /* C5  */ 523, /* C#5 */ 554, /* D5  */ 587, /* D#5 */ 622,
    /* E5  */ 659, /* F5  */ 698, /* F#5 */ 740, /* G5  */ 784
};

/* ── Main ─────────────────────────────────────────────────────────────── */
int main(void)
{
    volatile int *pixel_ctrl = (volatile int *)PIXEL_BUF_CTRL;
    volatile int *ps2        = (volatile int *)PS2_BASE;

    /* Single-buffer VGA init */
    pixel_buffer_start = *pixel_ctrl;
    *(pixel_ctrl + 1)  = pixel_buffer_start;

    /* Init mouse FIRST so the FIFO doesn't overflow during background draw */
    mouse_init();

    /* Clear character buffer then render the full background */
    char_buf_clear();
    draw_background();   /* pixels + char-buffer labels */

    /* Cursor start position */
    int cx = FB_WIDTH  / 2;
    int cy = FB_HEIGHT / 2;
    int cx_max = FB_WIDTH  - ARROW_W;   /* 309 */
    int cy_max = FB_HEIGHT - ARROW_H;   /* 224 */

    int ax = 0, ay = 0;            /* sub-pixel accumulators */
    int byte_idx = 0;
    unsigned char pkt[3];
    int prev_left = 0;

    draw_arrow(cx, cy, BLACK);

    /* ── Main loop ── */
    while (1)
    {
        int raw = ps2_read_byte(ps2);
        if (raw < 0) continue;

        unsigned char b = (unsigned char)raw;

        /* Sync: flags byte always has bit 3 set; discard stale delta bytes */
        if (byte_idx == 0 && !(b & 0x08)) continue;

        pkt[byte_idx++] = b;
        if (byte_idx < 3) continue;
        byte_idx = 0;

        /* Decode 3-byte PS/2 mouse packet */
        unsigned char flags = pkt[0];
        int dx = (int)pkt[1];
        int dy = (int)pkt[2];

        if (flags & 0x10) dx |= 0xFFFFFF00;   /* sign-extend X */
        if (flags & 0x20) dy |= 0xFFFFFF00;   /* sign-extend Y */
        if (flags & 0xC0) { prev_left = (flags & 0x01); continue; } /* overflow */

        /* Speed divisor via accumulator — no motion is lost */
        ax += dx;  ay += dy;
        int mx = ax / SPEED_DIV;
        int my = ay / SPEED_DIV;
        ax -= mx * SPEED_DIV;
        ay -= my * SPEED_DIV;

        erase_arrow(cx, cy);

        cx += mx;
        cy -= my;   /* PS/2 Y is inverted relative to screen Y */
        if (cx < 0)      cx = 0;
        if (cx > cx_max) cx = cx_max;
        if (cy < 0)      cy = 0;
        if (cy > cy_max) cy = cy_max;

        /* Rising-edge left click */
        int left_now = (flags & 0x01) ? 1 : 0;
        if (left_now && !prev_left) {
            int btn = button_hit(cx, cy);
            if (btn == 0) {
                /* TODO: start playback */
            } else if (btn == 1) {
                /* TODO: stop playback */
            } else if (btn == 2) {
                if (speed_level < SPEED_LEVELS - 1) speed_level++;
            } else if (btn == 3) {
                if (speed_level > 0) speed_level--;
            } else if (cy >= MENUBAR_H && num_dots < MAX_DOTS) {
                dot_x[num_dots] = cx;
                dot_y[num_dots] = cy;
                num_dots++;
                draw_dot(cx, cy);
            }
        }
        prev_left = left_now;

        draw_arrow(cx, cy, BLACK);

        /* Keep buffer pointer fresh (single-buffer, no swap needed) */
        pixel_buffer_start = *pixel_ctrl;
    }

    return 0;
}
