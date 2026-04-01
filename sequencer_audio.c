/*
 * sequencer_audio.c
 * ─────────────────────────────────────────────────────────────────────────────
 * Playback engine for the 4-staff music sequencer.
 * Include sequencer_audio.h in vga_music_v2.c and call play_sequence().
 */

#include <stdint.h>
#include "background.h"
#include "sequencer_audio.h"
#include "toolbar.h"
#include "piano_samples.h"
#include "xylophone_samples.h"

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

/* ═══════════════════════════════════════════════════════════════════════
   Grid constants  (must match background.h / vga_music_v2.c)
   ═══════════════════════════════════════════════════════════════════════ */
#define SLOTS_PER_STAFF  ((LINES_PER_STAFF - 1) * 2 + 3)   /* 11 */
#define TOTAL_ROWS       (NUM_STAVES * SLOTS_PER_STAFF)     /* 44 */
#define TOTAL_COLS       NUM_STEPS                          /* 16 */
#define FIRST_COL        1   /* cols 0 overlap treble clef */

/* =======================================================================
   Note struct  (must be identical to main.c)
   ======================================================================= */
#define MAX_NOTES  512
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
    int page;
} Note;
#endif /* NOTE_STRUCT_DEFINED */

/* Variables living in main.c that we need to read/write */
extern Note      notes[MAX_NOTES];
extern int       num_notes;
extern int       pixel_buffer_start;
extern short int bg[FB_HEIGHT][FB_WIDTH];
extern int       cur_page;

/* ADD THESE EXTERNS TO FIX YOUR RED SQUIGGLES IN VS CODE */
extern int       cur_note_type; 
extern void      safe_draw_toolbar(int nt); 

extern volatile int seq_is_playing;
extern volatile int seq_is_paused;

/* ADD THESE TO TALK TO MAIN.C */
extern volatile int seq_user_stopped;
extern volatile int seq_user_restarted;

/* ═══════════════════════════════════════════════════════════════════════
   Pitch table - slot 0 (top line, F5) -> slot 8 (bottom line, E4)
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
     831, 740, 698, 622, 554, 523, 466, 415, 370, 349, 311 
};

static const int pitch_freq_slot_flat[11] = {
     740, 659, 622, 554, 494, 466, 415, 370, 330, 311, 277  
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
   ═══════════════════════════════════════════════════════════════════════ */
#define FS_HZ          8000

static int quarter_samples_current(void)
{
    int bpm = toolbar_state.bpm;
    if (bpm < 1) bpm = 120;
    return (FS_HZ * 60) / bpm;
}

static int samples_per_64_current(void)
{
    return quarter_samples_current() / 16;
}

#define SQ_AMP  0x7A0000

/* ═══════════════════════════════════════════════════════════════════════
   Visual - playhead geometry
   ═══════════════════════════════════════════════════════════════════════ */
#define PLAYHEAD_COLOR  ((short int)0x07E0)   /* bright green RGB565 */
#define PLAYHEAD_W      2                     /* pixels each side of centre */

#define PH_Y_TOP(s)  (staff_top[s] - (STAFF_SPACING / 2) - 2)
#define PH_Y_BOT(s)  (staff_top[s] + (LINES_PER_STAFF - 1) * STAFF_SPACING + (STAFF_SPACING / 2) + 2)
#define PH_HEIGHT    (PH_Y_BOT(0) - PH_Y_TOP(0) + 1)

#define PH_SAVE_W    (2 * PLAYHEAD_W + 1)
#define PH_SAVE_H    40
static short int ph_save[PH_SAVE_H][PH_SAVE_W];

/* ═══════════════════════════════════════════════════════════════════════
   Grid & Pixel helpers
   ═══════════════════════════════════════════════════════════════════════ */
static int col_to_x_audio(int col)
{
    if (col < FIRST_COL)   col = FIRST_COL;
    if (col >= TOTAL_COLS) col = TOTAL_COLS - 1;
    return STAFF_X0 + col * STEP_W + STEP_W / 2;
}

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
   Playhead draw / erase
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
   Oscillator & Sampling
   ═══════════════════════════════════════════════════════════════════════ */
typedef struct {
    int      freq;
    uint32_t phase;
    uint32_t phase_inc;   
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
    out = (o->phase < 0x8000u) ? (int32_t)SQ_AMP : -(int32_t)SQ_AMP;
    o->phase = (o->phase + o->phase_inc) & 0xFFFFu;
    return out;
}

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

static void play_piano_buf(volatile audio_t *audiop,
                           const int16_t *buf, int buf_len,
                           int num_samples)
{
    int t;
    for (t = 0; t < num_samples; t++) {
        int32_t s;
        if (buf_len > 0) {
            int idx = t % buf_len;
            s = (int32_t)buf[idx] * 350; 
            s = clamp24(s);
        } else {
            s = 0;
        }
        while (audiop->wsrc == 0 || audiop->wslc == 0)
            ;
        audiop->ldata = (uint32_t)s;
        audiop->rdata = (uint32_t)s;
    }
}

