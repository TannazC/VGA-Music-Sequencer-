/* Auto-generated combined C file */

/* =========================================
   Start of treble_clef_bitmap.h
   ========================================= */
#ifndef TREBLE_CLEF_BITMAP_H
#define TREBLE_CLEF_BITMAP_H

/* ═══════════════════════════════════════════════════════════════════════
   Treble clef bitmap  —  12 columns × 36 rows
   ───────────────────────────────────────────────────────────────────────
   Derived from uploaded treble clef image, scaled to fit the staff layout:
     STAFF_SPACING = 6 px, LINES_PER_STAFF = 5
     Glyph spans: 1 space above top line → 1 space below bottom line
                  = 6 + (4×6) + 6 = 36 px total height

   Each entry is one row. Bit 11 = column 0 (leftmost), bit 0 = column 11.
   # = black pixel drawn,  . = transparent (background shows through)
   ═══════════════════════════════════════════════════════════════════════ */

#define CLEF_BMP_W  12
#define CLEF_BMP_H  36

static const unsigned short treble_clef_bmp[CLEF_BMP_H] = {
    /* row  0 */  0x038,  /* ......###... */
    /* row  1 */  0x078,  /* .....####... */
    /* row  2 */  0x06C,  /* .....##.##.. */
    /* row  3 */  0x044,  /* .....#...#.. */
    /* row  4 */  0x044,  /* .....#...#.. */
    /* row  5 */  0x0C4,  /* ....##...#.. */
    /* row  6 */  0x0CC,  /* ....##..##.. */
    /* row  7 */  0x08C,  /* ....#...##.. */
    /* row  8 */  0x09C,  /* ....#..###.. */
    /* row  9 */  0x058,  /* .....#.##... */
    /* row 10 */  0x070,  /* .....###.... */
    /* row 11 */  0x070,  /* .....###.... */
    /* row 12 */  0x0E0,  /* ....###..... */
    /* row 13 */  0x1E0,  /* ...####..... */
    /* row 14 */  0x3C0,  /* ..####...... */
    /* row 15 */  0x3E0,  /* ..#####..... */
    /* row 16 */  0x720,  /* .###..#..... */
    /* row 17 */  0xE38,  /* ###...###... */
    /* row 18 */  0xE7C,  /* ###..#####.. */
    /* row 19 */  0xCFE,  /* ##..#######. */
    /* row 20 */  0xCF7,  /* ##..####.### */
    /* row 21 */  0xC93,  /* ##..#..#..## */
    /* row 22 */  0xC93,  /* ##..#..#..## */
    /* row 23 */  0x493,  /* .#..#..#..## */
    /* row 24 */  0x413,  /* .#.....#..## */
    /* row 25 */  0x202,  /* ..#.......#. */
    /* row 26 */  0x18C,  /* ...##...##.. */
    /* row 27 */  0x078,  /* .....####... */
    /* row 28 */  0x008,  /* ........#... */
    /* row 29 */  0x000,  /* .........#.. */
    /* row 30 */  0x0C4,  /* ....##...#.. */
    /* row 31 */  0x1E4,  /* ...####..#.. */
    /* row 32 */  0x1E4,  /* ...####..#.. */
    /* row 33 */  0x1E4,  /* ...####..#.. */
    /* row 34 */  0x1C8,  /* ...###..#... */
    /* row 35 */  0x0F0,  /* ....####.... */
};

#endif /* TREBLE_CLEF_BITMAP_H */
/* =========================================
   End of treble_clef_bitmap.h
   ========================================= */

/* =========================================
   Start of background.h
   ========================================= */
#ifndef BACKGROUND_H
#define BACKGROUND_H

/* ── Frame-buffer dimensions ── */
#define FB_WIDTH    320
#define FB_HEIGHT   240

/* ============== Staff layout ================
   4 lines per staff, 12 px between lines  so each staff is 36 px tall.
   Two staves fit comfortably in 240 px with room between them.          */
#define NUM_STAVES      4
#define LINES_PER_STAFF 5
#define STAFF_SPACING   6
#define STAFF_X0        20
#define STAFF_X1       (FB_WIDTH - 10)

// snap_to_step parameters
#define NUM_STEPS   16
#define STEP_W      ((STAFF_X1 - STAFF_X0) / NUM_STEPS)
/* Shared across background.c and vga_music_v2.c */
extern const int staff_top[NUM_STAVES];
extern short int bg[FB_HEIGHT][FB_WIDTH]; //precomputed background array, read by restore_pixel in vga_music_v2.c

/* Build bg[][] procedurally and blit to frame buffer. Call once at
   startup after pixel_buffer_start is set.                              */
void build_and_draw_background(void);

#endif
/* =========================================
   End of background.h
   ========================================= */

/* =========================================
   Start of background.c
   ========================================= */
// Skipped local include by merge script: #include "background.h"
// Skipped local include by merge script: #include "treble_clef_bitmap.h"

/* ═══════════════════════════════════════════════════════════════════════
   Colours (RGB565 format)
   ═══════════════════════════════════════════════════════════════════════ */
#define WHITE  ((short int)0xFFFF)
#define BLACK  ((short int)0x0000)

