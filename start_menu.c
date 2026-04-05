/**
 * @file start_menu.c
 * @brief VGA start screen and song-select menu for the music sequencer.
 *
 * Renders the title screen (logo, staff decorations, menu options) and the
 * song-select submenu. Provides functions to redraw only the interactive
 * button rows so navigation does not cause full-screen flicker.
 *
 * @authors Tannaz Chowdhury, Dareen Nasreldin
 */

#include "start_menu.h"
#include "toolbar.h"
#include "background.h"

/** @brief plot_pixel is defined in main.c. */
extern void plot_pixel(int x, int y, short int c);

/** @brief Non-zero while the start or song-select screen is active. */
volatile int g_start_screen_active = 0;

/** @brief Currently highlighted option on the main start screen (1 or 2). */
int g_start_selection = 1;

/**
 * @brief Currently highlighted song on the song-select screen.
 *
 * 1 = Do Re Mi, 2 = Fur Elise, 3 = Ode to Joy, 4 = Seven Nation Army.
 */
int g_song_selection = 1;

/* ═══════════════════════════════════════════════════════════════════════
   Colour palette  (RGB565) — local to the start screen
   ═══════════════════════════════════════════════════════════════════════ */
#define COLOR_PINK_BG        RGB565(244, 184, 206)
#define COLOR_STAFF          RGB565(210, 155, 178)
#define COLOR_SPEARMINT_DARK RGB565( 50, 105,  90)
#define COLOR_LIGHT_GRAY     RGB565(215, 205, 210)
#define COLOR_DIM_TEXT       RGB565(160, 140, 150)

/* ═══════════════════════════════════════════════════════════════════════
   Screen dimensions
   ═══════════════════════════════════════════════════════════════════════ */
#define SCREEN_W 320
#define SCREEN_H 240

/* ═══════════════════════════════════════════════════════════════════════
   Drawing primitives
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Fills a rectangle with a solid colour.
 *
 * @param x     Left edge in screen coordinates.
 * @param y     Top edge in screen coordinates.
 * @param w     Width in pixels.
 * @param h     Height in pixels.
 * @param color RGB565 fill colour.
 */
static void fill_rect(int x, int y, int w, int h, short int color)
{
    int i, j;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++)
            plot_pixel(i, j, color);
}

/**
 * @brief Draws a horizontal line of a given length.
 *
 * @param x     Start X coordinate.
 * @param y     Y coordinate.
 * @param len   Length in pixels.
 * @param color RGB565 colour.
 */
static void draw_hline(int x, int y, int len, short int color)
{
    int i;
    for (i = 0; i < len; i++) plot_pixel(x + i, y, color);
}

/**
 * @brief Draws a vertical line of a given length.
 *
 * @param x     X coordinate.
 * @param y     Start Y coordinate.
 * @param len   Length in pixels.
 * @param color RGB565 colour.
 */
static void draw_vline(int x, int y, int len, short int color)
{
    int i;
    for (i = 0; i < len; i++) plot_pixel(x, y + i, color);
}

/**
 * @brief Draws a hollow rectangle outline (1-pixel border).
 *
 * @param x     Left edge.
 * @param y     Top edge.
 * @param w     Width in pixels.
 * @param h     Height in pixels.
 * @param color RGB565 border colour.
 */
static void draw_rect_outline(int x, int y, int w, int h, short int color)
{
    draw_hline(x,         y,         w, color);
    draw_hline(x,         y + h - 1, w, color);
    draw_vline(x,         y,         h, color);
    draw_vline(x + w - 1, y,         h, color);
}



/**
 * @brief Renders a string horizontally centred around a given X coordinate.
 *
 * @param cx    Centre X coordinate.
 * @param y     Top Y coordinate.
 * @param str   Null-terminated ASCII string.
 * @param scale Pixel scale factor.
 * @param fg    RGB565 foreground colour.
 */
static void draw_string_centered(int cx, int y, const char *str, short int fg)
{
    int n = 0;
    const char *p = str;
    while (*p++) n++;
    int total_w = n * 6; 
    tb_draw_string(cx - total_w / 2, y, str, fg);
}

/**
 * @brief Draws a 5-line musical staff across the full screen width.
 *
 * Lines are spaced 6 pixels apart, matching the sequencer grid spacing.
 *
 * @param y0 Y coordinate of the top staff line.
 */
static void draw_staff(int y0)
{
    int i;
    for (i = 0; i < 5; i++) draw_hline(0, y0 + i * 6, SCREEN_W, COLOR_STAFF);
}

/**
 * @brief Draws a labelled menu option button with a key badge on the left.
 *
 * The button is rendered in active (highlighted) or inactive style depending
 * on the active parameter. Active buttons use the spearmint colour scheme;
 * inactive buttons use white/gray.
 *
 * @param x      Left edge of the button.
 * @param y      Top edge of the button.
 * @param w      Total button width in pixels.
 * @param h      Button height in pixels.
 * @param key    Single character shown in the left badge (e.g. '1', '2').
 * @param label  Null-terminated label string drawn to the right of the badge.
 * @param active Non-zero to render in the highlighted/active style.
 */
