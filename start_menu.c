/* ============================================================
   start_screen.c  –  VGA start screen for music sequencer
   ============================================================ */

#include "start_menu.h"
#include "toolbar.h"
#include "background.h"

/* plot_pixel is defined in vga_music_v2.c; declare it here */
extern void plot_pixel(int x, int y, short int c);

volatile int g_start_screen_active = 0;
int g_start_selection = 1;
int g_song_selection  = 1;   /* 1=Ode to Joy, 2=O Canada, 3=Fur Elise, 4=Do Re Mi */

/* -------------------------------------------------------
   Colour palette  (RGB 5-6-5)  — additional colours local to the start screen
   ------------------------------------------------------- */
#define COLOR_PINK_BG         RGB565(244, 184, 206)
#define COLOR_STAFF           RGB565(210, 155, 178)
#define COLOR_SPEARMINT_DARK  RGB565( 50, 105,  90)
#define COLOR_LIGHT_GRAY      RGB565(215, 205, 210)
#define COLOR_DIM_TEXT        RGB565(160, 140, 150)

/* -------------------------------------------------------
   VGA resolution
   ------------------------------------------------------- */
#define SCREEN_W  320
#define SCREEN_H  240

/* -------------------------------------------------------
   5×7 bitmap font
   ------------------------------------------------------- */
#define FONT_START  32
#define FONT_END    90
#define FONT_W       5
#define FONT_H       7

