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

/* -- Shared colour palette (RGB 5-6-5) -- */
#ifndef RGB565
#define RGB565(r, g, b)  ((short int)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)))
#endif

#define COLOR_SPEARMINT       RGB565( 75, 145, 125)
#define COLOR_NEON_SPEARMINT  RGB565( 87, 200, 160)
#define COLOR_FUCHSIA         RGB565(240,  55, 165)
#define COLOR_CITRIC          RGB565(205, 245, 100)
#define COLOR_MUTED_NEON_BLUE RGB565( 69, 185, 220)
#define COLOR_WHITE           RGB565(255, 255, 255)
#define COLOR_BLACK           RGB565(  0,   0,   0)
#define COLOR_GRAY            RGB565(200, 150, 170)

/* -- Toolbar vertical extent -- 
   In Compact 2-Row mode, each row is 22px tall. Total = 44px. */
#define TOOLBAR_TOP   0
#define TOOLBAR_BOT   43

/* -- Instrument constants -- */
#define TB_INST_BEEP         0 
#define TB_INST_PIANO        1 
#define TB_INST_PIANO_REVERB 2 
#define TB_INST_XYLOPHONE    3

/* -- Playback state -- */
#define TB_STATE_STOPPED  0
#define TB_STATE_PLAYING  1
#define TB_STATE_PAUSED   2

/* -- Note type count -- */
#define TB_NUM_NOTE_TYPES  8

/* -- Menu geometry -- */
#define MENU_X0   70
#define MENU_Y0   60  /* Lowered to clear Row 2 toolbar */
#define MENU_X1  250
#define MENU_Y1  200

/* -- Menu submenu state -- */
#define MENU_STATE_MAIN        0   /* Top-level: 1=Change Instrument, 2=Go Back */
#define MENU_STATE_INSTRUMENT  1   /* Sub-level: 1=BEEP, 2=Piano, 3=Go Back    */

/* -- Global toolbar state -- */
typedef struct {
    int instrument; /* TB_INST_BEEP / _PIANO / _PIANO_REVERB */
    int bpm;        /* beats per minute (default 120) */
    int muted;      /* 0 = sound on, 1 = silent */
    int playback;   /* TB_STATE_STOPPED / _PLAYING / _PAUSED */
} ToolbarState;

extern ToolbarState toolbar_state;

/* Draws Row 1: Transport [Q E T R], Note Selection [1-8], and Tempo [- 120 +] */
void draw_toolbar(int cur_note_type);

/* Draws Row 2: Accidentals [Z X C V], Clear [N], and Options [M] */
void draw_toolbar_row2(int cur_accidental, int active_page_nav, int active_page_struct);

/* Updates only the 8 note-type badges in Row 1 */
void toolbar_set_note_type(int cur_note_type);

/* Updates the transport button highlights to reflect playback state */
void toolbar_set_playback(int state);

/* Updates the dynamic BPM counter on Row 1 */
void toolbar_set_bpm(int bpm);

/* Highlights the currently selected instrument inside the pop-up menu */
void toolbar_set_instrument(int inst);

/* Draws the centered main options pop-up menu (1=Change Instrument, 2=Go Back) */
void draw_options_menu(void);

/* Draws the instrument sub-menu (1=BEEP, 2=Piano, 3=Go Back) */
void draw_options_menu_instrument(void);

/* Draws the page count (e.g., "PAGE 1/1") at the bottom right */
void draw_page_indicator(int cur_page, int max_pages);


/* Helper to draw strings using the skinny font atlas */
void tb_draw_string(int x, int y, const char *str, short int col);

 /* Redraws the bottom-right "M: OPTIONS" tab to ensure it stays visible */
void draw_bottom_tab(void);

#endif /* TOOLBAR_H */