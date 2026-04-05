/**
 * @file toolbar.c
 * @brief Top toolbar and options menu rendering for the music sequencer.
 *
 * Implements the two-row toolbar at the top of the VGA display. Row 1 holds
 * transport controls (play/pause/stop/restart), note-type selectors, and BPM
 * adjustment badges. Row 2 holds accidental selectors, page navigation, page
 * structure controls, and the clear action. Also renders the options menu
 * overlay, instrument selector, page indicator, and bottom tab.
 *
 * @authors Tannaz Chowdhury, Dareen Nasreldin
 */

#include "toolbar.h"
#include "background.h"
#include "skinny_font.h"

/** @brief plot_pixel is defined in main.c. */
extern void plot_pixel(int x, int y, short int c);

/* ═══════════════════════════════════════════════════════════════════════
   Global toolbar state
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Live state of the toolbar: instrument, BPM, mute flag, and
 *        playback status. Read by sequencer_audio.c during playback.
 */
ToolbarState toolbar_state = {
    TB_INST_BEEP,    /**< Default instrument. */
    120,             /**< Default BPM.        */
    0,               /**< Not muted.          */
    TB_STATE_STOPPED /**< Initially stopped.  */
};

/* ═══════════════════════════════════════════════════════════════════════
   Layout geometry
   ═══════════════════════════════════════════════════════════════════════ */
#define ROW1_Y         0   /**< Y origin of toolbar row 1.                  */
#define BADGE_AREA_H  22   /**< Total pixel height of the row 1 area.       */
#define ROW2_Y        22   /**< Y origin of toolbar row 2.                  */
#define BADGE_H       18   /**< Height of individual badge boxes.           */
#define FONT_ADVANCE   6   /**< Horizontal advance per character (pixels).  */

/* Badge widths */
#define BADGE_W_TRANS   31  /**< Width of a transport (play/pause/stop) badge. */
#define BADGE_W1        12  /**< Width of a single note-type badge.            */
#define BADGE_W_ACC     18  /**< Width of an accidental badge.                 */
#define BADGE_W_TMP_BTN 10  /**< Width of the BPM +/- button badges.          */
#define BADGE_W_TMP_VAL 26  /**< Width of the BPM value display badge.        */
#define BADGE_GAP        2  /**< Gap in pixels between adjacent badges.       */
#define GROUP_SEP        8  /**< Wider gap used between badge groups.         */

/* ═══════════════════════════════════════════════════════════════════════
   Colour palette (RGB565) — toolbar-local colours
   ═══════════════════════════════════════════════════════════════════════ */
#define TB_BG          RGB565(220, 170, 190) /**< Toolbar background fill.  */
#define TB_BORDER      COLOR_BLACK           /**< Badge and menu borders.   */
#define TB_DIV         COLOR_BLACK           /**< Divider line colour.      */
#define DARK_PINK      RGB565(180, 80, 110)  /**< Accent for page indicator.*/

/* Transport badge colours */
#define TB_PLAY_FILL   COLOR_SPEARMINT
#define TB_PLAY_FILL_A COLOR_NEON_SPEARMINT  /**< Play badge when active.  */
#define TB_PLAY_ICO    COLOR_WHITE
#define TB_PLAY_KEY    COLOR_WHITE

#define TB_PAUSE_FILL   COLOR_FUCHSIA
#define TB_PAUSE_FILL_A COLOR_CITRIC         /**< Pause badge when active. */
#define TB_PAUSE_ICO    COLOR_WHITE
#define TB_PAUSE_KEY    COLOR_WHITE

#define TB_STOP_FILL   COLOR_MUTED_NEON_BLUE
#define TB_STOP_FILL_A COLOR_WHITE
#define TB_STOP_ICO    COLOR_WHITE
#define TB_STOP_KEY    COLOR_WHITE

#define TB_REST_FILL   COLOR_CITRIC
#define TB_REST_FILL_A COLOR_WHITE
#define TB_REST_ICO    COLOR_BLACK
#define TB_REST_KEY    COLOR_BLACK

/* Note-type badge colours */
#define TB_NOTE_FILL  COLOR_WHITE           /**< Inactive note badge fill. */
#define TB_NOTE_TXT   COLOR_BLACK
#define TB_NOTEA_FILL COLOR_SPEARMINT       /**< Active note badge fill.   */
#define TB_NOTEA_TXT  COLOR_WHITE