static const unsigned char font[][FONT_H] = {
/* ' ' 32 */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
/* '!' 33 */ {0x04,0x04,0x04,0x04,0x04,0x00,0x04},
/* '"' 34 */ {0x0A,0x0A,0x00,0x00,0x00,0x00,0x00},
/* '#' 35 */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
/* '$' 36 */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
/* '%' 37 */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
/* '&' 38 */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
/* ''' 39 */ {0x04,0x04,0x00,0x00,0x00,0x00,0x00},
/* '(' 40 */ {0x02,0x04,0x04,0x04,0x04,0x04,0x02},
/* ')' 41 */ {0x08,0x04,0x04,0x04,0x04,0x04,0x08},
/* '*' 42 */ {0x00,0x15,0x0E,0x1F,0x0E,0x15,0x00},
/* '+' 43 */ {0x00,0x04,0x04,0x1F,0x04,0x04,0x00},
/* ',' 44 */ {0x00,0x00,0x00,0x00,0x06,0x04,0x08},
/* '-' 45 */ {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},
/* '.' 46 */ {0x00,0x00,0x00,0x00,0x00,0x06,0x06},
/* '/' 47 */ {0x01,0x01,0x02,0x04,0x08,0x10,0x10},
/* '0' 48 */ {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
/* '1' 49 */ {0x04,0x0C,0x04,0x04,0x04,0x04,0x1F},
/* '2' 50 */ {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F},
/* '3' 51 */ {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E},
/* '4' 52 */ {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
/* '5' 53 */ {0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E},
/* '6' 54 */ {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
/* '7' 55 */ {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
/* '8' 56 */ {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
/* '9' 57 */ {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
/* ':' 58 */ {0x00,0x06,0x06,0x00,0x06,0x06,0x00},
/* ';' 59 */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
/* '<' 60 */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
/* '=' 61 */ {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00},
/* '>' 62 */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
/* '?' 63 */ {0x0E,0x11,0x01,0x06,0x04,0x00,0x04},
/* '@' 64 */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
/* 'A' 65 */ {0x04,0x0A,0x11,0x1F,0x11,0x11,0x11},
/* 'B' 66 */ {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
/* 'C' 67 */ {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
/* 'D' 68 */ {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
/* 'E' 69 */ {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
/* 'F' 70 */ {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
/* 'G' 71 */ {0x0E,0x11,0x10,0x17,0x11,0x11,0x0E},
/* 'H' 72 */ {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
/* 'I' 73 */ {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F},
/* 'J' 74 */ {0x1F,0x01,0x01,0x01,0x01,0x11,0x0E},
/* 'K' 75 */ {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
/* 'L' 76 */ {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
/* 'M' 77 */ {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
/* 'N' 78 */ {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
/* 'O' 79 */ {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
/* 'P' 80 */ {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
/* 'Q' 81 */ {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
/* 'R' 82 */ {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
/* 'S' 83 */ {0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E},
/* 'T' 84 */ {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
/* 'U' 85 */ {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
/* 'V' 86 */ {0x11,0x11,0x11,0x11,0x0A,0x0A,0x04},
/* 'W' 87 */ {0x11,0x11,0x11,0x15,0x15,0x1B,0x11},
/* 'X' 88 */ {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
/* 'Y' 89 */ {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
/* 'Z' 90 */ {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
};

static void fill_rect(int x, int y, int w, int h, short int color) {
    int i, j;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++)
            plot_pixel(i, j, color);
}

static void draw_hline(int x, int y, int len, short int color) {
    int i;
    for (i = 0; i < len; i++) plot_pixel(x + i, y, color);
}

static void draw_vline(int x, int y, int len, short int color) {
    int i;
    for (i = 0; i < len; i++) plot_pixel(x, y + i, color);
}

static void draw_rect_outline(int x, int y, int w, int h, short int color) {
    draw_hline(x,     y,         w, color);
    draw_hline(x,     y + h - 1, w, color);
    draw_vline(x,         y, h, color);
    draw_vline(x + w - 1, y, h, color);
}

static void draw_char(int x, int y, char c, int scale, short int fg) {
    int row, col, s, t;
    unsigned char bits;
    int idx;
    if ((int)c < FONT_START || (int)c > FONT_END) return;
    idx = (int)c - FONT_START;
    for (row = 0; row < FONT_H; row++) {
        bits = font[idx][row];
        for (col = 0; col < FONT_W; col++) {
            if (bits & (0x10 >> col)) {
                for (s = 0; s < scale; s++)
                    for (t = 0; t < scale; t++)
                        plot_pixel(x + col * scale + t, y + row * scale + s, fg);
            }
        }
    }
}

static int draw_string(int x, int y, const char *str, int scale, short int fg) {
    while (*str) {
        draw_char(x, y, *str, scale, fg);
        x += (FONT_W + 1) * scale;
        str++;
    }
    return x;
}

static void draw_string_centered(int cx, int y, const char *str, int scale, short int fg) {
    int n = 0, total_w;
    const char *p = str;
    while (*p++) n++;
    total_w = n * FONT_W * scale + (n - 1) * scale;
    draw_string(cx - total_w / 2, y, str, scale, fg);
}

static void draw_staff(int y0) {
    int i;
    for (i = 0; i < 5; i++) draw_hline(0, y0 + i * 6, SCREEN_W, COLOR_STAFF);
}

static void draw_option(int x, int y, int w, int h, char key, const char *label, int active) {
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

    kx = x + (badge_w - FONT_W * 2) / 2;
    ky = y + (h       - FONT_H * 2) / 2;
    draw_char(kx, ky, key, 2, active ? COLOR_WHITE : COLOR_DIM_TEXT);

    draw_vline(x + badge_w, y, h, bdr_color);

    lx = x + badge_w + 6;
    ly = y + (h - FONT_H) / 2;
    draw_string(lx, ly, label, 1, txt_color);

    draw_rect_outline(x, y, w, h, bdr_color);
}
/* =================================================================
   Draws the broad 240x32 customized 'MUSIC SEQUENCER' logo.
   This asset uses a full 2D array of bytes for a cohesive pre-render.
   ================================================================= */
static void draw_custom_logo_bitmap(double scale) {
    int tx, ty;
    short int color = COLOR_SPEARMINT; /* Primary theme color */

    /* Calculate exact target dimensions based on the double */
    int target_w = (int)(TITLE_LOGO_W * scale);
    int target_h = (int)(TITLE_LOGO_H * scale);

    /* Recalculate centering using the new scaled width */
    int start_x = (SCREEN_W - target_w) / 2;
    int start_y = 40; 

    /* Loop over every pixel in the target destination */
    for (ty = 0; ty < target_h; ty++) {
        /* Reverse map to find the source Y coordinate */
        int src_y = (int)(ty / scale);
        
        for (tx = 0; tx < target_w; tx++) {
            /* Reverse map to find the source X coordinate */
            int src_x = (int)(tx / scale);
            
            /* Translate the X coordinate into our byte/bit layout */
            int col_byte = src_x / 8;
            int bit = src_x % 8;
            
            /* Bounds check just in case of any floating point rounding quirks */
            if (src_y < TITLE_LOGO_H && col_byte < TITLE_LOGO_STRIDE) {
                unsigned char bits = music_sequencer_bmp[src_y][col_byte];
                
                /* Read the bit (MSB first) */
                if (bits & (0x80 >> bit)) {
                    plot_pixel(start_x + tx, start_y + ty, color);
                }
            }
        }
    }
}

/* =================================================================
   Public Interface
   ================================================================= */
void draw_start_screen(void) {
    g_start_screen_active = 1;
    g_start_selection = 1; /* Default to Create Your Own */

    fill_rect(0, 0, SCREEN_W, SCREEN_H, COLOR_PINK_BG);

    draw_staff(14); 
    draw_treble_clef(10, 14 - 6, COLOR_STAFF);
    draw_staff(184);
    draw_treble_clef(10, 184 - 6, COLOR_STAFF);

    /* Text */
    /*draw_string_centered(SCREEN_W / 2, 50, "MUSIC SEQUENCER", 3, COLOR_SPEARMINT);
    draw_string_centered(SCREEN_W / 2, 92, "START", 1, COLOR_BLACK);*/
    draw_custom_logo_bitmap(1.5);

    draw_hline(90, 95, 140, COLOR_SPEARMINT);

    /* Draw the initial buttons */
    update_start_selection(g_start_selection);
}

/* Repaints ONLY the buttons, preventing full-screen flicker when navigating */
void update_start_selection(int active_opt) {
    draw_option(80, 115, 160, 24, '1', "CREATE YOUR OWN", (active_opt == 1));
    draw_option(80, 145, 160, 24, '2', "PRELOAD SONG",    (active_opt == 2));
}

/* =================================================================
   Song-select submenu  (appears after choosing option 2)
   ================================================================= */
void update_song_selection(int active_opt) {
    draw_option(50,  99, 220, 22, '1', "DO RE MI",          (active_opt == 1));
    draw_option(50, 123, 220, 22, '2', "FUR ELISE",            (active_opt == 2));
    draw_option(50, 147, 220, 22, '3', "ODE TO JOY",           (active_opt == 3));
    draw_option(50, 171, 220, 22, '4', "SEVEN NATION ARMY",            (active_opt == 4));
    draw_option(50, 201, 220, 22, '5', "BACK TO MAIN MENU",   0);
}

void draw_song_select_screen(void) {
    g_start_screen_active = 1;
    g_song_selection = 1;

    fill_rect(0, 0, SCREEN_W, SCREEN_H, COLOR_PINK_BG);

    draw_staff(14);
    draw_treble_clef(10, 14 - 6, COLOR_STAFF);

    draw_custom_logo_bitmap(1.5);

    draw_hline(50, 80, 220, COLOR_SPEARMINT);
    draw_string_centered(SCREEN_W / 2, 85, "SELECT A SONG", 1, COLOR_SPEARMINT);

    update_song_selection(g_song_selection);
}