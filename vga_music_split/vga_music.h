#ifndef VGA_MUSIC_H
#define VGA_MUSIC_H

/* ═══════════════════════════════════════════════════════════════════════
   VGA frame-buffer
   512-wide × 256-tall in memory, stride = y<<10 (1024 bytes/row).
   Visible region: 320 × 240.
   ═══════════════════════════════════════════════════════════════════════ */
#define FB_WIDTH        320
#define FB_HEIGHT       240

/* ── Hardware addresses ─────────────────────────────────────────────── */
#define PIXEL_BUF_CTRL  0xFF203020
#define PS2_BASE        0xFF200100
#define PS2_RVALID      0x8000
#define AUDIO_BASE      0xFF203040
#define CHAR_BUF_BASE   0x09000000
#define CHAR_BUF_COLS   80
#define CHAR_BUF_ROWS   60

/* ── Colours RGB565 ─────────────────────────────────────────────────── */
#define WHITE      ((short int)0xFFFF)
#define BLACK      ((short int)0x0000)
#define DARK_GREY  ((short int)0x4A49)
#define GREEN      ((short int)0x07E0)
#define RED        ((short int)0xF800)
#define BLUE       ((short int)0x001F)
#define ORANGE     ((short int)0xFC00)

/* ── Mouse / cursor ─────────────────────────────────────────────────── */
#define ARROW_W     11
#define ARROW_H     16
#define SPEED_DIV   2
#define DOT_R       3
#define MAX_DOTS    256

/* ── Menu bar ───────────────────────────────────────────────────────── */
#define MENUBAR_H    22
#define BTN_Y        4
#define BTN_H        16
#define BTN_W        40
#define BTN_GAP      6
#define BTN_PLAY_X   4
#define BTN_STOP_X   (BTN_PLAY_X   + BTN_W + BTN_GAP)
#define BTN_SPD_UP_X (BTN_STOP_X   + BTN_W + BTN_GAP)
#define BTN_SPD_DN_X (BTN_SPD_UP_X + BTN_W + BTN_GAP)

/*
 * Character-buffer label positions.
 * Pixel-to-char: char_col = pixel_x / 8,  char_row = pixel_y / 8
 * CLABEL_ROW 1  → pixel y = 8..15, centred inside the 16-px-tall button
 * (old value of 2 put text at y=16..23, bleeding 1 px below the menubar)
 */
#define CLABEL_ROW   1
#define CLABEL_PLAY  1
#define CLABEL_STOP  7
#define CLABEL_SPDUP 13
#define CLABEL_SPDN  19

/* ── Staff layout ───────────────────────────────────────────────────── */
#define NUM_STAFFS   4
#define NUM_LINES    5
#define LINE_GAP     6
#define STAFF_LEFT   28          /* leaves room for the treble clef      */
#define STAFF_RIGHT  (FB_WIDTH - 2)

/* Top y-coordinate of each staff's first (topmost) line */
extern const int staff_top[NUM_STAFFS];

/* ── Treble clef geometry ───────────────────────────────────────────── */
#define CLEF_W       18          /* rendered width  (px)                 */
#define CLEF_H       56          /* rendered height (px)                 */
/* clef origin relative to its staff: (STAFF_LEFT - CLEF_W - 2, top-15) */

/* ── Audio ──────────────────────────────────────────────────────────── */
#define AUDIO_CTRL   0
#define AUDIO_FIFO   1
#define AUDIO_LEFT   2
#define AUDIO_RIGHT  3
#define SPEED_LEVELS 5
#define DEFAULT_SPEED 2
#define SAMPLE_RATE  48000

extern const int speed_ticks[SPEED_LEVELS];
extern const int note_freq_hz[];

/* ── Global pixel buffer pointer ────────────────────────────────────── */
extern int pixel_buffer_start;

/* ── Note dots ──────────────────────────────────────────────────────── */
extern int dot_x[MAX_DOTS];
extern int dot_y[MAX_DOTS];
extern int num_dots;

/* ── Playback state ─────────────────────────────────────────────────── */
extern int playing;
extern int play_idx;
extern int speed_level;
extern int play_timer;
extern int note_phase;
extern int note_freq;
extern int note_samples;

#endif /* VGA_MUSIC_H */