static void get_sample_buf(int inst, int slot, int accidental,
                           const int16_t **out_buf, int *out_len)
{
    if (slot < 0 || slot >= 11) { *out_buf = 0; *out_len = 0; return; }

    /* Select the right table set based on instrument */
    const int16_t * const *nat_tbl;
    const int              *nat_len;
    const int16_t * const *sharp_tbl;
    const int              *sharp_len;
    const int16_t * const *flat_tbl;
    const int              *flat_len;

    switch (inst) {
    case TB_INST_XYLOPHONE:
        nat_tbl   = xylophone_nat_table;   nat_len   = xylophone_nat_len_table;
        sharp_tbl = xylophone_sharp_table; sharp_len = xylophone_sharp_len_table;
        flat_tbl  = xylophone_flat_table;  flat_len  = xylophone_flat_len_table;
        break;
    case TB_INST_PIANO_REVERB:
        nat_tbl   = piano_rev_nat_table;   nat_len   = piano_rev_nat_len_table;
        sharp_tbl = piano_rev_sharp_table; sharp_len = piano_rev_sharp_len_table;
        flat_tbl  = piano_rev_flat_table;  flat_len  = piano_rev_flat_len_table;
        break;
    default: /* TB_INST_PIANO */
        nat_tbl   = piano_nat_table;   nat_len   = piano_nat_len_table;
        sharp_tbl = piano_sharp_table; sharp_len = piano_sharp_len_table;
        flat_tbl  = piano_flat_table;  flat_len  = piano_flat_len_table;
        break;
    }

    switch (accidental) {
    case ACC_SHARP:
        *out_buf = sharp_tbl[slot]; *out_len = sharp_len[slot]; break;
    case ACC_FLAT:
        *out_buf = flat_tbl[slot];  *out_len = flat_len[slot];  break;
    default:
        *out_buf = nat_tbl[slot];   *out_len = nat_len[slot];   break;
    }
}

/* kept for backward compatibility */
static void get_piano_buf(int slot, int accidental, int reverb,
                           const int16_t **out_buf, int *out_len)
{
    int inst = reverb ? TB_INST_PIANO_REVERB : TB_INST_PIANO;
    get_sample_buf(inst, slot, accidental, out_buf, out_len);
}

/* ═══════════════════════════════════════════════════════════════════════
   play_column
   ═══════════════════════════════════════════════════════════════════════ */
static void play_column(volatile audio_t *audiop, int col, int s)
{
    int i, h;
    int found = 0;
    int inst = toolbar_state.instrument;

    for (i = 0; i < num_notes; i++) {
        /* Only check notes on the currently active page and staff */
        if (notes[i].staff != s || notes[i].page != cur_page) continue;

        for (h = 0; h < notes[i].num_heads; h++) {
            if (notes[i].head_step[h] != col) continue;

            {
                int slot    = notes[i].head_pitch_slot[h];
                int total_s = notes[i].duration_64 * samples_per_64_current();
                int nh      = notes[i].num_heads;
                int head_s  = (nh > 1) ? (total_s / nh) : total_s;

                if (notes[i].note_type == NOTE_REST) {
                    silence(audiop, head_s);
                } else if (inst == TB_INST_BEEP) {
                    Osc osc;
                    int freq = note_frequency_hz(slot, notes[i].accidental);
                    osc_init(&osc, freq);
                    play_note_sound(audiop, &osc, head_s);
                } else {
                    const int16_t *buf;
                    int            buf_len;
                    get_sample_buf(inst, slot, notes[i].accidental, &buf, &buf_len);
                    play_piano_buf(audiop, buf, buf_len, head_s);
                }
                found = 1;
            }
            break; 
        }
    }

    if (!found)
        silence(audiop, quarter_samples_current());
}

/* ═══════════════════════════════════════════════════════════════════════
   play_sequence
   ═══════════════════════════════════════════════════════════════════════ */
void play_sequence(void)
{
    volatile audio_t *audiop = (volatile audio_t *)AUDIO_BASE;
    volatile int *ps2 = (volatile int *)0xFF200100; /* PS2_BASE */
    int s, col;
    
    int got_break = 0;

    audiop->control = 0xC;
    audiop->control = 0x0;

    for (s = 0; s < NUM_STAVES; s++) {
        if (!seq_is_playing) break; 

        for (col = FIRST_COL; col < TOTAL_COLS; col++) {
            if (!seq_is_playing) break; 

            while (1) {
                int raw = *ps2;
                
                if (raw & 0x8000) { 
                    unsigned char b = (unsigned char)(raw & 0xFF);

                    if (b == 0xE0) continue;
                    if (b == 0xF0) { got_break = 1; continue; } 
                    if (got_break) { got_break = 0; continue; }

                    if (b == 0x15) { 
                        seq_is_paused = 0; 
                        safe_draw_toolbar(cur_note_type);
                    }
                    if (b == 0x24) { 
                        seq_is_paused = 1; 
                        safe_draw_toolbar(cur_note_type);
                    }
                    if (b == 0x2C) { 
                        seq_is_playing = 0;
                        seq_is_paused = 0; 
                        seq_user_stopped = 1; /* Notify main.c to break the page loop */
                    }
                    if (b == 0x2D) {
                        seq_is_playing = 0;
                        seq_is_paused = 0; 
                        seq_user_restarted = 1; /* Notify main.c to restart at Page 1 */
                    }
                }

                if (!seq_is_paused) break;
            }

            if (!seq_is_playing) break; 

            draw_playhead(col, s);
            play_column(audiop, col, s);
            erase_playhead();
        }
    }
    
    /* Final cleanup */
    audiop->control = 0xC;
    audiop->control = 0x0;

    seq_is_playing = 0;
}