/* ═══════════════════════════════════════════════════════════════════════
   Font rendering
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Renders one character from a packed bitmap font atlas.
 *
 * The atlas is a flat array of (num_chars × h × stride) bytes. Each byte
 * holds 8 pixels, MSB first. Only pixels within the declared glyph width w
 * are drawn; the remaining bits in the last byte of each row are ignored.
 *
 * @param x      Left edge of the glyph.
 * @param y      Top edge of the glyph.
 * @param idx    Glyph index into the atlas.
 * @param w      Glyph width in pixels.
 * @param h      Glyph height in pixels.
 * @param stride Bytes per row in the atlas (ceil(w/8)).
 * @param ptr    Pointer to the flat atlas byte array.
 * @param col    RGB565 foreground colour.
 */
static void draw_atlas_char(int x, int y, int idx, int w, int h,
                            int stride, const unsigned char *ptr, short int col)
{
    for (int r = 0; r < h; r++) {
        for (int b = 0; b < stride; b++) {
            int offset = (idx * h * stride) + (r * stride) + b;
            unsigned char bits = ptr[offset];
            for (int bit = 0; bit < 8; bit++) {
                if ((b * 8 + bit) < w && (bits & (0x80 >> bit)))
                    plot_pixel(x + (b * 8) + bit, y + r, col);
            }
        }
    }
}

/**
 * @brief Renders one character from the skinny font atlas at (x, y).
 *
 * @param x   Left edge of the character.
 * @param y   Top edge of the character.
 * @param c   ASCII character to draw.
 * @param col RGB565 foreground colour.
 */
static void tb_draw_char(int x, int y, unsigned char c, short int col)
{
    int idx = get_skinny_font_index(c);
    const unsigned char *flat_ptr = (const unsigned char *)skinny_font_atlas;
    draw_atlas_char(x, y, idx, SKINNY_FONT_WIDTH, SKINNY_FONT_HEIGHT,
                    SKINNY_FONT_STRIDE, flat_ptr, col);
}

/* ═══════════════════════════════════════════════════════════════════════
   Drawing primitives
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Fills a rectangular region with a solid colour (inclusive bounds).
 *
 * @param x0 Left edge.
 * @param y0 Top edge.
 * @param x1 Right edge (inclusive).
 * @param y1 Bottom edge (inclusive).
 * @param c  RGB565 fill colour.
 */
static void tb_fill(int x0, int y0, int x1, int y1, short int c)
{
    int x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            plot_pixel(x, y, c);
}

/**
 * @brief Draws a horizontal line from x0 to x1 (inclusive) at height y.
 *
 * @param x0 Start X.
 * @param x1 End X (inclusive).
 * @param y  Y coordinate.
 * @param c  RGB565 colour.
 */
static void tb_hline(int x0, int x1, int y, short int c)
{
    int x;
    for (x = x0; x <= x1; x++) plot_pixel(x, y, c);
}

/**
 * @brief Draws a vertical line from y0 to y1 (inclusive) at column x.
 *
 * @param x  X coordinate.
 * @param y0 Start Y.
 * @param y1 End Y (inclusive).
 * @param c  RGB565 colour.
 */
static void tb_vline(int x, int y0, int y1, short int c)
{
    int y;
    for (y = y0; y <= y1; y++) plot_pixel(x, y, c);
}

/**
 * @brief Draws a short vertical divider line between badge groups in row 1.
 *
 * @param x X coordinate of the divider.
 * @param y Top Y of row 1 (ROW1_Y).
 */
static void tb_group_div(int x, int y)
{
    tb_vline(x, y + 2, y + BADGE_AREA_H - 3, COLOR_BLACK);
}

/* ═══════════════════════════════════════════════════════════════════════
   Badge rendering
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Draws a rectangular badge with a text label at an arbitrary row.
 *
 * The badge is centred vertically within BADGE_AREA_H and drawn with a
 * 1-pixel black border. The label is centred horizontally within the badge.
 *
 * @param x     Left edge of the badge.
 * @param y     Top of the row area (ROW1_Y or ROW2_Y).
 * @param bw    Badge width in pixels.
 * @param label Null-terminated label string.
 * @param fill  RGB565 background colour.
 * @param txt   RGB565 text colour.
 * @return X coordinate immediately after the right edge of the badge.
 */
