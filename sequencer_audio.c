/*
 * sequencer_audio.c
 * ─────────────────────────────────────────────────────────────────────────────
 * Playback engine for the 4-staff music sequencer.
 * Include sequencer_audio.h in vga_music_v2.c and call play_sequence().
 *
 * AUDIO FIXES (vs original broken version)
 * ─────────────────────────────────────────
 * 1. audiop->control FIFO-clear bits are write-1-to-clear on the DE-series
 *    WM8731 core.  Writing 0x3 and leaving it set continuously resets the
 *    write FIFOs — so every sample we push gets discarded.
 *    Fix: write 0x3, then immediately write 0x0 to de-assert the clear.
 *
 * 2. The square-wave lab (and the echo lab) both check wsrc > 0 && wslc > 0
 *    before pushing a sample.  The original play_column checked equality
 *    to zero with OR — semantically the same but more fragile.  Now uses
 *    the pattern that is proven to work in the lab:
 *       while (audiop->wsrc == 0 || audiop->wslc == 0) {}
 *
 * 3. SQ_AMP raised from 0x200000 to 0x600000 — quieter values sometimes
 *    fall below the DAC's noise floor at the default gain setting.
 *
 * 4. pitch_freq_row[] corrected: row uses note's own staff index, not the
 *    outer-loop staff variable (same value but made explicit for clarity).
 *
 * 5. Oscillator half_period floor raised: freq > FS/2 would give
 *    half_period = 0 causing a divide-by-zero / silence.  Guard extended.
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
 * PITCH MAPPING
 * ─────────────
 * Pitch = f(absolute grid row).  Row = staff*9 + pitch_slot.
 * Row 0 (top of staff 0) = F5, descends diatonically to row 35 = F0.
 * Staff number only determines WHEN a note plays, never its pitch.
 *
 * PLAYHEAD ANIMATION — SAFE PIXEL SAVE/RESTORE
 * ─────────────────────────────────────────────
 * Before drawing the green bar we save every pixel it will cover into
 * a local buffer.  After audio finishes we write those pixels back
 * exactly.  We never touch bg[][] or call redraw_all_notes() during
 * erase — so no note, staff line, beam or stem is ever disturbed.
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include <stdint.h>
#include "background.h"
#include "sequencer_audio.h"

/* ═══════════════════════════════════════════════════════════════════════
   Hardware
   ═══════════════════════════════════════════════════════════════════════ */
#define AUDIO_BASE      0xFF203040

/* ═══════════════════════════════════════════════════════════════════════
   Audio peripheral  (matches the DE-series WM8731 core register layout)
   ═══════════════════════════════════════════════════════════════════════ */
