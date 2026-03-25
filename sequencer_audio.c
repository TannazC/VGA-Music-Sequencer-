/*
 * sequencer_audio.c
 * ─────────────────────────────────────────────────────────────────────────────
 * Playback engine for the 4-staff music sequencer.
 * Include sequencer_audio.h in vga_music_v2.c and call play_sequence().
 *
 * TIMELINE MODEL
 * ──────────────
 * The 4 staves are ONE continuous melody wrapped like lines of text:
 *   Staff 0  col 1-15  (bar 1)
 *   Staff 1  col 1-15  (bar 2)
 *   Staff 2  col 1-15  (bar 3)
 *   Staff 3  col 1-15  (bar 4)
 * Playback goes left-to-right across staff 0, then staff 1, 2, 3.
 *
 * PITCH MAPPINGA
 * ─────────────
 * Pitch = f(absolute grid row).  Row = staff*9 + pitch_slot.
 * Row 0 (top of staff 0) = F5, descends diatonically to row 35 = F0.Q
 * Staff number only determines WHEN a note plays, never its pitch.
 *
 * PLAYHEAD ANIMATION — SAFE PIXEL SAVE/RESTORE
 * ─────────────────────────────────────────────
 * Before drawing the green bar we save every pixel it will cover into
 * a local buffer.  After audio finishes we write those pixels back
 * exactly.  We never touch bg[][] or call redraw_all_notes() during
 * erase — so no note, staff line, beam or stem is ever disturbed.
 *
 * AUDIO
 * ─────
 * Square-wave synthesis.  Fs=8000 Hz, 120 BPM → quarter=4000 samples.
 * Each column = one quarter note.  Up to 8 simultaneous oscillators.
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include <stdint.h>
#include "background.h"
#include "sequencer_audio.h"

/* ═══════════════════════════════════════════════════════════════════════
   Hardware
   ═══════════════════════════════════════════════════════════════════════ */
#define AUDIO_BASE      0xFF203040
#define PIXEL_BUF_CTRL  0xFF203020

/* ═══════════════════════════════════════════════════════════════════════
   Audio peripheral  (matches the DE-series WM8731 core register layout)
   ═══════════════════════════════════════════════════════════════════════ */
typedef struct {
    volatile uint32_t control;
    volatile uint8_t  rarc;
    volatile uint8_t  ralc;
    volatile uint8_t  wsrc;
    volatile uint8_t  wslc;
    volatile uint32_t ldata;
    volatile uint32_t rdata;
} audio_t;

#define MAX24   0x007FFFFF
#define MIN24  (-0x00800000)
static inline int32_t clamp24(int32_t x)
{
    if (x >  MAX24) return  MAX24;
    if (x <  MIN24) return  MIN24;
    return x;
}

/* ═══════════════════════════════════════════════════════════════════════
   Note types  (must match vga_music_v2.c exactly)
   ═══════════════════════════════════════════════════════════════════════ */
#define NOTE_WHOLE      0
#define NOTE_HALF       1
#define NOTE_QUARTER    2
#define NOTE_BEAM2_8TH  3
#define NOTE_BEAM4_16TH 4
#define NOTE_BEAM2_16TH 5
#define NOTE_SINGLE16TH 6
#define NUM_NOTE_TYPES  7

static const int note_num_heads_audio[NUM_NOTE_TYPES] = { 1,1,1,2,4,2,1 };

/* ═══════════════════════════════════════════════════════════════════════
   Grid constants  (must match background.h / vga_music_v2.c)
   ═══════════════════════════════════════════════════════════════════════ */
#define SLOTS_PER_STAFF  ((LINES_PER_STAFF - 1) * 2 + 1)   /* 9  */
#define TOTAL_ROWS       (NUM_STAVES * SLOTS_PER_STAFF)     /* 36 */
#define TOTAL_COLS       NUM_STEPS                          /* 16 */
#define FIRST_COL        1

/* ═══════════════════════════════════════════════════════════════════════
   Note struct  (must be identical to vga_music_v2.c)
   ═══════════════════════════════════════════════════════════════════════ */
#define MAX_NOTES  256
#define MAX_HEADS  4

/* Note struct and shared globals — only define once.
   In combined.c the struct is already defined by vga_music_v2.c above us.
   When compiling sequencer_audio.c standalone, we define them here.     */