static void draw_option(int x, int y, int w, int h,
                        char key, const char *label, int active)
{
    short int box_fill, badge_fill, txt_color, bdr_color;
    int badge_w, kx, ky, lx, ly;

    if (active) {
        box_fill   = COLOR_SPEARMINT;
        badge_fill = COLOR_SPEARMINT_DARK;
        txt_color  = COLOR_WHITE;
        bdr_color  = COLOR_BLACK;
    } else {
        box_fill   = COLOR_WHITE;
        badge_fill = COLOR_LIGHT_GRAY;
        txt_color  = COLOR_DIM_TEXT;
        bdr_color  = COLOR_GRAY;
    }

    badge_w = h;
    fill_rect(x, y, w, h, box_fill);
    fill_rect(x, y, badge_w, h, badge_fill);

    // Key badge alignment: 6px width, 9px height for the skinny font
    char key_str[2] = {key, '\0'};
    kx = x + (badge_w - 6) / 2;
    ky = y + (h - 9) / 2 + 1; // Slight nudge for centering
    tb_draw_string(kx, ky, key_str, active ? COLOR_WHITE : COLOR_DIM_TEXT);

    draw_vline(x + badge_w, y, h, bdr_color);

    // Label alignment
    lx = x + badge_w + 8;
    ly = y + (h - 9) / 2 + 1;
    tb_draw_string(lx, ly, label, txt_color);

    draw_rect_outline(x, y, w, h, bdr_color);
}

/**
 * @brief Renders the "MUSIC SEQUENCER" title logo bitmap at a given scale.
 *
 * Uses nearest-neighbour scaling: each destination pixel reverse-maps to a
 * source pixel in music_sequencer_bmp[][] (defined in start_menu.h). The
 * bitmap is stored as packed bytes (MSB = leftmost pixel) with stride
 * TITLE_LOGO_STRIDE. The rendered logo is horizontally centred on screen
 * and placed at y = 40.
 *
 * @param scale Scaling factor (e.g. 1.5 renders at 150% of the bitmap size).
 */
static void draw_custom_logo_bitmap(double scale)
{
    int tx, ty;
    short int color = COLOR_SPEARMINT;

    int target_w = (int)(TITLE_LOGO_W * scale);
    int target_h = (int)(TITLE_LOGO_H * scale);
    int start_x  = (SCREEN_W - target_w) / 2;
    int start_y  = 40;

    for (ty = 0; ty < target_h; ty++) {
        int src_y = (int)(ty / scale);

        for (tx = 0; tx < target_w; tx++) {
            int src_x    = (int)(tx / scale);
            int col_byte = src_x / 8;
            int bit      = src_x % 8;

            if (src_y < TITLE_LOGO_H && col_byte < TITLE_LOGO_STRIDE) {
                unsigned char bits = music_sequencer_bmp[src_y][col_byte];
                if (bits & (0x80 >> bit))
                    plot_pixel(start_x + tx, start_y + ty, color);
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   Public interface
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Renders the main start screen.
 *
 * Fills the background, draws decorative staves and treble clefs, renders
 * the title logo, and draws the initial menu buttons via
 * update_start_selection(). Sets g_start_screen_active and resets
 * g_start_selection to 1.
 */
void draw_start_screen(void)
{
    g_start_screen_active = 1;
    g_start_selection = 1;

    fill_rect(0, 0, SCREEN_W, SCREEN_H, COLOR_PINK_BG);

    draw_staff(14);
    draw_treble_clef(10, 14 - 6, COLOR_STAFF);
    draw_staff(184);
    draw_treble_clef(10, 184 - 6, COLOR_STAFF);

    draw_custom_logo_bitmap(1.5);

    draw_hline(90, 95, 140, COLOR_SPEARMINT);

    update_start_selection(g_start_selection);
}

/**
 * @brief Repaints only the two main-menu buttons to reflect the current
 *        selection without redrawing the full screen.
 *
 * Call this whenever g_start_selection changes to avoid flicker.
 *
 * @param active_opt Currently highlighted option (1 = Create Your Own,
 *                   2 = Preload Song).
 */
void update_start_selection(int active_opt)
{
    draw_option(80, 115, 160, 24, '1', "CREATE YOUR OWN", (active_opt == 1));
    draw_option(80, 145, 160, 24, '2', "PRELOAD SONG",    (active_opt == 2));
}

/**
 * @brief Renders the full song-select screen.
 *
 * Fills the background, draws a decorative staff and logo, and calls
 * update_song_selection() to draw all song option buttons. Sets
 * g_start_screen_active and resets g_song_selection to 1.
 */
void draw_song_select_screen(void)
{
    g_start_screen_active = 1;
    g_song_selection = 1;

    fill_rect(0, 0, SCREEN_W, SCREEN_H, COLOR_PINK_BG);

    draw_staff(14);
    draw_treble_clef(10, 14 - 6, COLOR_STAFF);

    draw_custom_logo_bitmap(1.5);

    draw_hline(50, 80, 220, COLOR_SPEARMINT);
    draw_string_centered(SCREEN_W / 2, 85, "SELECT A SONG", COLOR_SPEARMINT);
    update_song_selection(g_song_selection);
}

/**
 * @brief Repaints only the song-select buttons to reflect the current
 *        selection without redrawing the full screen.
 *
 * @param active_opt Currently highlighted song (1–4). Option 5 (Back) is
 *                   always rendered inactive.
 */
void update_song_selection(int active_opt)
{
    draw_option(50,  99, 220, 22, '1', "DO RE MI",          (active_opt == 1));
    draw_option(50, 123, 220, 22, '2', "FUR ELISE",         (active_opt == 2));
    draw_option(50, 147, 220, 22, '3', "ODE TO JOY",        (active_opt == 3));
    draw_option(50, 171, 220, 22, '4', "SEVEN NATION ARMY", (active_opt == 4));
    draw_option(50, 201, 220, 22, '5', "BACK TO MAIN MENU", 0);
}