/* ═══════════════════════════════════════════════════════════════════════
   Background buffer

   Stores one colour per visible pixel (320 x 240).
   This acts as a "ground truth" copy of the screen so we can restore
   pixels correctly when the cursor moves.

   Important:
   We NEVER read back from VGA memory directly — we always use bg[][]
   ═══════════════════════════════════════════════════════════════════════ */
short int bg[FB_HEIGHT][FB_WIDTH];

/* Provided by main VGA file — this is the base address of the frame buffer */
extern int pixel_buffer_start;

/* PS/2 hardware (used only for FIFO flush at the end) */
#define PS2_BASE    0xFF200100
#define PS2_RVALID  0x8000

const int staff_top[NUM_STAVES] = { 60, 100, 140, 180 };

/* ═══════════════════════════════════════════════════════════════════════
   bg_plot

   Writes a pixel BOTH:
     1. into bg[][] (software copy)
     2. into VGA memory (hardware display)

   This is critical because:
     - bg[][] is used later to restore pixels (cursor erase)
     - VGA memory is what actually shows on screen

   Parameters:
     x -> x-coordinate
     y -> y-coordinate
     c -> colour (RGB565)

   Input:
     pixel position + colour

   Output:
     none

   Side effects:
     updates both software buffer AND hardware frame buffer

   WARNING:
     Bounds checking is mandatory — writing out of bounds corrupts
     memory and can break unrelated hardware (like PS/2 mouse).
   ═══════════════════════════════════════════════════════════════════════ */