static int tb_badge_at(int x, int y, int bw, const char *label,
                       short int fill, short int txt)
{
    int llen = 0;
    for (const char *p = label; *p; p++) llen++;

    int text_w = llen * FONT_ADVANCE;
    int tx = x + (bw - text_w) / 2 + 1;

    int b_y0 = y + (BADGE_AREA_H - BADGE_H) / 2;
    int b_y1 = b_y0 + BADGE_H - 1;
    int f_y0 = b_y0 + (BADGE_H - SKINNY_FONT_HEIGHT) / 2;

    tb_fill(x + 1, b_y0 + 1, x + bw - 2, b_y1 - 1, fill);
    tb_hline(x, x + bw - 1, b_y0, COLOR_BLACK);
    tb_hline(x, x + bw - 1, b_y1, COLOR_BLACK);
    tb_vline(x, b_y0, b_y1, COLOR_BLACK);
    tb_vline(x + bw - 1, b_y0, b_y1, COLOR_BLACK);

    for (const char *p = label; *p; p++, tx += FONT_ADVANCE)
        tb_draw_char(tx, f_y0, (unsigned char)*p, txt);

    return x + bw;
}

/**
 * @brief Draws a badge in row 1 (ROW1_Y). Thin wrapper around tb_badge_at().
 *
 * @param x    Left edge.
 * @param bw   Badge width.
 * @param label Label string.
 * @param fill Fill colour.
 * @param txt  Text colour.
 * @return X coordinate after the badge.
 */
static int tb_badge(int x, int bw, const char *label, short int fill, short int txt)
{
    return tb_badge_at(x, ROW1_Y, bw, label, fill, txt);
}

/* ═══════════════════════════════════════════════════════════════════════
   Transport icon bitmaps  (12×12, 1 = draw, 0 = skip)
   ═══════════════════════════════════════════════════════════════════════ */

/** @brief Right-pointing triangle icon for the Play button. */
static const unsigned char ICON_PLAY[12][12] = {
    {0,0,1,1,0,0,0,0,0,0,0,0},{0,0,1,1,1,1,0,0,0,0,0,0},{0,0,1,1,1,1,1,1,0,0,0,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},{0,0,1,1,1,1,1,1,1,1,1,1},{0,0,1,1,1,1,1,1,1,1,1,1},
    {0,0,1,1,1,1,1,1,1,1,0,0},{0,0,1,1,1,1,1,1,0,0,0,0},{0,0,1,1,1,1,0,0,0,0,0,0},
    {0,0,1,1,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0}
};

/** @brief Two vertical bars icon for the Pause button. */
static const unsigned char ICON_PAUSE[12][12] = {
    {0,0,1,1,1,0,0,1,1,1,0,0},{0,0,1,1,1,0,0,1,1,1,0,0},{0,0,1,1,1,0,0,1,1,1,0,0},
    {0,0,1,1,1,0,0,1,1,1,0,0},{0,0,1,1,1,0,0,1,1,1,0,0},{0,0,1,1,1,0,0,1,1,1,0,0},
    {0,0,1,1,1,0,0,1,1,1,0,0},{0,0,1,1,1,0,0,1,1,1,0,0},{0,0,1,1,1,0,0,1,1,1,0,0},
    {0,0,1,1,1,0,0,1,1,1,0,0},{0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0}
};

/** @brief Solid square icon for the Stop button. */
static const unsigned char ICON_STOP[12][12] = {
    {0,0,1,1,1,1,1,1,1,1,0,0},{0,0,1,1,1,1,1,1,1,1,0,0},{0,0,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},{0,0,1,1,1,1,1,1,1,1,0,0},{0,0,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},{0,0,1,1,1,1,1,1,1,1,0,0},{0,0,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},{0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0}
};

/** @brief Circular arrow icon for the Restart button. */
static const unsigned char ICON_RESTART[12][12] = {
    {0,0,0,0,1,1,1,1,1,0,0,0},{0,0,1,1,0,0,0,0,1,1,1,0},{0,1,1,0,0,0,0,1,1,1,1,0},
    {0,1,1,0,0,0,0,0,1,1,1,0},{1,1,0,0,0,0,0,0,0,1,1,0},{1,1,0,0,0,0,0,0,0,0,1,0},
    {1,1,0,0,0,0,0,0,0,0,0,0},{0,1,0,0,0,0,0,0,0,1,1,0},{0,1,1,0,0,0,0,0,1,1,0,0},
    {0,0,1,1,0,0,0,1,1,0,0,0},{0,0,0,1,1,1,1,1,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0}
};

