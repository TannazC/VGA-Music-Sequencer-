/**
 * @file sequencer_audio.c
 * @brief Playback engine for the 4-staff music sequencer.
 *
 * Handles all audio output: square-wave oscillator (Beep), PCM sample
 * playback (Piano, Piano Reverb, Xylophone), timing, and the animated
 * green playhead. Entry point is play_sequence().
 *
 * @authors Tannaz Chowdhury, Dareen Nasreldin
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

/** @brief Memory-mapped base address of the WM8731 audio core. */
#define AUDIO_BASE 0xFF203040

/**
 * @brief Register layout of the DE1-SoC WM8731 audio peripheral.
 *
 * All fields are volatile because the hardware updates them independently
 * of the CPU. ldata/rdata are written to output audio samples.
 */
typedef struct {
    volatile uint32_t control; /**< Offset 0:  bits[1:0] = CW, CR (write-1-to-clear) */
    volatile uint8_t  rarc;    /**< Offset 4:  right input FIFO count                 */
    volatile uint8_t  ralc;    /**< Offset 5:  left input FIFO count                  */
    volatile uint8_t  wsrc;    /**< Offset 6:  right output FIFO empty slots          */
    volatile uint8_t  wslc;    /**< Offset 7:  left output FIFO empty slots           */
    volatile uint32_t ldata;   /**< Offset 8:  left channel  (write = output sample)  */
    volatile uint32_t rdata;   /**< Offset 12: right channel (write = output sample)  */
} audio_t;

/** @brief Maximum positive value for a 24-bit signed audio sample. */
#define MAX24  0x007FFFFF

/** @brief Minimum negative value for a 24-bit signed audio sample. */
#define MIN24 (-0x00800000)

/**
 * @brief Clamps a 32-bit value to the 24-bit signed range accepted by the DAC.
 *
 * @param x Input sample value.
 * @return Clamped value in [MIN24, MAX24].
 */
static inline int32_t clamp24(int32_t x)
{
    if (x >  MAX24) return  MAX24;
    if (x <  MIN24) return  MIN24;
    return x;
}

/* ═══════════════════════════════════════════════════════════════════════
   Note type constants  (must match vga_music_v2.c exactly)
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

#define ACC_NONE    0
#define ACC_SHARP   1
#define ACC_FLAT    2
#define ACC_NATURAL 3

/* ═══════════════════════════════════════════════════════════════════════
   Grid constants  (must match background.h / vga_music_v2.c)
   ═══════════════════════════════════════════════════════════════════════ */

/** @brief Pitch slots per staff: lines, spaces, and two ledger positions. */
#define SLOTS_PER_STAFF  ((LINES_PER_STAFF - 1) * 2 + 3)  /* 11 */

/** @brief Total pitch rows across all staves. */
#define TOTAL_ROWS       (NUM_STAVES * SLOTS_PER_STAFF)    /* 44 */

/** @brief Total column count (matches NUM_STEPS). */
#define TOTAL_COLS       NUM_STEPS                         /* 17 */

/** @brief First playable column; column 0 is occupied by the treble clef. */
#define FIRST_COL        1

/* ═══════════════════════════════════════════════════════════════════════
   Note struct  (must be identical to vga_music_v2.c)
   ═══════════════════════════════════════════════════════════════════════ */
#define MAX_NOTES 512
#define MAX_HEADS 4

#ifndef NOTE_STRUCT_DEFINED
#define NOTE_STRUCT_DEFINED
/**
 * @brief Represents a single placed note or rest on the sequencer grid.
 *
 * Multi-head notes (beamed groups) store up to MAX_HEADS head positions
 * within one struct. All heads share the same duration_64 and accidental.
 */
