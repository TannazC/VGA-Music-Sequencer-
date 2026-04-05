/**
 * @file toolbar.h
 * @brief Public interface, constants, and shared colour palette for the
 *        sequencer toolbar and options menu.
 *
 * The toolbar occupies the top 44 pixels of the VGA display in two rows:
 *
 *   Row 1 (y 0–21):  Transport [▶ Q] [⏸ E] [■ T] [↺ R] | Note type [1–8] | BPM [- 120 +]
 *   Row 2 (y 22–43): Accidentals [Z X C V] | Page nav [PREV NEXT] | Page struct [+PG -PG] | [CLEAR N]
 *
 * Key assignments (for TA reference):
 *   Q = Play          E = Pause/Resume     T = Stop        R = Restart
 *   1–8 = Note types  Z/X/C/V = Accidentals
 *   - / = = BPM down/up    K / L = Add/remove page
 *   ← / → = Navigate pages     N = Clear all notes
 *
 * Integration in main.c:
 * @code
 *   #include "toolbar.h"
 *   draw_toolbar(cur_note_type);          // after build_and_draw_background()
 *   toolbar_set_note_type(cur_note_type); // on keys 1–8
 *   toolbar_set_bpm(toolbar_state.bpm);   // on - / = keys
 * @endcode
 *
 * @authors Tannaz Chowdhury, Dareen Nasreldin
 */

#ifndef TOOLBAR_H
#define TOOLBAR_H

/* ═══════════════════════════════════════════════════════════════════════
   Shared colour palette (RGB565)
   Used by toolbar.c, background.c, and start_menu.c.
   ═══════════════════════════════════════════════════════════════════════ */

#ifndef RGB565
/** @brief Converts 8-bit RGB components to the RGB565 format used by the VGA controller. */
#define RGB565(r, g, b) ((short int)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)))
#endif

#define COLOR_SPEARMINT       RGB565( 75, 145, 125) /**< Primary theme green.          */
#define COLOR_NEON_SPEARMINT  RGB565( 87, 200, 160) /**< Bright active-state green.    */
#define COLOR_FUCHSIA         RGB565(240,  55, 165) /**< Accent / accidental highlight.*/
#define COLOR_CITRIC          RGB565(205, 245, 100) /**< Yellow-green menu accent.     */
#define COLOR_MUTED_NEON_BLUE RGB565( 69, 185, 220) /**< Stop/clear badge fill.        */
#define COLOR_WHITE           RGB565(255, 228, 238) /**< Off-white for badge text.     */
#define COLOR_BLACK           RGB565(  0,   0,   0) /**< Pure black for borders/text.  */
#define COLOR_GRAY            RGB565(200, 150, 170) /**< Inactive border colour.       */

/* ═══════════════════════════════════════════════════════════════════════
   Toolbar vertical extent
   ═══════════════════════════════════════════════════════════════════════ */

/** @brief Y coordinate of the top of the toolbar (row 1 start). */
#define TOOLBAR_TOP 0

/** @brief Y coordinate of the bottom of the toolbar (row 2 end, inclusive). */
#define TOOLBAR_BOT 43

/* ═══════════════════════════════════════════════════════════════════════
   Instrument constants
   ═══════════════════════════════════════════════════════════════════════ */

#define TB_INST_BEEP         0 /**< Square-wave oscillator (no sample data). */
#define TB_INST_PIANO        1 /**< Piano PCM samples with sustain loop.      */
#define TB_INST_PIANO_REVERB 2 /**< Piano PCM samples with reverb variant.    */
#define TB_INST_XYLOPHONE    3 /**< Xylophone PCM samples, one-shot playback. */

/* ═══════════════════════════════════════════════════════════════════════
   Playback state constants
   ═══════════════════════════════════════════════════════════════════════ */

#define TB_STATE_STOPPED 0 /**< No playback; sequencer is idle.          */
#define TB_STATE_PLAYING 1 /**< Playback is running.                     */
#define TB_STATE_PAUSED  2 /**< Playback is paused between columns.      */

/* ═══════════════════════════════════════════════════════════════════════
   Note type count
   ═══════════════════════════════════════════════════════════════════════ */

/** @brief Number of selectable note types shown in the toolbar (whole through rest). */
#define TB_NUM_NOTE_TYPES 8

/* ═══════════════════════════════════════════════════════════════════════
   Options menu geometry
   ═══════════════════════════════════════════════════════════════════════ */