/**
 * @brief Renders a 12×12 icon bitmap at the given position.
 *
 * @param x    Left edge.
 * @param y    Top edge.
 * @param icon 12×12 array; 1 = plot pixel, 0 = skip.
 * @param col  RGB565 colour for set pixels.
 */
static void tb_draw_icon_12(int x, int y,
                             const unsigned char icon[12][12], short int col)
{
    int r, c;
    for (r = 0; r < 12; r++)
        for (c = 0; c < 12; c++)
            if (icon[r][c] == 1) plot_pixel(x + c, y + r, col);
}

/**
 * @brief Draws a transport control badge (icon on the left, key label on
 *        the right, separated by a vertical divider).
 *
 * @param x        Left edge of the badge.
 * @param icon     12×12 icon bitmap to draw on the left side.
 * @param key_char Single character shown on the right side (keyboard shortcut).
 * @param fill     RGB565 background fill colour.
 * @param icon_col RGB565 colour for the icon pixels.
 * @param key_col  RGB565 colour for the key character.
 * @return X coordinate immediately after the right edge of the badge.
 */
static int tb_transport_badge(int x, const unsigned char icon[12][12],
                               char key_char, short int fill,
                               short int icon_col, short int key_col)
{
    int bw     = BADGE_W_TRANS;
    int div_x  = x + 19;
    int icon_x = x + (18 - 12) / 2;
    int b_y0   = (BADGE_AREA_H - BADGE_H) / 2;
    int icon_y = b_y0 + (BADGE_H - 12) / 2;
    int key_x  = div_x + ((x + bw - div_x) - SKINNY_FONT_WIDTH) / 2 + 1;
    int f_y0   = b_y0 + (BADGE_H - SKINNY_FONT_HEIGHT) / 2;

    tb_fill(x + 1, b_y0 + 1, x + bw - 2, b_y0 + BADGE_H - 2, fill);
    tb_hline(x, x + bw - 1, b_y0, COLOR_BLACK);
    tb_hline(x, x + bw - 1, b_y0 + BADGE_H - 1, COLOR_BLACK);
    tb_vline(x, b_y0, b_y0 + BADGE_H - 1, COLOR_BLACK);
    tb_vline(x + bw - 1, b_y0, b_y0 + BADGE_H - 1, COLOR_BLACK);

    tb_draw_icon_12(icon_x, icon_y, icon, icon_col);
    tb_vline(div_x, b_y0 + 2, b_y0 + BADGE_H - 3, COLOR_BLACK);
    tb_draw_char(key_x, f_y0, (unsigned char)key_char, key_col);

    return x + bw;
}

/* ── Cached X positions for dynamic redraws ── */

/** @brief X origin of the first note-type badge (set during draw_toolbar). */
static int g_note_badge_x0 = 0;

/** @brief X origin of the first transport badge (set during draw_toolbar). */
static int g_trans_x0 = 0;

/** @brief X origin of the BPM value badge (set during draw_toolbar). */
static int g_bpm_badge_x0 = 0;

/**
 * @brief Returns the left X coordinate of note-type badge i.
 *
 * @param i Note type index (0 = whole, up to TB_NUM_NOTE_TYPES-1).
 * @return Left edge pixel of that badge.
 */
static int note_badge_x(int i)
{
    return g_note_badge_x0 + i * (BADGE_W1 + BADGE_GAP);
}

/* ═══════════════════════════════════════════════════════════════════════
   Public rendering functions
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Renders the full row 1 toolbar: transport controls, note-type
 *        selector, and BPM badges.
 *
 * Highlights the active transport state (playing/paused) and the currently
 * selected note type. Caches g_note_badge_x0, g_trans_x0, and
 * g_bpm_badge_x0 for later partial redraws by toolbar_set_bpm() and
 * toolbar_set_note_type().
 *
 * @param cur_note_type Index of the currently selected note type (0-based).
 */