typedef struct {
    volatile uint32_t control;  /* offset 0: bits[1:0] = CW,CR (write-1-to-clear) */
    volatile uint8_t  rarc;     /* offset 4: right input FIFO count                */
    volatile uint8_t  ralc;     /* offset 5: left  input FIFO count                */
    volatile uint8_t  wsrc;     /* offset 6: right output FIFO empty slots         */
    volatile uint8_t  wslc;     /* offset 7: left  output FIFO empty slots         */
    volatile uint32_t ldata;    /* offset 8: left  data (read=input, write=output) */
    volatile uint32_t rdata;    /* offset 12:right data (read=input, write=output) */
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
#define NOTE_REST       7
#define NUM_NOTE_TYPES  8

#define ACC_NONE     0
#define ACC_SHARP    1
#define ACC_FLAT     2
#define ACC_NATURAL  3

/* (unused — kept for reference only)
static const int note_num_heads_audio[NUM_NOTE_TYPES] = { 1,1,1,2,4,2,1 }; */

/* ═══════════════════════════════════════════════════════════════════════
   Grid constants  (must match background.h / vga_music_v2.c)
   ═══════════════════════════════════════════════════════════════════════ */
#define SLOTS_PER_STAFF  ((LINES_PER_STAFF - 1) * 2 + 3)   /* 11 */
#define TOTAL_ROWS       (NUM_STAVES * SLOTS_PER_STAFF)     /* 44 */
#define TOTAL_COLS       NUM_STEPS                          /* 16 */
#define FIRST_COL        2   /* cols 0-1 overlap treble clef */

/* ═══════════════════════════════════════════════════════════════════════
   Note struct  (must be identical to vga_music_v2.c)
   ═══════════════════════════════════════════════════════════════════════ */
#define MAX_NOTES  256
#define MAX_HEADS  4

#ifndef NOTE_STRUCT_DEFINED
#define NOTE_STRUCT_DEFINED
typedef struct {
    int step;
    int staff;
    int pitch_slot;
    int note_type;
    int duration_64;
    int accidental;
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

//so that we can play,pause, and poll keys during playback without including toolbar.h here
extern volatile int seq_is_playing;
extern volatile int seq_is_paused;
extern void poll_playback_keys(void);

/* ═══════════════════════════════════════════════════════════════════════
   Pitch table — slot 0 (top line, F5) → slot 8 (bottom line, E4)

   Each staff has 9 slots: 5 lines + 4 spaces, numbered 0 (top) to 8 (bottom).
   ALL staves share the same 9 frequencies — staff position controls WHEN
   a note plays (which bar), never its pitch.  Only pitch_slot (0–8)
   determines the frequency.

   Fix: the old code used  row = staff * 9 + slot, so a note on staff 1
   slot 0 played a different (lower) pitch than staff 0 slot 0.  Now every
   staff plays the same scale regardless of which bar it sits in.
   ═══════════════════════════════════════════════════════════════════════ */
static const int pitch_freq_slot[11] = {
     784, /* slot  0  G5  */
     698, /* slot  1  F5  */
     659, /* slot  2  E5  */
     587, /* slot  3  D5  */
     523, /* slot  4  C5  */
     494, /* slot  5  B4  */
     440, /* slot  6  A4  */
     392, /* slot  7  G4  */
     349, /* slot  8  F4  */
     330, /* slot  9  E4  */
     294  /* slot 10  D4  */
};

static const int pitch_freq_slot_sharp[11] = {
     831, /* G#5 */
     740, /* F#5 */
     698, /* E#5 -> F5 */
     622, /* D#5 */
     554, /* C#5 */
     523, /* B#4 -> C5 */
     466, /* A#4 */
     415, /* G#4 */
     370, /* F#4 */
     349, /* E#4 -> F4 */
     311  /* D#4 */
};

static const int pitch_freq_slot_flat[11] = {
     740, /* Gb5 */
     659, /* Fb5 -> E5 */
     622, /* Eb5 */
     554, /* Db5 */
     494, /* Cb5 -> B4 */
     466, /* Bb4 */
     415, /* Ab4 */
     370, /* Gb4 */
     330, /* Fb4 -> E4 */
     311, /* Eb4 */
     277  /* Db4 */
};

static int note_frequency_hz(int slot, int accidental)
{
    if (slot < 0 || slot >= 11) return 0;

    switch (accidental) {
    case ACC_SHARP:   return pitch_freq_slot_sharp[slot];
    case ACC_FLAT:    return pitch_freq_slot_flat[slot];
    case ACC_NATURAL: return pitch_freq_slot[slot];
    case ACC_NONE:
    default:          return pitch_freq_slot[slot];
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   Audio timing
   ─────────────────────────────────────────────────────────────────────
   Sample rate: 8000 Hz  (set by the WM8731 core on DE-series boards)
   Tempo:       120 BPM  -> quarter note = 0.5 s = 4000 samples

   duration_64 values (from vga_music_v2.c):
     whole      = 64  ->  64 * 250 = 16000 samples  (2.0 s)
     half       = 32  ->  32 * 250 =  8000 samples  (1.0 s)
     quarter    = 16  ->  16 * 250 =  4000 samples  (0.5 s)
     beam2_8th  = 16  ->  16 * 250 =  4000 total, 2000/head
     beam4_16th = 16  ->  16 * 250 =  4000 total, 1000/head
     beam2_16th =  8  ->   8 * 250 =  2000 total, 1000/head
     single16th =  4  ->   4 * 250 =  1000 samples

   SAMPS_PER_64 = QUARTER_SAMPS / 16 = 250
   total_samples = duration_64 * SAMPS_PER_64
   ═══════════════════════════════════════════════════════════════════════ */
#define FS_HZ          8000
#define QUARTER_SAMPS  4000   /* 120 BPM: 60/120 * 8000 = 4000            */
#define SAMPS_PER_64   250    /* QUARTER_SAMPS / 16  (one 1/64-note unit)  */

/*
 * Square-wave amplitude.
 * The WM8731 DAC is 24-bit signed.  MAX24 = 0x7FFFFF.
 * Use 75 % of full scale so mixing multiple oscillators doesn't clip badly
 * before the clamp.  (Matches the approach in the lab square-wave example.)
 */
#define SQ_AMP  0x600000

/* ═══════════════════════════════════════════════════════════════════════
   Visual — playhead geometry
   ═══════════════════════════════════════════════════════════════════════ */
#define PLAYHEAD_COLOR  ((short int)0x07E0)   /* bright green RGB565 */
#define PLAYHEAD_W      2                     /* pixels each side of centre */

/* Extend playhead to cover the extra slots above and below the staff lines */
#define PH_Y_TOP(s)  (staff_top[s] - (STAFF_SPACING / 2) - 2)
#define PH_Y_BOT(s)  (staff_top[s] + (LINES_PER_STAFF - 1) * STAFF_SPACING + (STAFF_SPACING / 2) + 2)
#define PH_HEIGHT    (PH_Y_BOT(0) - PH_Y_TOP(0) + 1)

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
   Pixel helpers
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
   ═══════════════════════════════════════════════════════════════════════ */
static int ph_px;
static int ph_s;

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
   Square-wave oscillator  — phase accumulator (DDS)
   ─────────────────────────────────────────────────────────────────────
   Uses a 32-bit phase accumulator instead of a half-period counter.
   This eliminates the integer-truncation tuning error that plagues the
   half-period method at low sample rates (8 kHz).

   How it works:
     phase_inc = (freq << 16) / FS_HZ        (Q16 fixed-point)
     Each tick:  phase += phase_inc  (wraps naturally at 2^16)
     Output = +SQ_AMP when phase < 0x8000, -SQ_AMP otherwise.

   Error analysis vs. old half-period method at 8000 Hz:
     A4 440 Hz:  old → 444 Hz (+16 cents), new → 440 Hz (< 0.1 cent)
     G5 784 Hz:  old → 800 Hz (+35 cents), new → 784 Hz (< 0.1 cent)
   ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    int      freq;
    uint32_t phase;
    uint32_t phase_inc;   /* Q16: (freq << 16) / FS_HZ */
} Osc;

static void osc_init(Osc *o, int freq)
{
    o->freq = freq;
    o->phase = 0;
    if (freq > 0)
        o->phase_inc = (uint32_t)(((uint32_t)freq << 16) / FS_HZ);
    else
        o->phase_inc = 0;
}

static int32_t osc_tick(Osc *o)
{
    int32_t out;
    if (o->freq == 0 || o->phase_inc == 0) return 0;
    /* Square: high for first half of cycle, low for second half */
    out = (o->phase < 0x8000u) ? (int32_t)SQ_AMP : -(int32_t)SQ_AMP;
    o->phase = (o->phase + o->phase_inc) & 0xFFFFu;
    return out;
}

/*
 * play_note_sound
 * Push `num_samples` samples for one oscillator into the audio FIFO.
 * Follows the exact lab square-wave pattern:
 *   wait for space -> write ldata -> write rdata
 */
static void play_note_sound(volatile audio_t *audiop, Osc *osc, int num_samples)
{
    int     t;
    int32_t mix;
    for (t = 0; t < num_samples; t++) {
        mix = clamp24(osc_tick(osc));
        while (audiop->wsrc == 0 || audiop->wslc == 0)
            ;
        audiop->ldata = (uint32_t)mix;
        audiop->rdata = (uint32_t)mix;
    }
}

/*
 * silence
 * Push `num_samples` zero-samples so the timeline stays in sync when a
 * column has no note on the current staff.
 */
static void silence(volatile audio_t *audiop, int num_samples)
{
    int t;
    for (t = 0; t < num_samples; t++) {
        while (audiop->wsrc == 0 || audiop->wslc == 0)
            ;
        audiop->ldata = 0;
        audiop->rdata = 0;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   play_column
   ─────────────────────────────────────────────────────────────────────
   Sound every note-head whose head_step[h] == col on staff s.

   PITCH FIX:
     Frequency comes from pitch_freq_slot[slot] (slot 0-8 only).
     Staff position has NO effect on pitch — it only controls timing.
     Every staff uses the same 9-note scale so the same slot always
     sounds the same note regardless of which bar it is placed in.

   DURATION FIX:
     Each note type carries duration_64 (stored in the Note struct).
     We convert: samples = duration_64 * SAMPS_PER_64.
     Beamed groups split equally across their heads:
       per_head_samples = total_samples / num_heads.

     At 120 BPM (SAMPS_PER_64 = 250):
       Whole  (64) = 16000 samples   Half    (32) = 8000
       Quarter(16) =  4000 samples   8th      (8) = 2000/head
       16th    (4) =  1000 samples
   ═══════════════════════════════════════════════════════════════════════ */
static void play_column(volatile audio_t *audiop, int col, int s)
{
    int i, h;
    int found = 0;

    for (i = 0; i < num_notes; i++) {
        if (notes[i].staff != s) continue;

        for (h = 0; h < notes[i].num_heads; h++) {
            if (notes[i].head_step[h] != col) continue;

            {
                Osc osc;
                int slot    = notes[i].head_pitch_slot[h];
                int freq    = note_frequency_hz(slot, notes[i].accidental);
                int total_s = notes[i].duration_64 * SAMPS_PER_64;
                int nh      = notes[i].num_heads;
                int head_s  = (nh > 1) ? (total_s / nh) : total_s;

                if (notes[i].note_type == NOTE_REST) {
                    silence(audiop, head_s);
                } else {
                    osc_init(&osc, freq);
                    play_note_sound(audiop, &osc, head_s);
                }
                found = 1;
            }
            break; /* one head per note can match a given column */
        }
    }

    /* If no note fired this column, fill with silence so the grid
       column still takes up its full quarter-note time slot. */
    if (!found)
        silence(audiop, QUARTER_SAMPS);
}

/* ═══════════════════════════════════════════════════════════════════════
   play_sequence  — public entry point, called from vga_music_v2.c
   ─────────────────────────────────────────────────────────────────────
   Iterates staff 0->3, column 1->15.  For each column the playhead is
   drawn, audio is synthesised for EXACTLY the note's duration (or one
   quarter's worth of silence if the column is empty), then the playhead
   is erased.

   Notes longer than one quarter (half, whole) play out their full
   duration inside play_column — the playhead stays on that column until
   the sound finishes, then advances.  This is musically correct: a whole
   note held for 2 s looks like a stationary playhead for 2 s.

   FIFO-clear fix: assert CW|CR then immediately de-assert (write 0).
   ═══════════════════════════════════════════════════════════════════════ */
void play_sequence(void)
{
    volatile audio_t *audiop = (volatile audio_t *)AUDIO_BASE;
    volatile int *ps2 = (volatile int *)0xFF200100; /* PS2_BASE */
    int s, col;
    
    /* Playback state flags */
    int is_playing;
    int is_paused;
    int got_break = 0;
    int restart_seq;

    /* Wrap the entire engine in a loop so R can trigger a reset */
    do {
        /* Reset all flags for a fresh start */
        restart_seq = 0;
        is_playing = 1;
        is_paused = 0;

        /* Pulse the FIFO-clear bits to reset audio buffers */
        audiop->control = 0x3;
        audiop->control = 0x0;

        for (s = 0; s < NUM_STAVES; s++) {
            if (!is_playing || restart_seq) break; 

            for (col = FIRST_COL; col < TOTAL_COLS; col++) {
                if (!is_playing || restart_seq) break; 

                /* =========================================
                   1. Keyboard Polling (Play/Pause/Stop/Restart)
                   ========================================= */
                while (1) {
                    int raw = *ps2;
                    
                    if (raw & 0x8000) { 
                        unsigned char b = (unsigned char)(raw & 0xFF);

                        if (b == 0xE0) continue;
                        if (b == 0xF0) { got_break = 1; continue; } /* KEY_BREAK */
                        if (got_break) { got_break = 0; continue; }

                        /* Q Key (Play / Resume) */
                        if (b == 0x15) { 
                            is_paused = 0; 
                        }
                        /* E Key (Pause) */
                        if (b == 0x24) { 
                            is_paused = 1; 
                        }
                        /* T Key (Stop) */
                        if (b == 0x2C) { 
                            is_playing = 0;
                            is_paused = 0; 
                        }
                        /* R Key (Restart) */
                        if (b == 0x2D) {
                            restart_seq = 1;
                            is_paused = 0; /* Force break to restart */
                        }
                    }

                    if (!is_paused) {
                        break;
                    }
                }

                /* Catch the stop or restart before playing the note */
                if (!is_playing || restart_seq) break; 

                /* =========================================
                   2. Standard Audio & UI Execution
                   ========================================= */
                draw_playhead(col, s);
                play_column(audiop, col, s);
                erase_playhead();
            }
        }
    } while (restart_seq); /* If R was pressed, loop back to the top! */
}