#define MENU_X0  70  /**< Left edge of the options menu overlay.   */
#define MENU_Y0  60  /**< Top edge (cleared below row 2 toolbar).  */
#define MENU_X1 250  /**< Right edge of the options menu overlay.  */
#define MENU_Y1 200  /**< Bottom edge of the options menu overlay. */

/* ═══════════════════════════════════════════════════════════════════════
   Menu submenu state constants
   ═══════════════════════════════════════════════════════════════════════ */

/** @brief Top-level options menu (1 = Change Instrument, 2 = Main Menu). */
#define MENU_STATE_MAIN       0

/** @brief Instrument submenu (1 = Beep, 2 = Piano, 3 = Xylophone). */
#define MENU_STATE_INSTRUMENT 1

/* ═══════════════════════════════════════════════════════════════════════
   Toolbar state struct
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Holds the live runtime state of the toolbar.
 *
 * Read by sequencer_audio.c during playback to determine BPM and instrument.
 * Written by toolbar_set_bpm(), toolbar_set_instrument(), and the main loop.
 */
typedef struct {
    int instrument; /**< Active instrument (TB_INST_BEEP / _PIANO / _PIANO_REVERB / _XYLOPHONE). */
    int bpm;        /**< Current tempo in beats per minute (range 40–999, default 120).          */
    int muted;      /**< Mute flag: 0 = audio on, 1 = silent.                                   */
    int playback;   /**< Current playback state (TB_STATE_STOPPED / _PLAYING / _PAUSED).         */
} ToolbarState;

/** @brief Global toolbar state instance. Defined in toolbar.c. */
extern ToolbarState toolbar_state;

/* ═══════════════════════════════════════════════════════════════════════
   Public functions
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Renders the full row 1 toolbar.
 *
 * Draws transport badges (Q/E/T/R), note-type selector badges (1–8), and
 * BPM adjustment badges (- value +). Highlights the active transport state
 * and selected note type.
 *
 * @param cur_note_type Index of the currently selected note type (0-based).
 */
void draw_toolbar(int cur_note_type);

/**
 * @brief Renders the full row 2 toolbar.
 *
 * Draws accidental badges (Z/X/C/V), page navigation badges (PREV/NEXT),
 * page structure badges (+PG/-PG), and the clear badge (N).
 *
 * @param cur_accidental     Active accidental index (0=none, 1=sharp, 2=flat, 3=natural).
 * @param active_page_nav    Bitmask: bit 0 = PREV held, bit 1 = NEXT held.
 * @param active_page_struct Bitmask: bit 0 = +PG held, bit 1 = -PG held.
 */
void draw_toolbar_row2(int cur_accidental, int active_page_nav, int active_page_struct);

/**
 * @brief Redraws only the eight note-type badges in row 1.
 *
 * More efficient than a full draw_toolbar() call when only the note
 * selection changes.
 *
 * @param cur_note_type Index of the newly selected note type (0-based).
 */
void toolbar_set_note_type(int cur_note_type);

/**
 * @brief Updates the BPM value in toolbar_state and redraws the BPM badge.
 *
 * Clamps the value to [40, 999] and suppresses leading zeros in the display.
 *
 * @param bpm New tempo in beats per minute.
 */
void toolbar_set_bpm(int bpm);

/**
 * @brief Sets the active instrument and redraws the instrument submenu.
 *
 * @param inst Instrument constant (TB_INST_BEEP, TB_INST_PIANO, etc.).
 */
void toolbar_set_instrument(int inst);

/**
 * @brief Renders the main options menu overlay.
 *
 * Displays two entries: Change Instrument (1) and Main Menu (2).
 */
void draw_options_menu(void);

/**
 * @brief Renders the instrument-select submenu inside the options overlay.
 *
 * Highlights the currently active instrument.
 */
void draw_options_menu_instrument(void);

/**
 * @brief Draws the "PAGE x/y" indicator centred at the bottom of the screen.
 *
 * @param cur_page  Current page number (1-based).
 * @param max_pages Total number of pages.
 */
void draw_page_indicator(int cur_page, int max_pages);

/**
 * @brief Draws a string using the skinny bitmap font atlas.
 *
 * Spaces advance the cursor without drawing a glyph.
 *
 * @param x   Starting X coordinate.
 * @param y   Starting Y coordinate.
 * @param str Null-terminated ASCII string.
 * @param col RGB565 foreground colour.
 */
void tb_draw_string(int x, int y, const char *str, short int col);

/**
 * @brief Draws the "[M] OPTIONS" tab in the bottom-right corner.
 *
 * Should be called after any full-screen redraw to ensure the tab
 * remains visible.
 */
void draw_bottom_tab(void);

#endif /* TOOLBAR_H */