void draw_toolbar(int cur_note_type)
{
    int x, i;
    int is_playing = (toolbar_state.playback == TB_STATE_PLAYING);
    int is_paused  = (toolbar_state.playback == TB_STATE_PAUSED);

    tb_fill(0, ROW1_Y, FB_WIDTH - 1, ROW1_Y + BADGE_AREA_H - 1, TB_BG);
    tb_hline(0, FB_WIDTH - 1, ROW1_Y + BADGE_AREA_H - 1, TB_DIV);

    x = 4; g_trans_x0 = x;
    x = tb_transport_badge(x, ICON_PLAY,    'Q', is_playing ? TB_PLAY_FILL_A  : TB_PLAY_FILL,  TB_PLAY_ICO,  TB_PLAY_KEY)  + BADGE_GAP;
    x = tb_transport_badge(x, ICON_PAUSE,   'E', is_paused  ? TB_PAUSE_FILL_A : TB_PAUSE_FILL, TB_PAUSE_ICO, TB_PAUSE_KEY) + BADGE_GAP;
    x = tb_transport_badge(x, ICON_STOP,    'T', TB_STOP_FILL,  TB_STOP_ICO,  TB_STOP_KEY)  + BADGE_GAP;
    x = tb_transport_badge(x, ICON_RESTART, 'R', TB_REST_FILL,  TB_REST_ICO,  TB_REST_KEY);

    x += GROUP_SEP / 2; tb_group_div(x, ROW1_Y); x += GROUP_SEP / 2;
    g_note_badge_x0 = x;
    for (i = 0; i < TB_NUM_NOTE_TYPES; i++) {
        char label[2] = {(char)('1' + i), '\0'};
        short int fill = (i == cur_note_type) ? TB_NOTEA_FILL : TB_NOTE_FILL;
        short int txt  = (i == cur_note_type) ? TB_NOTEA_TXT  : TB_NOTE_TXT;
        x = tb_badge(x, BADGE_W1, label, fill, txt) + BADGE_GAP;
    }
    x -= BADGE_GAP;

    x += GROUP_SEP / 2; tb_group_div(x, ROW1_Y); x += GROUP_SEP / 2;
    x = tb_badge(x, BADGE_W_TMP_BTN, "-", COLOR_FUCHSIA, COLOR_WHITE) + BADGE_GAP;

    g_bpm_badge_x0 = x;
    toolbar_set_bpm(toolbar_state.bpm);
    x += BADGE_W_TMP_VAL + BADGE_GAP;
    tb_badge(x, BADGE_W_TMP_BTN, "+", COLOR_SPEARMINT, COLOR_WHITE);
}

/**
 * @brief Renders the full row 2 toolbar: accidentals, page navigation,
 *        page structure controls, and the clear badge.
 *
 * Highlights the active accidental and reflects pressed states for page
 * navigation and structure buttons using the bitmask parameters.
 *
 * @param cur_accidental   Active accidental index (0=none, 1=sharp, 2=flat, 3=natural).
 * @param active_page_nav  Bitmask: bit 0 = PREV pressed, bit 1 = NEXT pressed.
 * @param active_page_struct Bitmask: bit 0 = +PG pressed, bit 1 = -PG pressed.
 */