#ifndef NOTE_STRUCT_DEFINED
#define NOTE_STRUCT_DEFINED
typedef struct {
    int step;
    int staff;
    int pitch_slot;
    int note_type;
    int duration_64;
    int num_heads;
    int head_step[MAX_HEADS];
    int head_pitch_slot[MAX_HEADS];
    
    int screen_x;
    int screen_y;
    int head_x[MAX_HEADS];
    int head_y[MAX_HEADS];
} Note;
extern Note      notes[MAX_NOTES];
extern int       num_notes;
extern int       pixel_buffer_start;
extern short int bg[FB_HEIGHT][FB_WIDTH];
#endif /* NOTE_STRUCT_DEFINED */

/* ═══════════════════════════════════════════════════════════════════════
   Pitch table — row 0 (F5) → row 35 (F0)
   row = staff * SLOTS_PER_STAFF + pitch_slot
   ═══════════════════════════════════════════════════════════════════════ */
static const int pitch_freq_row[36] = {
     698, /* row  0  F5 */  659, /* row  1  E5 */  587, /* row  2  D5 */
     523, /* row  3  C5 */  494, /* row  4  B4 */  440, /* row  5  A4 */
     392, /* row  6  G4 */  349, /* row  7  F4 */  330, /* row  8  E4 */
     294, /* row  9  D4 */  262, /* row 10  C4 */  247, /* row 11  B3 */
     220, /* row 12  A3 */  196, /* row 13  G3 */  175, /* row 14  F3 */
     165, /* row 15  E3 */  147, /* row 16  D3 */  131, /* row 17  C3 */
     123, /* row 18  B2 */  110, /* row 19  A2 */   98, /* row 20  G2 */
      87, /* row 21  F2 */   82, /* row 22  E2 */   73, /* row 23  D2 */
      65, /* row 24  C2 */   62, /* row 25  B1 */   55, /* row 26  A1 */
      49, /* row 27  G1 */   44, /* row 28  F1 */   41, /* row 29  E1 */
      37, /* row 30  D1 */   33, /* row 31  C1 */   31, /* row 32  B0 */
      28, /* row 33  A0 */   25, /* row 34  G0 */   22  /* row 35  F0 */
};

/* ═══════════════════════════════════════════════════════════════════════
   Audio timing
   ═══════════════════════════════════════════════════════════════════════ */
#define FS_HZ         8000
#define QUARTER_SAMPS 4000   /* 120 BPM */
#define COLUMN_TICKS  QUARTER_SAMPS
#define SQ_AMP        0x200000

/* ═══════════════════════════════════════════════════════════════════════
   Visual — playhead geometry
   ═══════════════════════════════════════════════════════════════════════ */
#define PLAYHEAD_COLOR  ((short int)0x07E0)   /* bright green RGB565 */
#define PLAYHEAD_W      2                     /* pixels wide (1 px each side of centre) */

/* Playhead height = staff height with a small margin above/below */
#define PH_Y_TOP(s)  (staff_top[s] - 2)
#define PH_Y_BOT(s)  (staff_top[s] + (LINES_PER_STAFF - 1) * STAFF_SPACING + 2)
#define PH_HEIGHT    (PH_Y_BOT(0) - PH_Y_TOP(0) + 1)  /* same for all staves */

/* Save buffer: (2*PLAYHEAD_W+1) columns × PH_HEIGHT rows
   = 5 × 32 = 160 pixels max. Use 5*40 to be safe. */
#define PH_SAVE_W    (2 * PLAYHEAD_W + 1)
#define PH_SAVE_H    40
static short int ph_save[PH_SAVE_H][PH_SAVE_W];

/* ═══════════════════════════════════════════════════════════════════════
   Grid helper
   ═══════════════════════════════════════════════════════════════════════ */
static int col_to_x_audio(int col)
{
    if (col < FIRST_COL)   col = FIRST_COL;
    if (col >= TOTAL_COLS) col = TOTAL_COLS - 1;
    return STAFF_X0 + col * STEP_W + STEP_W / 2;
}

/* ═══════════════════════════════════════════════════════════════════════
   Pixel helper
   ═══════════════════════════════════════════════════════════════════════ */
static void audio_plot(int x, int y, short int c)
{
    if (x < 0 || x >= FB_WIDTH)  return;
    if (y < 0 || y >= FB_HEIGHT) return;
    *(volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = c;
}

static short int audio_read_pixel(int x, int y)
{
    if (x < 0 || x >= FB_WIDTH)  return 0;
    if (y < 0 || y >= FB_HEIGHT) return 0;
    return *(volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1));
}

/* ═══════════════════════════════════════════════════════════════════════
   Playhead draw / erase  — safe pixel save & restore
   ─────────────────────────────────────────────────────────────────────
   draw_playhead: read every pixel that the bar will cover into ph_save[][],
                  then paint those pixels PLAYHEAD_COLOR.
   erase_playhead: write ph_save[][] back pixel-for-pixel.
   No bg[], no redraw_all_notes — nothing else is ever touched.
   ═══════════════════════════════════════════════════════════════════════ */
