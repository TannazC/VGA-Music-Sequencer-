/* ═══════════════════════════════════════════════════════════════════════
   toolbar.h  —  Top transport + shortcut toolbar for the music sequencer
   ═══════════════════════════════════════════════════════════════════════

   Visual layout (320 px wide, rows 0-25):

     TRANSPORT:   [▶ Q]  [⏸ E]  [■ T]  [↺ R]
     NOTE TYPE:   [ 1 ]  [ 2 ]  [ 3 ]  [ 4 ]  [ 5 ]     (active = blue)
     ACTIONS:     [+ SPC]  [✕ DEL]
     LEGEND BAR:  W = note  H = half  Q = qtr  8 = 8th  16 = 16th

   Key assignments:
     Q = Play       (start playback from beginning)
     E = Pause/Resume
     T = Stop       (stop + reset position)
     R = Restart    (stop + immediately play again)
     1 = Whole note
     2 = Half note
     3 = Quarter note
     4 = Beamed 8ths (2 heads)
     5 = Beamed 16ths (4 heads)
     Space = Place note at cursor
     Del   = Delete note at cursor
     W/A/S/D = Navigate cursor (not shown — purely directional)

   ── Integration ──────────────────────────────────────────────────────
   In vga_music_v2.c:
     1. #include "toolbar.h"
     2. After build_and_draw_background():   draw_toolbar(cur_note_type);
     3. On keys 1-5:    toolbar_set_note_type(cur_note_type);
     4. On Q pressed:   toolbar_set_playback(TB_STATE_PLAYING);
     5. After playback ends:  toolbar_set_playback(TB_STATE_STOPPED);
   ═══════════════════════════════════════════════════════════════════════ */

#ifndef TOOLBAR_H
#define TOOLBAR_H

/* ── Toolbar vertical extent (rows 0-25, 26 px) ──────────────────────
   Ensure staff_top[0] in background.h is >= TOOLBAR_BOT + 2.          */
#define TOOLBAR_TOP   0
#define TOOLBAR_BOT  25

/* ── Waveform constants (read by osc_tick in sequencer_audio.c) ────── */
#define TB_WAVE_SQUARE    0
#define TB_WAVE_PULSE     1
#define TB_WAVE_TRIANGLE  2

/* ── Playback state (written by play_sequence / main loop) ─────────── */
#define TB_STATE_STOPPED  0
#define TB_STATE_PLAYING  1
#define TB_STATE_PAUSED   2

/* ── Note type count exported (toolbar only shows 5) ───────────────── */
#define TB_NUM_NOTE_TYPES  8

/* ── Global toolbar state ────────────────────────────────────────────
   Defined in toolbar.c; shared via extern with sequencer_audio.c.     */
typedef struct {
    int waveform;   /* TB_WAVE_SQUARE / _PULSE / _TRIANGLE              */
    int bpm;        /* beats per minute (default 120)                   */
    int muted;      /* 0 = sound on, 1 = silent                         */
    int playback;   /* TB_STATE_STOPPED / _PLAYING / _PAUSED            */
} ToolbarState;

extern ToolbarState toolbar_state;

/* ── Public API ─────────────────────────────────────────────────────── */

/* Full first-time draw. Call once after build_and_draw_background().
   cur_note_type : 0 (whole) .. 4 (16th)                               */
void draw_toolbar(int cur_note_type);

/* Repaints only the 5 note-type badges. Call when user presses 1-5.   */
void toolbar_set_note_type(int cur_note_type);

/* Updates the transport button highlights to reflect playback state.   */
void toolbar_set_playback(int state);

/* Called by play_sequence() each column to advance the progress bar.
   step: 0 .. 13  (column index relative to FIRST_COL = 2)            */
void toolbar_update_step(int step);

/* Draws the static [M] OPTIONS tab at the bottom of the screen */
void draw_bottom_tab(void);

/* Draws the static pop-up menu in the center of the screen */
void draw_options_menu(void);

/* Updates the dynamic BPM counter on the toolbar */
void toolbar_set_bpm(int bpm);

/* Helper to draw strings anywhere on the screen */
void tb_draw_string(int x, int y, const char *str, short int col);

#endif /* TOOLBAR_H */