void draw_toolbar_row2(int cur_accidental, int active_page_nav, int active_page_struct)
{
    int x = 4;

    tb_fill(0, ROW2_Y, FB_WIDTH - 1, ROW2_Y + BADGE_AREA_H + 1, TB_BG);
    tb_hline(0, FB_WIDTH - 1, ROW2_Y + BADGE_AREA_H + 1, TB_DIV);

    /* Accidentals: Z X C V */
    const char *acc_labels[] = {"Z", "X", "C", "V"};
    for (int i = 0; i < 4; i++) {
        short int fill = (i == cur_accidental) ? COLOR_FUCHSIA : COLOR_WHITE;
        short int txt  = (i == cur_accidental) ? COLOR_WHITE   : COLOR_BLACK;
        x = tb_badge_at(x, ROW2_Y, 14, acc_labels[i], fill, txt) + BADGE_GAP;
    }

    x += GROUP_SEP / 2; tb_group_div(x, ROW2_Y); x += GROUP_SEP / 2;

    /* Page navigation: PREV (<) and NEXT (>) */
    const char *pg_nav_labels[] = {"PREV", "NEXT"};
    const char *pg_nav_keys[]   = {"<",    ">"};
    for (int i = 0; i < 2; i++) {
        int is_pressed = (active_page_nav & (1 << i));
        int label_w = 28, key_w = 12, badge_w = label_w + key_w;
        int split_x = x + label_w;

        tb_fill(x, ROW2_Y + 3, split_x, ROW2_Y + BADGE_H,
                is_pressed ? COLOR_MUTED_NEON_BLUE : RGB565(180, 200, 210));
        tb_fill(split_x, ROW2_Y + 3, x + badge_w, ROW2_Y + BADGE_H, COLOR_WHITE);

        tb_hline(x, x + badge_w, ROW2_Y + 3, COLOR_BLACK);
        tb_hline(x, x + badge_w, ROW2_Y + BADGE_H, COLOR_BLACK);
        tb_vline(x, ROW2_Y + 3, ROW2_Y + BADGE_H, COLOR_BLACK);
        tb_vline(x + badge_w, ROW2_Y + 3, ROW2_Y + BADGE_H, COLOR_BLACK);
        tb_vline(split_x, ROW2_Y + 3, ROW2_Y + BADGE_H, COLOR_BLACK);

        tb_draw_string(x + 3, ROW2_Y + 6, pg_nav_labels[i],
                       is_pressed ? COLOR_WHITE : COLOR_BLACK);
        tb_draw_string(split_x + 4, ROW2_Y + 6, pg_nav_keys[i], COLOR_BLACK);

        x += badge_w + BADGE_GAP;
    }

    x += GROUP_SEP / 2; tb_group_div(x, ROW2_Y); x += GROUP_SEP / 2;

    /* Page structure: +PG (K) and -PG (L) */
    const char *struct_labels[] = {"+PG", "-PG"};
    const char *struct_keys[]   = {"K", "L"};
    for (int i = 0; i < 2; i++) {
        int is_pressed = (active_page_struct & (1 << i));
        int label_w = 24, key_w = 12, badge_w = label_w + key_w;
        int split_x = x + label_w;

        tb_fill(x, ROW2_Y + 3, split_x, ROW2_Y + BADGE_H,
                is_pressed ? COLOR_NEON_SPEARMINT : COLOR_SPEARMINT);
        tb_fill(split_x, ROW2_Y + 3, x + badge_w, ROW2_Y + BADGE_H, COLOR_WHITE);

        tb_hline(x, x + badge_w, ROW2_Y + 3, COLOR_BLACK);
        tb_hline(x, x + badge_w, ROW2_Y + BADGE_H, COLOR_BLACK);
        tb_vline(x, ROW2_Y + 3, ROW2_Y + BADGE_H, COLOR_BLACK);
        tb_vline(x + badge_w, ROW2_Y + 3, ROW2_Y + BADGE_H, COLOR_BLACK);
        tb_vline(split_x, ROW2_Y + 3, ROW2_Y + BADGE_H, COLOR_BLACK);

        tb_draw_string(x + 3, ROW2_Y + 6, struct_labels[i], COLOR_WHITE);
        tb_draw_string(split_x + 4, ROW2_Y + 6, struct_keys[i], COLOR_BLACK);

        x += badge_w + BADGE_GAP;
    }

    x += GROUP_SEP / 2; tb_group_div(x, ROW2_Y); x += GROUP_SEP / 2;

    /* Clear badge */
    int label_w = 34, key_w = 12, clear_w = label_w + key_w;
    int split_x = x + label_w;

    tb_fill(x, ROW2_Y + 3, split_x, ROW2_Y + BADGE_H, COLOR_MUTED_NEON_BLUE);
    tb_fill(split_x, ROW2_Y + 3, x + clear_w, ROW2_Y + BADGE_H, COLOR_WHITE);

    tb_hline(x, x + clear_w, ROW2_Y + 3, COLOR_BLACK);
    tb_hline(x, x + clear_w, ROW2_Y + BADGE_H, COLOR_BLACK);
    tb_vline(x, ROW2_Y + 3, ROW2_Y + BADGE_H, COLOR_BLACK);
    tb_vline(x + clear_w, ROW2_Y + 3, ROW2_Y + BADGE_H, COLOR_BLACK);
    tb_vline(split_x, ROW2_Y + 3, ROW2_Y + BADGE_H, COLOR_BLACK);

    tb_draw_string(x + 3, ROW2_Y + 6, "CLEAR", COLOR_WHITE);
    tb_draw_string(split_x + 4, ROW2_Y + 6, "N", COLOR_BLACK);
}

/**
 * @brief Renders a null-terminated string using the skinny font.
 *
 * Spaces advance the cursor without drawing. Each character advances by
 * FONT_ADVANCE pixels.
 *
 * @param x   Starting X coordinate.
 * @param y   Starting Y coordinate.
 * @param str Null-terminated string to draw.
 * @param col RGB565 colour.
 */