typedef struct {
    int step;                      /**< Grid column of the first head.          */
    int staff;                     /**< Staff index (0 = top).                  */
    int pitch_slot;                /**< Pitch row of the first head (0–10).     */
    int note_type;                 /**< One of NOTE_WHOLE … NOTE_REST.          */
    int duration_64;               /**< Duration in 64th-note units.            */
    int accidental;                /**< ACC_NONE, ACC_SHARP, ACC_FLAT, ACC_NAT. */
    int num_heads;                 /**< Number of note heads (1–4).             */
    int head_step[MAX_HEADS];      /**< Column index for each head.             */
    int head_pitch_slot[MAX_HEADS];/**< Pitch slot for each head.               */
    int screen_x;                  /**< Pixel X of the first head.              */
    int screen_y;                  /**< Pixel Y of the first head.              */
    int head_x[MAX_HEADS];         /**< Pixel X for each head.                  */
    int head_y[MAX_HEADS];         /**< Pixel Y for each head.                  */
    int page;                      /**< Page number this note belongs to.       */
} Note;
#endif /* NOTE_STRUCT_DEFINED */

/* ── Externs from vga_music_v2.c ── */
extern Note      notes[MAX_NOTES];
extern int       num_notes;
extern int       pixel_buffer_start;
extern short int bg[FB_HEIGHT][FB_WIDTH];
extern int       cur_page;
extern int       cur_note_type;
extern void      safe_draw_toolbar(int nt);

extern volatile int seq_is_playing;
extern volatile int seq_is_paused;
extern volatile int seq_user_stopped;
extern volatile int seq_user_restarted;

/** @brief Page containing the last note; play_sequence() stops after this page. */
extern int seq_last_note_page;

/** @brief Staff containing the last note on seq_last_note_page. */
extern int seq_last_note_staff;

/** @brief Column containing the last note; playback stops after this column. */
extern int seq_last_note_col;

/* ═══════════════════════════════════════════════════════════════════════
   Pitch tables
   slot 0 = G5 (top of staff)  →  slot 10 = D4 (bottom of staff)
   ═══════════════════════════════════════════════════════════════════════ */

/** @brief Natural frequencies (Hz) indexed by pitch slot. */
static const int pitch_freq_slot[11] = {
    784, 698, 659, 587, 523, 494, 440, 392, 349, 330, 294
};

/** @brief Sharp frequencies (Hz) indexed by pitch slot. */
static const int pitch_freq_slot_sharp[11] = {
    831, 740, 698, 622, 554, 523, 466, 415, 370, 349, 311
};

/** @brief Flat frequencies (Hz) indexed by pitch slot. */
static const int pitch_freq_slot_flat[11] = {
    740, 659, 622, 554, 494, 466, 415, 370, 330, 311, 277
};

/**
 * @brief Returns the playback frequency for a given pitch slot and accidental.
 *
 * @param slot       Pitch slot index (0–10). Out-of-range returns 0.
 * @param accidental One of ACC_NONE, ACC_SHARP, ACC_FLAT, ACC_NATURAL.
 * @return Frequency in Hz, or 0 if slot is invalid.
 */