static void bg_plot(int x, int y, short int c)
{
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;

    bg[y][x] = c;

    *(volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = c;
}

/* ═══════════════════════════════════════════════════════════════════════
   draw_staves

   Draws the 5 horizontal lines for each musical staff.

   Structure:
     - NUM_STAVES staffs total
     - each staff has LINES_PER_STAFF lines
     - vertical spacing between lines = STAFF_SPACING

   Parameters:
     none

   Input:
     constants (staff positions, spacing)

   Output:
     none

   Side effect:
     draws horizontal black lines into bg[][] and VGA
   ═══════════════════════════════════════════════════════════════════════ */
static void draw_staves(void)
{
    int s, l, x;

    for (s = 0; s < NUM_STAVES; s++) {
        for (l = 0; l < LINES_PER_STAFF; l++) {

            int y = staff_top[s] + l * STAFF_SPACING;

            for (x = STAFF_X0; x < STAFF_X1; x++)
                bg_plot(x, y, BLACK);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   draw_barlines

   Draws vertical boundary lines at the left and right edges of each staff.

   These define the start and end of each measure visually.

   Parameters:
     none

   Input:
     staff positions

   Output:
     none

   Side effect:
     draws vertical black lines into bg[][] and VGA
   ═══════════════════════════════════════════════════════════════════════ */
static void draw_barlines(void)
{
    int s, y;

    for (s = 0; s < NUM_STAVES; s++) {

        int y0 = staff_top[s];
        int y1 = staff_top[s] + (LINES_PER_STAFF - 1) * STAFF_SPACING;

        for (y = y0; y <= y1; y++) {
            bg_plot(STAFF_X0,     y, BLACK);
            bg_plot(STAFF_X1 - 1, y, BLACK);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   draw_treble_clef

   Draws a treble clef using a bitmap (bitmask per row).

   How it works:
     - each row is a 16-bit value (treble_clef_bmp[row])
     - each bit represents whether a pixel should be drawn
     - we scan across bits and draw where bit = 1

   Parameters:
     x0 -> left position where bitmap starts
     y0 -> top position where bitmap starts

   Input:
     bitmap + placement coordinates

   Output:
     none

   Side effect:
     draws the clef into bg[][] and VGA

   Note:
     bg_plot handles bounds checking, so no need here
   ═══════════════════════════════════════════════════════════════════════ */
static void draw_treble_clef(int x0, int y0)
{
    int row, col;

    for (row = 0; row < CLEF_BMP_H; row++) {

        unsigned short bits = treble_clef_bmp[row];

        for (col = 0; col < CLEF_BMP_W; col++) {

            /* Check if this bit is set (pixel should be drawn) */
            if (bits & (1 << (CLEF_BMP_W - 1 - col)))
                bg_plot(x0 + col, y0 + row, BLACK);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   build_and_draw_background

   Builds the entire background once at startup.

   Steps:
     1. clear full VGA memory (including hidden region)
     2. initialise bg[][] to match (all white)
     3. draw musical staves
     4. draw barlines
     5. draw treble clefs
     6. flush PS/2 FIFO to avoid corrupted mouse packets

   Parameters:
     none

   Input:
     none

   Output:
     none

   Side effects:
     fully initializes the screen and bg[][] buffer
   ═══════════════════════════════════════════════════════════════════════ */
void build_and_draw_background(void)
{
    int x, y, s;

    /* ── Step 1: clear full frame buffer (512 x 256, not just visible area) ──
       This ensures no garbage remains in off-screen memory */
    for (y = 0; y < 256; y++) {

        volatile short int *row =
            (volatile short int *)(pixel_buffer_start + (y << 10));

        for (x = 0; x < 512; x++)
            row[x] = WHITE;
    }

    /* ── Step 2: initialise bg[][] to match screen (all white) ── */
    for (y = 0; y < FB_HEIGHT; y++)
        for (x = 0; x < FB_WIDTH; x++)
            bg[y][x] = WHITE;

    /* ── Step 3–4: draw staff structure ── */
    draw_staves();
    draw_barlines();

    /* ── Step 5: draw treble clefs ──
       Positioned slightly above the top staff line so that the
       spiral aligns with the correct pitch reference */
    for (s = 0; s < NUM_STAVES; s++)
        draw_treble_clef(STAFF_X0 + 1,
                         staff_top[s] - STAFF_SPACING);

    /* ── Step 6: flush PS/2 FIFO ─────────────────────────────────────────
       Why this matters:

       Drawing the background takes a noticeable amount of time.
       During this time, the mouse continues sending movement bytes.

       If we do NOT flush:
         - leftover bytes stay in the FIFO
         - packet alignment breaks
         - main loop misinterprets data
         - mouse appears "frozen" or glitchy

       Fix:
         drain everything before entering main loop
       ─────────────────────────────────────────────────────────────────── */
    {
        volatile int *ps2 = (volatile int *)PS2_BASE;

        while (*ps2 & PS2_RVALID)
            (void)(*ps2);
    }
}

/* =========================================
   End of background.c
   ========================================= */

/* =========================================
   Start of sequencer_audio.h
   ========================================= */
/*
 * sequencer_audio.h
 * ─────────────────────────────────────────────────────────────────────
 * Include this in vga_music_v2.c to wire up playback.
 *
 * In vga_music_v2.c, add at the top:
 *     #include "sequencer_audio.h"
 *
 * In the main keyboard loop, add:
 *     if (b == KEY_Q) play_sequence();
 *
 * That is all. The audio file handles everything else.
 * ─────────────────────────────────────────────────────────────────────
 */

#ifndef SEQUENCER_AUDIO_H
#define SEQUENCER_AUDIO_H

#define KEY_Q  0x15   /* PS/2 Set-2 scancode for Q */

/* Trigger full playback: staff 0 bar → staff 1 → staff 2 → staff 3.
   Green playhead animates column by column on the active staff only.
   Audio plays each column's notes as square waves at the correct pitch.
   Returns when the full sequence has finished. */
void play_sequence(void);

#endif /* SEQUENCER_AUDIO_H */

/* =========================================
   End of sequencer_audio.h
   ========================================= */

/* =========================================
   Start of sequencer_audio.c
   ========================================= */
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
// Skipped local include by merge script: #include "background.h"
// Skipped local include by merge script: #include "sequencer_audio.h"

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

/* =========================================
   End of sequencer_audio.c
   ========================================= */

/* =========================================
   Start of sprites.h
   ========================================= */
#ifndef SPRITES_H
#define SPRITES_H

/* ── Filled oval: tilted ~20 deg CCW (quarter / eighth / 16th heads) ── */
#define OVAL_W 11
#define OVAL_H 7

static const unsigned char FILLED_OVAL[7][11] = {
    {0,0,0,0,0,1,1,1,0,0,0},
    {0,0,0,1,1,1,1,1,1,1,0},
    {0,0,1,1,1,1,1,1,1,1,0},
    {0,0,1,1,1,1,1,1,1,1,0},
    {0,0,1,1,1,1,1,1,1,1,0},
    {0,0,0,1,1,1,1,1,1,0,0},
    {0,0,0,0,1,1,1,0,0,0,0},
};

/* ── Open oval: upright (whole / half heads) ── */
#define OPEN_OVAL_W 13
#define OPEN_OVAL_H 7

static const unsigned char OPEN_OVAL[7][13] = {
    {0,0,0,0,1,1,1,1,1,0,0,0,0},
    {0,0,0,1,1,1,1,1,1,1,0,0,0},
    {0,0,1,1,1,0,0,0,1,1,1,0,0},
    {0,1,1,1,0,0,0,0,0,1,1,1,0},
    {0,0,1,1,1,0,0,0,1,1,1,0,0},
    {0,0,0,1,1,1,1,1,1,1,0,0,0},
    {0,0,0,0,1,1,1,1,1,0,0,0,0},
};

#endif /* SPRITES_H */
/* =========================================
   End of sprites.h
   ========================================= */

/* =========================================
   Start of vga_music_v2.c
   ========================================= */
#include <stdlib.h>
// Skipped local include by merge script: #include "background.h"
// Skipped local include by merge script: #include "sequencer_audio.h"
// Skipped local include by merge script: #include "sprites.h"

int pixel_buffer_start;

/* ═══════════════════════════════════════════════════════════════════════
   Hardware addresses
   ═══════════════════════════════════════════════════════════════════════ */
#define PIXEL_BUF_CTRL  0xFF203020
#define PS2_BASE        0xFF200100
#define PS2_RVALID      0x8000

/* ═══════════════════════════════════════════════════════════════════════
   PS/2 Set-2 scan codes
   W/A/S/D  = navigate
   1-7      = select note type  (1=whole .. 7=64th)
   Space    = place note
   Backspace= delete note
   ═══════════════════════════════════════════════════════════════════════ */
#define KEY_W      0x1D
#define KEY_A      0x1C
#define KEY_S      0x1B
#define KEY_D      0x23

#define KEY_1      0x16   /* Whole note       (1 head)  */
#define KEY_2      0x1E   /* Half note        (1 head)  */
#define KEY_3      0x26   /* Quarter note     (1 head)  */
#define KEY_4      0x25   /* 2 beamed eighths (2 heads) */
#define KEY_5      0x2E   /* 4 beamed 16ths   (4 heads) */
#define KEY_6      0x36   /* 2 beamed 16ths   (2 heads) */
#define KEY_7      0x3D   /* Single 16th      (1 head)  */

#define KEY_SPACE  0x29
#define KEY_DELETE 0x66
#define KEY_Q      0x15   /* Q - start playback */
#define KEY_BREAK  0xF0

/* ═══════════════════════════════════════════════════════════════════════
   Note types  (left-to-right matches the reference image)
   ─────────────────────────────────────────────────────────────────────
   Duration in 1/64-note units.
   Beamed groups store each sub-beat in head_step[] / head_pitch_slot[]
   so the playback engine can iterate each individual beat precisely.
   ═══════════════════════════════════════════════════════════════════════ */
#define NOTE_WHOLE      0   /* open oval, no stem            – 64/64  1 head  */
#define NOTE_HALF       1   /* open oval + stem              – 32/64  1 head  */
#define NOTE_QUARTER    2   /* filled oval + stem            – 16/64  1 head  */
#define NOTE_BEAM2_8TH  3   /* 2 beamed eighths (beam group) – 16/64  2 heads */
#define NOTE_BEAM4_16TH 4   /* 4 beamed 16ths   (beam group) – 16/64  4 heads */
#define NOTE_BEAM2_16TH 5   /* 2 beamed 16ths   (beam group) –  8/64  2 heads */
#define NOTE_SINGLE16TH 6   /* single 16th flag              –  4/64  1 head  */
#define NUM_NOTE_TYPES  7

/* Total duration of the whole glyph in 1/64-note units */
static const int note_duration_64[NUM_NOTE_TYPES] = {
    64, 32, 16,   /* whole, half, quarter           */
    16,           /* 2x eighth  = 16/64             */
    16,           /* 4x 16th    = 16/64             */
     8,           /* 2x 16th    =  8/64             */
     4            /* 1x 16th    =  4/64             */
};

/* How many individual note-heads each glyph contains */
static const int note_num_heads[NUM_NOTE_TYPES] = {
    1, 1, 1, 2, 4, 2, 1
};

/* ═══════════════════════════════════════════════════════════════════════
   Grid layout
   ═══════════════════════════════════════════════════════════════════════ */
#define SLOTS_PER_STAFF   ((LINES_PER_STAFF - 1) * 2 + 1)   /* 9  */
#define TOTAL_ROWS        (NUM_STAVES * SLOTS_PER_STAFF)     /* 36 */
#define TOTAL_COLS        NUM_STEPS                          /* 16 */

/* ═══════════════════════════════════════════════════════════════════════
   Visual constants
   ═══════════════════════════════════════════════════════════════════════ */
#define CURSOR_COLOR  ((short int)0x051F)
#define WHITE         ((short int)0xFFFF)
#define BLACK         ((short int)0x0000)


#define STEM_X_OFF    (OVAL_W)/2    /* stem at right edge of oval */
#define STEM_HEIGHT   11

/* Beamed notes: heads are spaced exactly STEP_W apart (one grid column each) */
/* So head[i] is at col+i, screen_x + i*STEP_W                               */

/* Beam bar thickness */
#define BEAM_THICK    2

/* Single flag: diagonal from stem tip */
#define FLAG_LEN      5

/* Cursor cell */
#define CELL_W        (STEP_W - 1)
#define CELL_H        (STAFF_SPACING / 2)

/* Max glyph bounding box for erase (4 heads wide + stem + flag) */
#define GLYPH_ERASE_W  (3 * STEP_W + OVAL_W/2 + FLAG_LEN + 2)
#define GLYPH_ERASE_H  (STEM_HEIGHT + OVAL_H/2 + 4)

#define MAX_NOTES      256
#define MAX_HEADS      4    /* maximum heads in one glyph */

/* ═══════════════════════════════════════════════════════════════════════
   Note struct
   ─────────────────────────────────────────────────────────────────────
   anchor: the leftmost/only head — used as the glyph's reference point.
   heads[]:  each individual beat position.
     • For single notes: num_heads=1, heads[0] = anchor.
     • For beamed groups: num_heads=N, heads[i] is at step+i, same
       pitch_slot (same horizontal row, consecutive columns).
   The playback engine reads heads[i].step / heads[i].pitch_slot for
   every sub-beat tick.
   ═══════════════════════════════════════════════════════════════════════ */
#ifndef NOTE_STRUCT_DEFINED
#define NOTE_STRUCT_DEFINED
   typedef struct {
    int step;         /* anchor column (first/only head)   */
    int staff;        /* 0 .. NUM_STAVES-1                 */
    int pitch_slot;   /* 0 (top) .. 8 (bottom) in staff    */
    int note_type;    /* NOTE_WHOLE .. NOTE_SINGLE16TH      */
    int duration_64;  /* total glyph duration in 1/64 units */

    /* Sub-beat positions (playback use) */
    int num_heads;
    int head_step[MAX_HEADS];        /* column index of each head           */
    int head_pitch_slot[MAX_HEADS];  /* pitch slot of each head (same row)  */

    /* Screen coordinates */
    int screen_x;    /* pixel x of anchor head centre */
    int screen_y;    /* pixel y of anchor head centre */
    int head_x[MAX_HEADS];   /* pixel x of each head centre */
    int head_y[MAX_HEADS];   /* pixel y of each head centre */
} Note;
#endif /* NOTE_STRUCT_DEFINED */   /* prevents redefinition in sequencer_audio.c */
Note notes[MAX_NOTES];
int  num_notes = 0;

int cur_note_type = NOTE_QUARTER;

/* ═══════════════════════════════════════════════════════════════════════
   Grid helpers
   ═══════════════════════════════════════════════════════════════════════ */
static int col_to_x(int col)
{
    if (col < 0)           col = 0;
    if (col >= TOTAL_COLS) col = TOTAL_COLS - 1;
    return STAFF_X0 + col * STEP_W + STEP_W / 2;
}

static int row_to_y(int row, int *staff_out, int *slot_out)
{
    int s, slot;
    if (row < 0)           row = 0;
    if (row >= TOTAL_ROWS) row = TOTAL_ROWS - 1;
    s    = row / SLOTS_PER_STAFF;
    slot = row % SLOTS_PER_STAFF;
    if (staff_out) *staff_out = s;
    if (slot_out)  *slot_out  = slot;
    return staff_top[s] + slot * (STAFF_SPACING / 2);
}

/* ═══════════════════════════════════════════════════════════════════════
   Pixel helpers
   ═══════════════════════════════════════════════════════════════════════ */
void plot_pixel(int x, int y, short int c)
{
    if (x < 0 || x >= FB_WIDTH)  return;
    if (y < 0 || y >= FB_HEIGHT) return;
    *(volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = c;
}

/*
   restore_pixel: if pixel (x,y) falls inside ANY head of ANY placed note,
   paint it black; otherwise restore the background.
   We check every head of every note so beamed multi-head glyphs are
   protected correctly when the cursor moves over them.
*/
void restore_pixel(int x, int y)
{
    int i, h;
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;

    for (i = 0; i < num_notes; i++) {
        for (h = 0; h < notes[i].num_heads; h++) {
            int ddx = x - notes[i].head_x[h];
            int ddy = y - notes[i].head_y[h];
            int nt  = notes[i].note_type;
            if (nt == NOTE_WHOLE || nt == NOTE_HALF) {
                int bx = ddx + OPEN_OVAL_W/2;
                int by = ddy + OPEN_OVAL_H/2;
                if (bx >= 0 && bx < OPEN_OVAL_W && by >= 0 && by < OPEN_OVAL_H)
                    if (OPEN_OVAL[by][bx]) { plot_pixel(x, y, BLACK); return; }
            } else {
                int bx = ddx + OVAL_W/2;
                int by = ddy + OVAL_H/2;
                if (bx >= 0 && bx < OVAL_W && by >= 0 && by < OVAL_H)
                    if (FILLED_OVAL[by][bx]) { plot_pixel(x, y, BLACK); return; }
            }
        }
    }
    plot_pixel(x, y, bg[y][x]);
}

/* ═══════════════════════════════════════════════════════════════════════
   Cursor cell
   ═══════════════════════════════════════════════════════════════════════ */
static void draw_cursor_cell(int cx, int cy)
{
    int x, y;
    int x0 = cx - CELL_W/2, x1 = cx + CELL_W/2;
    int y0 = cy - CELL_H/2, y1 = cy + CELL_H/2;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            plot_pixel(x, y, CURSOR_COLOR);
}

static void erase_cursor_cell(int cx, int cy)
{
    int x, y;
    int x0 = cx - CELL_W/2, x1 = cx + CELL_W/2;
    int y0 = cy - CELL_H/2, y1 = cy + CELL_H/2;
    /* Restore every pixel in the cell directly from bg[][] – no oval test.
       Then redraw_all_notes() is called by the caller after moving so any
       stem / beam / flag that passed through here gets repainted.          */
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT)
                plot_pixel(x, y, bg[y][x]);
        }
}

/* ═══════════════════════════════════════════════════════════════════════
   Note glyph primitives
   ═══════════════════════════════════════════════════════════════════════ */

static void filled_oval(int ax, int ay, short int c)
{
    int dx, dy;
    for (dy = 0; dy < OVAL_H; dy++)
        for (dx = 0; dx < OVAL_W; dx++)
            if (FILLED_OVAL[dy][dx])
                plot_pixel(ax + dx - OVAL_W/2, ay + dy - OVAL_H/2, c);
}

static void open_oval(int ax, int ay, short int c)
{
    int dx, dy;
    for (dy = 0; dy < OPEN_OVAL_H; dy++)
        for (dx = 0; dx < OPEN_OVAL_W; dx++)
            if (OPEN_OVAL[dy][dx])
                plot_pixel(ax + dx - OPEN_OVAL_W/2, ay + dy - OPEN_OVAL_H/2, c);
}

/* Stem at right edge of oval, going straight up */
static void stem_up(int ax, int ay, short int c)
{
    int y;
    for (y = ay - STEM_HEIGHT; y <= ay; y++)
        plot_pixel(ax + STEM_X_OFF, y, c);
}

/*
   Single flag: diagonal stroke from stem tip curving right-downward.
   Two beams-wide stroke for visibility.
*/
//COmmented out since not used and flagged in cpulator
/*static void single_flag(int ax, int ay, short int c)
{
    int k;
    int sx = ax + STEM_X_OFF;
    int sy = ay - STEM_HEIGHT;
    for (k = 0; k < FLAG_LEN; k++) {
        plot_pixel(sx + k,     sy + k,     c);
        plot_pixel(sx + k + 1, sy + k,     c);
        plot_pixel(sx + k,     sy + k + 1, c);
    }
}
*/
/*
   Double flag: two stacked diagonals (for 16th single note).
*/
static void double_flag(int ax, int ay, short int c)
{
    int k;
    int sx = ax + STEM_X_OFF;
    /* First flag at stem tip */
    for (k = 0; k < FLAG_LEN; k++) {
        plot_pixel(sx + k,     ay - STEM_HEIGHT + k,     c);
        plot_pixel(sx + k + 1, ay - STEM_HEIGHT + k,     c);
        plot_pixel(sx + k,     ay - STEM_HEIGHT + k + 1, c);
    }
    /* Second flag 3px below first */
    for (k = 0; k < FLAG_LEN; k++) {
        plot_pixel(sx + k,     ay - STEM_HEIGHT + 3 + k,     c);
        plot_pixel(sx + k + 1, ay - STEM_HEIGHT + 3 + k,     c);
        plot_pixel(sx + k,     ay - STEM_HEIGHT + 3 + k + 1, c);
    }
}

/*
   Horizontal beam bar.
   x0 = left stem x,  x1 = right stem x,  y_top = top of beam,
   thick = bar thickness in pixels.
*/
static void beam_bar(int x0, int x1, int y_top, int thick, short int c)
{
    int x, t;
    for (t = 0; t < thick; t++)
        for (x = x0; x <= x1; x++)
            plot_pixel(x, y_top + t, c);
}

/* ═══════════════════════════════════════════════════════════════════════
   draw_note_glyph
   Draws the complete visual glyph for note type `nt` anchored at (cx,cy).
   For beamed types, additional heads are at cx + i*STEP_W (i=1,2,3).
   ═══════════════════════════════════════════════════════════════════════ */
static void draw_note_glyph(int cx, int cy, int nt, short int c)
{
    int i;
    /* x of each stem (right edge of each head) */
    int stem_top_y = cy - STEM_HEIGHT;

    switch (nt) {

    /* ── Whole: open oval, no stem ── */
    case NOTE_WHOLE:
        open_oval(cx, cy, c);
        break;

    /* ── Half: open oval + stem ── */
    case NOTE_HALF:
        open_oval(cx, cy, c);
        stem_up(cx, cy, c);
        break;

    /* ── Quarter: filled oval + stem ── */
    case NOTE_QUARTER:
        filled_oval(cx, cy, c);
        stem_up(cx, cy, c);
        break;

    /* ── 2 beamed eighths: 2 heads + 2 stems + 1 beam bar ── */
    case NOTE_BEAM2_8TH:
        for (i = 0; i < 2; i++) {
            filled_oval(cx + i * STEP_W, cy, c);
            stem_up(cx + i * STEP_W, cy, c);
        }
        /* beam across stem tops */
        beam_bar(cx + STEM_X_OFF, cx + STEP_W + STEM_X_OFF,
                 stem_top_y, BEAM_THICK, c);
        break;

    /* ── 4 beamed 16ths: 4 heads + 4 stems + 2 beam bars ── */
    case NOTE_BEAM4_16TH:
        for (i = 0; i < 4; i++) {
            filled_oval(cx + i * STEP_W, cy, c);
            stem_up(cx + i * STEP_W, cy, c);
        }
        /* two beam bars, second one 3px below first */
        beam_bar(cx + STEM_X_OFF, cx + 3 * STEP_W + STEM_X_OFF,
                 stem_top_y, BEAM_THICK, c);
        beam_bar(cx + STEM_X_OFF, cx + 3 * STEP_W + STEM_X_OFF,
                 stem_top_y + BEAM_THICK + 1, BEAM_THICK, c);
        break;

    /* ── 2 beamed 16ths: 2 heads + 2 stems + 2 beam bars ── */
    case NOTE_BEAM2_16TH:
        for (i = 0; i < 2; i++) {
            filled_oval(cx + i * STEP_W, cy, c);
            stem_up(cx + i * STEP_W, cy, c);
        }
        beam_bar(cx + STEM_X_OFF, cx + STEP_W + STEM_X_OFF,
                 stem_top_y, BEAM_THICK, c);
        beam_bar(cx + STEM_X_OFF, cx + STEP_W + STEM_X_OFF,
                 stem_top_y + BEAM_THICK + 1, BEAM_THICK, c);
        break;

    /* ── Single 16th: filled oval + stem + 2 flags ── */
    case NOTE_SINGLE16TH:
        filled_oval(cx, cy, c);
        stem_up(cx, cy, c);
        double_flag(cx, cy, c);
        break;

    default: break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   Erase: restore the full bounding box (covers all types conservatively)
   ═══════════════════════════════════════════════════════════════════════ */
static void erase_note_glyph(int cx, int cy)
{
    int x, y;
    int x0 = cx - OVAL_W/2;
    int x1 = cx + GLYPH_ERASE_W;
    int y0 = cy - GLYPH_ERASE_H;
    int y1 = cy + OVAL_H/2 + 2;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT)
                plot_pixel(x, y, bg[y][x]);
}

static void redraw_all_notes(void)
{
    int i;
    for (i = 0; i < num_notes; i++)
        draw_note_glyph(notes[i].screen_x, notes[i].screen_y,
                        notes[i].note_type, BLACK);
}

/* ═══════════════════════════════════════════════════════════════════════
   fill_note_heads
   Populate the head_step[], head_pitch_slot[], head_x[], head_y[] arrays
   for a note of type `nt` placed at (col, staff, slot, screen_x, screen_y).
   Beamed notes occupy consecutive columns at the same pitch row.
   ═══════════════════════════════════════════════════════════════════════ */
static void fill_note_heads(Note *n, int col, int staff, int slot,
                             int sx, int sy, int nt)
{
    int i;
    int nh = note_num_heads[nt];
    n->num_heads = nh;
    for (i = 0; i < nh; i++) {
        n->head_step[i]       = col + i;           /* consecutive columns   */
        n->head_pitch_slot[i] = slot;              /* same pitch row        */
        n->head_x[i]          = sx + i * STEP_W;  /* screen x of head i    */
        n->head_y[i]          = sy;                /* same y for all heads  */
    }
    /* Pad unused slots */
    for (i = nh; i < MAX_HEADS; i++) {
        n->head_step[i] = n->head_pitch_slot[i] = 0;
        n->head_x[i] = n->head_y[i] = 0;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   col_is_occupied
   Returns 1 if column `col` on (staff, slot) is already claimed by any
   head of any existing note.  Used to block overlapping placements.
   ═══════════════════════════════════════════════════════════════════════ */
static int col_is_occupied(int col, int staff)
{
    int i, h;
    for (i = 0; i < num_notes; i++) {
        if (notes[i].staff != staff) continue;
        for (h = 0; h < notes[i].num_heads; h++) {
            if (notes[i].head_step[h] == col) return 1;
        }
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
   place_note
   Blocks placement if:
     (a) the cursor column is already occupied by any head of any note, OR
     (b) any of the new note's heads would land on an occupied column.
   ═══════════════════════════════════════════════════════════════════════ */
static void place_note(int cur_col, int cur_staff, int cur_slot,
                       int cur_x, int cur_y, int nt)
{
    int h;
    int nh = note_num_heads[nt];

    /* All heads must fit within the grid columns 0..TOTAL_COLS-1 */
    if (cur_col < 0 || cur_col + nh - 1 >= TOTAL_COLS) return;

    /* Check every column the new glyph would occupy */
    for (h = 0; h < nh; h++) {
        if (col_is_occupied(cur_col + h, cur_staff)) return;
    }

    if (num_notes >= MAX_NOTES) return;

    notes[num_notes].step        = cur_col;
    notes[num_notes].staff       = cur_staff;
    notes[num_notes].pitch_slot  = cur_slot;
    notes[num_notes].note_type   = nt;
    notes[num_notes].duration_64 = note_duration_64[nt];
    notes[num_notes].screen_x    = cur_x;
    notes[num_notes].screen_y    = cur_y;
    fill_note_heads(&notes[num_notes], cur_col, cur_staff, cur_slot,
                    cur_x, cur_y, nt);
    num_notes++;

    draw_note_glyph(cur_x, cur_y, nt, BLACK);
    draw_cursor_cell(cur_x, cur_y);
}

/* ═══════════════════════════════════════════════════════════════════════
   delete_note
   Deletes whatever note owns the cursor column on this staff/slot.
   Checks all heads so beamed notes can be deleted from any of their
   occupied columns, not just the anchor.
   ═══════════════════════════════════════════════════════════════════════ */
static void delete_note(int cur_col, int cur_staff, int cur_slot,
                        int cur_x, int cur_y)
{
    int i, h;
    for (i = 0; i < num_notes; i++) {
        if (notes[i].staff != cur_staff || notes[i].pitch_slot != cur_slot)
            continue;
        for (h = 0; h < notes[i].num_heads; h++) {
            if (notes[i].head_step[h] == cur_col) {
                erase_note_glyph(notes[i].screen_x, notes[i].screen_y);
                notes[i] = notes[num_notes - 1];
                num_notes--;
                redraw_all_notes();
                draw_cursor_cell(cur_x, cur_y);
                return;
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   PS/2 helpers
   ═══════════════════════════════════════════════════════════════════════ */
static int ps2_read_byte(volatile int *ps2)
{
    int v = *ps2;
    if (v & PS2_RVALID) return v & 0xFF;
    return -1;
}

static void ps2_flush(volatile int *ps2)
{
    int i;
    for (i = 0; i < 512; i++) {
        if (!(*ps2 & PS2_RVALID)) break;
        (void)(*ps2);
    }
}

static void keyboard_init(void)
{
    volatile int *ps2 = (volatile int *)PS2_BASE;
    int timeout;
    ps2_flush(ps2);
    *ps2 = 0xFF;
    timeout = 3000000;
    while (timeout-- > 0) {
        int v = *ps2;
        if ((v & PS2_RVALID) && (v & 0xFF) == 0xAA) break;
    }
    ps2_flush(ps2);
    *ps2 = 0xF4;
    timeout = 2000000;
    while (timeout-- > 0) {
        int v = *ps2;
        if ((v & PS2_RVALID) && (v & 0xFF) == 0xFA) break;
    }
    ps2_flush(ps2);
}

/* Forward declaration for play_sequence defined in sequencer_audio.c */
void play_sequence(void);

/* ═══════════════════════════════════════════════════════════════════════
   Main
   ═══════════════════════════════════════════════════════════════════════ */
int main(void)
{
    volatile int *pixel_ctrl = (volatile int *)PIXEL_BUF_CTRL;
    volatile int *ps2        = (volatile int *)PS2_BASE;

    pixel_buffer_start = *pixel_ctrl;
    *(pixel_ctrl + 1)  = pixel_buffer_start;

    keyboard_init();
    build_and_draw_background();

    int cur_col   = 0;
    int cur_row   = 0;
    int cur_staff = 0;
    int cur_slot  = 0;
    int cur_x     = col_to_x(cur_col);
    int cur_y     = row_to_y(cur_row, &cur_staff, &cur_slot);

    draw_cursor_cell(cur_x, cur_y);
    pixel_buffer_start = *pixel_ctrl;

    int got_break = 0;

    while (1)
    {
        int raw = ps2_read_byte(ps2);
        if (raw < 0) continue;

        unsigned char b = (unsigned char)raw;

        if (b == 0xE0)        continue;
        if (b == KEY_BREAK) { got_break = 1; continue; }
        if (got_break)      { got_break = 0; continue; }

        /* ── 1-7: select note type ── */
        if (b == KEY_1) { cur_note_type = NOTE_WHOLE;      continue; }
        if (b == KEY_2) { cur_note_type = NOTE_HALF;       continue; }
        if (b == KEY_3) { cur_note_type = NOTE_QUARTER;    continue; }
        if (b == KEY_4) { cur_note_type = NOTE_BEAM2_8TH;  continue; }
        if (b == KEY_5) { cur_note_type = NOTE_BEAM4_16TH; continue; }
        if (b == KEY_6) { cur_note_type = NOTE_BEAM2_16TH; continue; }
        if (b == KEY_7) { cur_note_type = NOTE_SINGLE16TH; continue; }

        /* ── Q: play sequence ── */
        if (b == KEY_Q) {
            play_sequence();
            /* Redraw everything cleanly after playback returns */
            redraw_all_notes();
            draw_cursor_cell(cur_x, cur_y);
        }

        /* ── Space: place ── */
        if (b == KEY_SPACE)
            place_note(cur_col, cur_staff, cur_slot, cur_x, cur_y, cur_note_type);

        /* ── Backspace: delete ── */
        if (b == KEY_DELETE)
            delete_note(cur_col, cur_staff, cur_slot, cur_x, cur_y);

        /* ── W/A/S/D: navigate ── */
        if (b == KEY_W || b == KEY_A || b == KEY_S || b == KEY_D) {
            int new_col = cur_col;
            int new_row = cur_row;

            if (b == KEY_W && cur_row > 0)              new_row--;
            if (b == KEY_S && cur_row < TOTAL_ROWS - 1) new_row++;
            if (b == KEY_A && cur_col > 0)              new_col--;
            if (b == KEY_D && cur_col < TOTAL_COLS - 1) new_col++;

            if (new_col != cur_col || new_row != cur_row) {
                erase_cursor_cell(cur_x, cur_y);
                cur_col = new_col;
                cur_row = new_row;
                cur_x   = col_to_x(cur_col);
                cur_y   = row_to_y(cur_row, &cur_staff, &cur_slot);
                /* Repaint all notes so stems/beams under old cursor survive */
                redraw_all_notes();
                draw_cursor_cell(cur_x, cur_y);
            }
        }

        pixel_buffer_start = *pixel_ctrl;
    }

    return 0;
}

/* =========================================
   End of vga_music_v2.c
   ========================================= */