static int ph_px;   /* x saved between draw and erase */
static int ph_s;    /* staff saved between draw and erase */

static void draw_playhead(int col, int s)
{
    int x, y, xi, yi;
    int px    = col_to_x_audio(col);
    int y_top = PH_Y_TOP(s);
    int y_bot = PH_Y_BOT(s);

    ph_px = px;
    ph_s  = s;

    yi = 0;
    for (y = y_top; y <= y_bot && yi < PH_SAVE_H; y++, yi++) {
        xi = 0;
        for (x = px - PLAYHEAD_W; x <= px + PLAYHEAD_W; x++, xi++) {
            ph_save[yi][xi] = audio_read_pixel(x, y);
            audio_plot(x, y, PLAYHEAD_COLOR);
        }
    }
}

static void erase_playhead(void)
{
    int x, y, xi, yi;
    int y_top = PH_Y_TOP(ph_s);
    int y_bot = PH_Y_BOT(ph_s);

    yi = 0;
    for (y = y_top; y <= y_bot && yi < PH_SAVE_H; y++, yi++) {
        xi = 0;
        for (x = ph_px - PLAYHEAD_W; x <= ph_px + PLAYHEAD_W; x++, xi++) {
            audio_plot(x, y, ph_save[yi][xi]);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   Square-wave oscillator
   ═══════════════════════════════════════════════════════════════════════ */
#define MAX_SIMULTANEOUS 8

typedef struct { int freq; int half_period; int counter; int sign; } Osc;

static void osc_init(Osc *o, int freq)
{
    o->freq = freq;
    o->half_period = (freq > 0) ? FS_HZ / (2 * freq) : 0;
    if (o->half_period < 1 && freq > 0) o->half_period = 1;
    o->counter = o->half_period;
    o->sign    = 1;
}

static int32_t osc_tick(Osc *o)
{
    int32_t out;
    if (o->freq == 0 || o->half_period == 0) return 0;
    out = (int32_t)(o->sign * SQ_AMP);
    if (--o->counter <= 0) { o->sign = -o->sign; o->counter = o->half_period; }
    return out;
}

/* ═══════════════════════════════════════════════════════════════════════
   play_column  — synthesise one column of notes for COLUMN_TICKS samples
   Only notes on staff s at head_step == col are sounded.
   ═══════════════════════════════════════════════════════════════════════ */
static void play_column(volatile audio_t *audiop, int col, int s)
{
    Osc oscs[MAX_SIMULTANEOUS];
    int n_osc = 0;
    int i, h, t;

    for (i = 0; i < num_notes && n_osc < MAX_SIMULTANEOUS; i++) {
        if (notes[i].staff != s) continue;
        for (h = 0; h < notes[i].num_heads; h++) {
            if (notes[i].head_step[h] != col) continue;
            {
                int slot = notes[i].head_pitch_slot[h];
                int row  = s * SLOTS_PER_STAFF + slot;
                int freq = (row >= 0 && row < 36) ? pitch_freq_row[row] : 0;
                osc_init(&oscs[n_osc++], freq);
            }
            break;
        }
    }

    for (i = n_osc; i < MAX_SIMULTANEOUS; i++) osc_init(&oscs[i], 0);

    for (t = 0; t < COLUMN_TICKS; t++) {
        int32_t mix = 0;
        for (i = 0; i < n_osc; i++) mix += osc_tick(&oscs[i]);
        if (n_osc > 1) mix /= n_osc;
        mix = clamp24(mix);
        while (audiop->wsrc == 0 || audiop->wslc == 0) ;
        audiop->ldata = (uint32_t)mix;
        audiop->rdata = (uint32_t)mix;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   play_sequence  — public entry point, called from vga_music_v2.c
   Plays all 4 bars in order with animated green playhead.
   ═══════════════════════════════════════════════════════════════════════ */
void play_sequence(void)
{
    volatile audio_t *audiop     = (volatile audio_t *)AUDIO_BASE;
    int s, col;

    /* Clear audio output FIFOs before starting */
    audiop->control = 0x3;

    for (s = 0; s < NUM_STAVES; s++) {
        for (col = FIRST_COL; col < TOTAL_COLS; col++) {
            /* 1. Save pixels under playhead and paint green bar */
            draw_playhead(col, s);

            /* 2. Synthesise and output audio for this column */
            play_column(audiop, col, s);

            /* 3. Restore exact saved pixels — nothing else disturbed */
            erase_playhead();
        }
    }
}