static int note_frequency_hz(int slot, int accidental)
{
    if (slot < 0 || slot >= 11) return 0;
    switch (accidental) {
    case ACC_SHARP:   return pitch_freq_slot_sharp[slot];
    case ACC_FLAT:    return pitch_freq_slot_flat[slot];
    case ACC_NATURAL:
    case ACC_NONE:
    default:          return pitch_freq_slot[slot];
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   Audio timing
   ═══════════════════════════════════════════════════════════════════════ */

/** @brief DAC sample rate in Hz. Fixed by the WM8731 configuration. */
#define FS_HZ 8000

/**
 * @brief Returns the number of samples that represent one quarter note at
 *        the current BPM.
 *
 * Formula: (FS_HZ * 60) / BPM. Defaults to 120 BPM if toolbar value is
 * invalid.
 *
 * @return Sample count for one quarter note.
 */
static int quarter_samples_current(void)
{
    int bpm = toolbar_state.bpm;
    if (bpm < 1) bpm = 120;
    return (FS_HZ * 60) / bpm;
}

/**
 * @brief Returns the number of samples per 1/64th-note unit.
 *
 * All note durations are stored as multiples of this unit (duration_64).
 *
 * @return quarter_samples / 16.
 */
static int samples_per_64_current(void)
{
    return quarter_samples_current() / 16;
}

/** @brief Amplitude of the square-wave beep oscillator (24-bit scale). */
#define SQ_AMP 0x7A0000

/* ═══════════════════════════════════════════════════════════════════════
   Playhead geometry
   ═══════════════════════════════════════════════════════════════════════ */

/** @brief RGB565 colour of the playhead bar (bright green). */
#define PLAYHEAD_COLOR ((short int)0x07E0)

/** @brief Half-width of the playhead bar in pixels (bar = 2*W+1 wide). */
#define PLAYHEAD_W 2

/** @brief Top pixel of the playhead on staff s (2 px above the top line). */
#define PH_Y_TOP(s) (staff_top[s] - (STAFF_SPACING / 2) - 2)

/** @brief Bottom pixel of the playhead on staff s (2 px below bottom line). */
#define PH_Y_BOT(s) (staff_top[s] + (LINES_PER_STAFF - 1) * STAFF_SPACING + (STAFF_SPACING / 2) + 2)

/** @brief Width of the pixel save buffer (one column of the playhead). */
#define PH_SAVE_W (2 * PLAYHEAD_W + 1)

/** @brief Height of the pixel save buffer. Must be >= max playhead height. */
#define PH_SAVE_H 40

/**
 * @brief Saved pixels from the screen region covered by the playhead.
 *
 * Written by draw_playhead() and read back by erase_playhead() to restore
 * the display without referencing the background buffer.
 */
static short int ph_save[PH_SAVE_H][PH_SAVE_W];

/* ═══════════════════════════════════════════════════════════════════════
   Grid & pixel helpers
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Converts a grid column index to the centre X pixel of that column.
 *
 * Clamps col to [FIRST_COL, TOTAL_COLS-1] before computing.
 *
 * @param col Grid column index.
 * @return Centre X pixel coordinate.
 */
static int col_to_x_audio(int col)
{
    if (col < FIRST_COL)   col = FIRST_COL;
    if (col >= TOTAL_COLS) col = TOTAL_COLS - 1;
    return STAFF_X0 + col * STEP_W + STEP_W / 2;
}

/**
 * @brief Writes a pixel directly to the VGA hardware frame buffer.
 *
 * Does not update bg[][]. Used only by the playhead which manages its own
 * save/restore buffer (ph_save).
 *
 * @param x Horizontal coordinate.
 * @param y Vertical coordinate.
 * @param c RGB565 colour.
 */
static void audio_plot(int x, int y, short int c)
{
    if (x < 0 || x >= FB_WIDTH)  return;
    if (y < 0 || y >= FB_HEIGHT) return;
    *(volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = c;
}

/**
 * @brief Reads a pixel directly from the VGA hardware frame buffer.
 *
 * @param x Horizontal coordinate.
 * @param y Vertical coordinate.
 * @return RGB565 colour at (x, y), or 0 if out of bounds.
 */
static short int audio_read_pixel(int x, int y)
{
    if (x < 0 || x >= FB_WIDTH)  return 0;
    if (y < 0 || y >= FB_HEIGHT) return 0;
    return *(volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1));
}

/* ═══════════════════════════════════════════════════════════════════════
   Playhead draw / erase
   ═══════════════════════════════════════════════════════════════════════ */

/** @brief X pixel of the currently drawn playhead column. */
static int ph_px;

/** @brief Staff index of the currently drawn playhead. */
static int ph_s;

/**
 * @brief Draws the playhead bar at the given column on staff s.
 *
 * Saves the pixels underneath into ph_save before overwriting them with
 * PLAYHEAD_COLOR. ph_px and ph_s are stored so erase_playhead() knows
 * where to restore.
 *
 * @param col Grid column to draw the playhead at.
 * @param s   Staff index (0–NUM_STAVES-1).
 */
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

/**
 * @brief Erases the playhead by restoring the pixels saved by draw_playhead().
 *
 * Must be called after play_column() finishes for the current column.
 * Uses ph_px and ph_s set by the preceding draw_playhead() call.
 */
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
   Oscillator
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Fixed-point phase accumulator for square-wave tone generation.
 *
 * phase_inc = (freq << 16) / FS_HZ. Each tick advances the 16-bit phase
 * by phase_inc; output flips sign at the midpoint (0x8000).
 */
typedef struct {
    int      freq;      /**< Target frequency in Hz.                     */
    uint32_t phase;     /**< Current phase accumulator (0–0xFFFF).       */
    uint32_t phase_inc; /**< Phase step per sample = (freq<<16)/FS_HZ.  */
} Osc;

/**
 * @brief Initialises an oscillator for the given frequency.
 *
 * @param o    Pointer to the Osc to initialise.
 * @param freq Target frequency in Hz. Zero produces silence.
 */
static void osc_init(Osc *o, int freq)
{
    o->freq  = freq;
    o->phase = 0;
    o->phase_inc = (freq > 0)
        ? (uint32_t)(((uint32_t)freq << 16) / FS_HZ)
        : 0;
}

/**
 * @brief Advances the oscillator by one sample and returns the output value.
 *
 * Produces +SQ_AMP for the first half of the cycle and -SQ_AMP for the
 * second half, creating a square wave at the configured frequency.
 *
 * @param o Pointer to the Osc to tick.
 * @return 24-bit signed sample value.
 */
static int32_t osc_tick(Osc *o)
{
    int32_t out;
    if (o->freq == 0 || o->phase_inc == 0) return 0;
    out = (o->phase < 0x8000u) ? (int32_t)SQ_AMP : -(int32_t)SQ_AMP;
    o->phase = (o->phase + o->phase_inc) & 0xFFFFu;
    return out;
}

/**
 * @brief Pushes num_samples square-wave samples from osc to the audio FIFO.
 *
 * Busy-waits on wsrc/wslc before each write to avoid overrunning the FIFO.
 *
 * @param audiop      Pointer to the mapped audio peripheral.
 * @param osc         Oscillator providing sample values.
 * @param num_samples Number of samples to output.
 */
static void play_note_sound(volatile audio_t *audiop, Osc *osc, int num_samples)
{
    int t;
    int32_t mix;
    for (t = 0; t < num_samples; t++) {
        mix = clamp24(osc_tick(osc));
        while (audiop->wsrc == 0 || audiop->wslc == 0) ;
        audiop->ldata = (uint32_t)mix;
        audiop->rdata = (uint32_t)mix;
    }
}

/**
 * @brief Pushes num_samples of silence (zero) to the audio FIFO.
 *
 * Used for rests and empty columns to maintain timing.
 *
 * @param audiop      Pointer to the mapped audio peripheral.
 * @param num_samples Number of silent samples to output.
 */
static void silence(volatile audio_t *audiop, int num_samples)
{
    int t;
    for (t = 0; t < num_samples; t++) {
        while (audiop->wsrc == 0 || audiop->wslc == 0) ;
        audiop->ldata = 0;
        audiop->rdata = 0;
    }
}

/**
 * @brief PCM amplitude multiplier applied to all sample-based instruments.
 *
 * Scaled so that piano/xylophone RMS roughly matches the beep square wave.
 */
#define SAMPLE_AMP 768

/**
 * @brief Number of tail samples looped to sustain a piano note.
 *
 * Piano recordings decay to near-silence at ~91% of their length. Looping
 * the final SUSTAIN_LOOP_LEN samples before that region extends the note
 * without replaying the attack transient.
 */
#define SUSTAIN_LOOP_LEN 256

/**
 * @brief Plays a PCM sample buffer into the audio FIFO for exactly
 *        num_samples output samples.
 *
 * Behaviour after the buffer is exhausted depends on one_shot:
 *   - one_shot = 1 (Xylophone): outputs silence — natural percussive decay.
 *   - one_shot = 0 (Piano): loops the last SUSTAIN_LOOP_LEN samples to
 *     simulate a held note.
 *
 * @param audiop      Pointer to the mapped audio peripheral.
 * @param buf         PCM sample array (int16_t, 8 kHz).
 * @param buf_len     Number of samples in buf.
 * @param num_samples Total samples to push (may exceed buf_len).
 * @param one_shot    1 = silence after buf exhausted; 0 = loop tail.
 * @param amp         Amplitude multiplier applied to each sample.
 */
static void play_sample_buf(volatile audio_t *audiop,
                            const int16_t *buf, int buf_len,
                            int num_samples, int one_shot, int amp)
{
    int t;
    for (t = 0; t < num_samples; t++) {
        int32_t s;
        if (buf_len <= 0) {
            s = 0;
        } else if (t < buf_len) {
            s = clamp24((int32_t)buf[t] * amp);
        } else if (one_shot) {
            s = 0;
        } else {
            int loop_start = buf_len - SUSTAIN_LOOP_LEN;
            if (loop_start < 0) loop_start = 0;
            int loop_idx = loop_start + ((t - buf_len) % SUSTAIN_LOOP_LEN);
            s = clamp24((int32_t)buf[loop_idx] * SAMPLE_AMP);
        }
        while (audiop->wsrc == 0 || audiop->wslc == 0) ;
        audiop->ldata = (uint32_t)s;
        audiop->rdata = (uint32_t)s;
    }
}

/**
 * @brief Legacy wrapper for piano playback. Calls play_sample_buf with
 *        sustain enabled and the default SAMPLE_AMP amplitude.
 *
 * @param audiop      Pointer to the mapped audio peripheral.
 * @param buf         PCM sample array.
 * @param buf_len     Number of samples in buf.
 * @param num_samples Total samples to push.
 */
static void play_piano_buf(volatile audio_t *audiop,
                           const int16_t *buf, int buf_len,
                           int num_samples)
{
    play_sample_buf(audiop, buf, buf_len, num_samples, 0, SAMPLE_AMP);
}

/**
 * @brief Resolves a PCM buffer pointer and length for the given instrument,
 *        pitch slot, and accidental.
 *
 * Selects the correct table (nat/sharp/flat) from the appropriate sample
 * header (piano, piano_reverb, xylophone) and writes the pointer and length
 * to the output parameters. Sets both to 0/NULL if slot is out of range.
 *
 * @param inst       Instrument constant (TB_INST_PIANO, TB_INST_XYLOPHONE, etc).
 * @param slot       Pitch slot index (0–10).
 * @param accidental Accidental constant (ACC_NONE, ACC_SHARP, ACC_FLAT).
 * @param out_buf    Output: pointer to the selected PCM array.
 * @param out_len    Output: number of samples in the selected array.
 */
static void get_sample_buf(int inst, int slot, int accidental,
                           const int16_t **out_buf, int *out_len)
{
    if (slot < 0 || slot >= 11) { *out_buf = 0; *out_len = 0; return; }

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

/**
 * @brief Legacy wrapper: resolves a piano PCM buffer by slot, accidental,
 *        and reverb flag.
 *
 * @param slot       Pitch slot (0–10).
 * @param accidental ACC_NONE, ACC_SHARP, or ACC_FLAT.
 * @param reverb     Non-zero selects the piano reverb variant.
 * @param out_buf    Output: pointer to the PCM array.
 * @param out_len    Output: sample count.
 */
static void get_piano_buf(int slot, int accidental, int reverb,
                           const int16_t **out_buf, int *out_len)
{
    int inst = reverb ? TB_INST_PIANO_REVERB : TB_INST_PIANO;
    get_sample_buf(inst, slot, accidental, out_buf, out_len);
}

/* ═══════════════════════════════════════════════════════════════════════
   play_column
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Plays all audio for one grid column on one staff.
 *
 * Scans notes[] for any note on the current page and staff whose head
 * falls on col. Dispatches to silence(), play_note_sound(), or
 * play_sample_buf() based on note_type and the selected instrument.
 * For multi-head notes the total duration is divided evenly across heads.
 * If no note is found, outputs one quarter note of silence to keep timing.
 *
 * @param audiop Pointer to the mapped audio peripheral.
 * @param col    Grid column index to play (FIRST_COL–TOTAL_COLS-1).
 * @param s      Staff index (0–NUM_STAVES-1).
 */
static void play_column(volatile audio_t *audiop, int col, int s)
{
    int i, h;
    int found = 0;
    int inst = toolbar_state.instrument;

    for (i = 0; i < num_notes; i++) {
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
                    int            one_shot = (inst == TB_INST_XYLOPHONE) ? 1 : 0;
                    int            amp      = (inst == TB_INST_XYLOPHONE) ? 1100 : 768;
                    get_sample_buf(inst, slot, notes[i].accidental, &buf, &buf_len);
                    play_sample_buf(audiop, buf, buf_len, head_s, one_shot, amp);
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

/**
 * @brief Plays the full sequence for the current page, staff by staff,
 *        column by column.
 *
 * Called by main() inside a page loop. For each column, polls the PS/2
 * keyboard between columns to handle pause (Q key, 0x15), resume (E key,
 * 0x24), stop (T key, 0x2C), and restart (R key, 0x2D). Pause spins in
 * the polling loop until resumed; stop/restart set flags read by main().
 *
 * Stops early once the column identified by seq_last_note_col on
 * seq_last_note_staff/page is reached, avoiding trailing silence.
 *
 * Flushes the audio FIFO (control = 0xC then 0x0) at entry and exit
 * to discard stale samples.
 *
 * @note PS/2 input is polled, not interrupt-driven. Keyboard presses
 *       during sample output (inside play_column) are not detected until
 *       the current column finishes.
 */
void play_sequence(void)
{
    volatile audio_t *audiop = (volatile audio_t *)AUDIO_BASE;
    volatile int *ps2 = (volatile int *)0xFF200100;
    int s, col;
    int got_break = 0;

    /* Flush audio FIFOs before starting */
    audiop->control = 0xC;
    audiop->control = 0x0;

    for (s = 0; s < NUM_STAVES; s++) {
        if (!seq_is_playing) break;

        for (col = FIRST_COL; col < TOTAL_COLS; col++) {
            if (!seq_is_playing) break;

            /* Poll keyboard between columns for transport controls */
            while (1) {
                int raw = *ps2;

                if (raw & 0x8000) {
                    unsigned char b = (unsigned char)(raw & 0xFF);

                    if (b == 0xE0) continue;
                    if (b == 0xF0) { got_break = 1; continue; }
                    if (got_break) { got_break = 0; continue; }

                    if (b == 0x15) { seq_is_paused = 0; safe_draw_toolbar(cur_note_type); }
                    if (b == 0x24) { seq_is_paused = 1; safe_draw_toolbar(cur_note_type); }
                    if (b == 0x2C) {
                        seq_is_playing = 0; seq_is_paused = 0;
                        seq_user_stopped = 1;
                    }
                    if (b == 0x2D) {
                        seq_is_playing = 0; seq_is_paused = 0;
                        seq_user_restarted = 1;
                    }
                }

                if (!seq_is_paused) break;
            }

            if (!seq_is_playing) break;

            draw_playhead(col, s);
            play_column(audiop, col, s);
            erase_playhead();

            /* Stop early after the last note column */
            if (seq_last_note_page >= 1
                    && cur_page == seq_last_note_page
                    && s == seq_last_note_staff
                    && col >= seq_last_note_col) {
                seq_is_playing = 0;
                break;
            }
        }
    }

    /* Flush audio FIFOs on exit */
    audiop->control = 0xC;
    audiop->control = 0x0;

    seq_is_playing = 0;
}