void tb_draw_string(int x, int y, const char *str, short int col)
{
    const char *p;
    int tx = x;
    for (p = str; *p; p++, tx += FONT_ADVANCE) {
        if (*p == ' ') continue;
        tb_draw_char(tx, y, (unsigned char)*p, col);
    }
}

/**
 * @brief Draws the "PAGE x/y" indicator centred at the bottom of the screen.
 *
 * @param cur_page  Current page number (1-based).
 * @param max_pages Total number of pages.
 */
void draw_page_indicator(int cur_page, int max_pages)
{
    int y = FB_HEIGHT - 16;
    char page_str[9];
    page_str[0] = 'P'; page_str[1] = 'A'; page_str[2] = 'G'; page_str[3] = 'E';
    page_str[4] = ' ';
    page_str[5] = '0' + (char)cur_page;
    page_str[6] = '/';
    page_str[7] = '0' + (char)max_pages;
    page_str[8] = '\0';

    int page_w = 8 * FONT_ADVANCE;
    int cx = (FB_WIDTH - page_w) / 2;
    tb_draw_string(cx, y, page_str, DARK_PINK);
}

/**
 * @brief Draws the "[M] OPTIONS" tab in the bottom-right corner of the screen.
 *
 * The tab serves as a visual reminder that pressing M opens the options menu.
 */
void draw_bottom_tab(void)
{
    int w = 78, h = 13;
    int x = FB_WIDTH - w - 8;
    int y = FB_HEIGHT - h - 6;

    tb_fill(x, y, x + w, y + h, TB_BG);
    tb_hline(x, x + w, y,     COLOR_BLACK);
    tb_hline(x, x + w, y + h, COLOR_BLACK);
    tb_vline(x,     y, y + h, COLOR_BLACK);
    tb_vline(x + w, y, y + h, COLOR_BLACK);

    tb_draw_string(x + 6, y + 3, "[M] OPTIONS", COLOR_BLACK);
}

/**
 * @brief Draws one row inside the options menu overlay.
 *
 * When selected, the key badge is filled; otherwise it is outlined only.
 *
 * @param y        Y position of the row.
 * @param label    Action label string drawn to the right of the badge.
 * @param key      Single-character key shortcut shown in the badge.
 * @param fill     Fill colour used when the row is selected.
 * @param txt      Text colour used when the row is selected.
 * @param selected Non-zero to render in the selected/highlighted style.
 */
static void menu_draw_row(int y, const char *label, const char *key,
                           short int fill, short int txt, int selected)
{
    int bx = MENU_X0 + 15;
    if (selected) {
        tb_fill(bx, y - 2, bx + 12, y + 10, fill);
    } else {
        tb_hline(bx, bx + 12, y - 2, COLOR_BLACK);
        tb_hline(bx, bx + 12, y + 10, COLOR_BLACK);
        tb_vline(bx,      y - 2, y + 10, COLOR_BLACK);
        tb_vline(bx + 12, y - 2, y + 10, COLOR_BLACK);
    }
    tb_draw_char(bx + 4, y + 1, key[0], selected ? txt : COLOR_BLACK);
    tb_draw_string(bx + 20, y + 1, label, selected ? fill : COLOR_BLACK);
}

/**
 * @brief Renders the main options menu overlay with two entries:
 *        Change Instrument and Main Menu.
 */
void draw_options_menu(void)
{
    // Right edge shadow
    tb_fill(MENU_X1, MENU_Y0 + 4, MENU_X1 + 4, MENU_Y1 + 4, TB_BORDER);
    // Bottom edge shadow
    tb_fill(MENU_X0 + 4, MENU_Y1, MENU_X1 - 1, MENU_Y1 + 4, TB_BORDER);
    
    // NEW: Menu Outline (Draws a solid black base)
    tb_fill(MENU_X0, MENU_Y0, MENU_X1, MENU_Y1, COLOR_BLACK);
    
    // Inner menu background (Draws over the black base, leaving a 2px border)
    tb_fill(MENU_X0 + 2, MENU_Y0 + 2, MENU_X1 - 2, MENU_Y1 - 2, TB_BG);

    tb_draw_string(MENU_X0 + 45, MENU_Y0 + 10, "OPTIONS MENU", DARK_PINK);
    
    tb_hline(MENU_X0 + 10, MENU_X1 - 10, MENU_Y0 + 24, COLOR_BLACK);

    menu_draw_row(MENU_Y0 + 45, "CHANGE INSTRUMENT", "1", TB_PLAY_FILL, COLOR_WHITE, 0);
    menu_draw_row(MENU_Y0 + 75, "MAIN MENU",         "2", TB_STOP_FILL, COLOR_WHITE, 0);

    tb_draw_string(MENU_X0 + 32, MENU_Y1 - 20, "PRESS M TO CLOSE", DARK_PINK);
}

/**
 * @brief Renders the instrument-select submenu inside the options overlay.
 *
 * Highlights the currently active instrument in fuchsia; others are shown
 * in black.
 */
void draw_options_menu_instrument(void)
{
    tb_fill(MENU_X0, MENU_Y0, MENU_X1, MENU_Y1, COLOR_BLACK);
    tb_fill(MENU_X0 + 2, MENU_Y0 + 2, MENU_X1 - 2, MENU_Y1 - 2, TB_BG);
    tb_draw_string(MENU_X0 + 45, MENU_Y0 + 10, "SELECT INSTRUMENT", COLOR_CITRIC);
    tb_hline(MENU_X0 + 10, MENU_X1 - 10, MENU_Y0 + 24, COLOR_BLACK);

    int inst = toolbar_state.instrument;
    short int c1 = (inst == TB_INST_BEEP)      ? COLOR_FUCHSIA : COLOR_BLACK;
    short int c2 = (inst == TB_INST_PIANO)     ? COLOR_FUCHSIA : COLOR_BLACK;
    short int c3 = (inst == TB_INST_XYLOPHONE) ? COLOR_FUCHSIA : COLOR_BLACK;

    tb_draw_string(MENU_X0 + 15, MENU_Y0 + 45, "[1] BEEP",      c1);
    tb_draw_string(MENU_X0 + 15, MENU_Y0 + 65, "[2] PIANO",     c2);
    tb_draw_string(MENU_X0 + 15, MENU_Y0 + 85, "[3] XYLOPHONE", c3);
    tb_draw_string(MENU_X0 + 32, MENU_Y1 - 18, "PRESS M TO CLOSE", DARK_PINK);
}

/**
 * @brief Sets the active instrument and immediately redraws the instrument
 *        submenu to reflect the change.
 *
 * @param inst Instrument constant (TB_INST_BEEP, TB_INST_PIANO,
 *             TB_INST_XYLOPHONE, or TB_INST_PIANO_REVERB).
 */
void toolbar_set_instrument(int inst)
{
    toolbar_state.instrument = inst;
    draw_options_menu_instrument();
}

/**
 * @brief Updates the stored BPM value and redraws only the BPM value badge.
 *
 * Clamps bpm to [40, 999]. Leading zeros are suppressed (e.g. 090 → "90").
 * Only the value badge is repainted; the rest of the toolbar is untouched.
 *
 * @param bpm New BPM value.
 */
void toolbar_set_bpm(int bpm)
{
    char str[4];
    if (bpm > 999) bpm = 999;
    if (bpm <  40) bpm = 40;
    toolbar_state.bpm = bpm;
    str[0] = '0' + (bpm / 100);
    str[1] = '0' + ((bpm / 10) % 10);
    str[2] = '0' + (bpm % 10);
    str[3] = '\0';
    char *display_str = (str[0] == '0') ? &str[1] : str;
    tb_badge_at(g_bpm_badge_x0, ROW1_Y, BADGE_W_TMP_VAL,
                display_str, COLOR_WHITE, COLOR_BLACK);
}

/**
 * @brief Redraws all note-type badges to reflect the newly selected type.
 *
 * Only the eight small badges in row 1 are repainted; the rest of the
 * toolbar is untouched.
 *
 * @param cur_note_type Index of the newly selected note type (0-based).
 */
void toolbar_set_note_type(int cur_note_type)
{
    for (int i = 0; i < TB_NUM_NOTE_TYPES; i++) {
        char label[2] = {(char)('1' + i), '\0'};
        short int fill = (i == cur_note_type) ? TB_NOTEA_FILL : TB_NOTE_FILL;
        short int txt  = (i == cur_note_type) ? TB_NOTEA_TXT  : TB_NOTE_TXT;
        tb_badge(note_badge_x(i), BADGE_W1, label, fill, txt);
    }
}
