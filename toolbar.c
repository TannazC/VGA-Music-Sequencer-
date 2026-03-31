/* =======================================================================
   toolbar.c  -  Top keyboard-shortcut toolbar (implementation)
   Optimized for PixelMix (5x7) for maximum legibility.
   Updated for 2-row top layout with compact height to prevent staff overlap.
   ======================================================================= */

#include "toolbar.h"
#include "background.h"
#include "skinny_font.h"

/* Externs defined in main.c or background.c */
extern void plot_pixel(int x, int y, short int c);

/* =======================================================================
   Global state
   ======================================================================= */
ToolbarState toolbar_state = {
    TB_INST_BEEP,     /* instrument */
    120,              /* bpm        */
    0,                /* muted      */
    TB_STATE_STOPPED  /* playback   */
};

/* =======================================================================
   Layout geometry (Compact Mode)
   ======================================================================= */
#define ROW1_Y         0
#define BADGE_AREA_H   22   /* Reduced from 26 */
#define ROW2_Y         22   /* Row 2 starts exactly after Row 1 */
#define BADGE_H        18   /* Reduced from 20 */

#define FONT_ADVANCE   6

/* Widths */
#define BADGE_W_TRANS   31   
#define BADGE_W1        12   
#define BADGE_W_ACC     18
#define BADGE_W_TMP_BTN 12  
#define BADGE_W_TMP_VAL 28  
#define BADGE_GAP       2    
#define GROUP_SEP       8

/* =======================================================================
   Colour palette  (RGB 5-6-5)
   ======================================================================= */
/* Base Toolbar Colors */
#define TB_BG           ((short int)0xDEFB)  
#define TB_BORDER       COLOR_BLACK          
#define TB_DIV          COLOR_BLACK          

/* Transport - Play */
#define TB_PLAY_FILL    COLOR_SPEARMINT  
#define TB_PLAY_FILL_A  COLOR_NEON_SPEARMINT      
#define TB_PLAY_ICO     COLOR_WHITE  
#define TB_PLAY_KEY     COLOR_WHITE
 
/* Transport - Pause */
#define TB_PAUSE_FILL   COLOR_FUCHSIA  
#define TB_PAUSE_FILL_A COLOR_CITRIC  
#define TB_PAUSE_ICO    COLOR_WHITE
#define TB_PAUSE_KEY    COLOR_WHITE
 
/* Transport - Stop */
#define TB_STOP_FILL    COLOR_MUTED_NEON_BLUE  
#define TB_STOP_FILL_A  COLOR_WHITE  
#define TB_STOP_ICO     COLOR_WHITE
#define TB_STOP_KEY     COLOR_WHITE

/* Transport - Restart */
#define TB_REST_FILL    COLOR_CITRIC     
#define TB_REST_FILL_A  COLOR_WHITE      
#define TB_REST_ICO     COLOR_BLACK      
#define TB_REST_KEY     COLOR_BLACK      
 
/* Note-type badges */
#define TB_NOTE_FILL    COLOR_WHITE
#define TB_NOTE_TXT     COLOR_BLACK
#define TB_NOTEA_FILL   COLOR_SPEARMINT
#define TB_NOTEA_TXT    COLOR_WHITE

/* =======================================================================
   Unified Rendering Logic
   ======================================================================= */

static void draw_atlas_char(int x, int y, int idx, int w, int h, int stride, const unsigned char *ptr, short int col) {
    for (int r = 0; r < h; r++) {
        for (int b = 0; b < stride; b++) {
            int offset = (idx * h * stride) + (r * stride) + b;
            unsigned char bits = ptr[offset];
            for (int bit = 0; bit < 8; bit++) {
                if ((b * 8 + bit) < w) {
                    if (bits & (0x80 >> bit)) {
                        plot_pixel(x + (b * 8) + bit, y + r, col);
                    }
                }
            }
        }
    }
}

static void tb_draw_char(int x, int y, unsigned char c, short int col) {
    int idx = get_skinny_font_index(c);
    const unsigned char *flat_ptr = (const unsigned char *)skinny_font_atlas;
    draw_atlas_char(x, y, idx, SKINNY_FONT_WIDTH, SKINNY_FONT_HEIGHT, SKINNY_FONT_STRIDE, flat_ptr, col);
}

/* =======================================================================
   UI Helpers
   ======================================================================= */
static void tb_fill(int x0,int y0,int x1,int y1,short int c) {
    int x,y; for(y=y0;y<=y1;y++) for(x=x0;x<=x1;x++) plot_pixel(x,y,c);
}
static void tb_hline(int x0,int x1,int y,short int c) { int x; for(x=x0;x<=x1;x++) plot_pixel(x,y,c); }
static void tb_vline(int x,int y0,int y1,short int c) { int y; for(y=y0;y<=y1;y++) plot_pixel(x,y,c); }

static void tb_group_div(int x, int y) { tb_vline(x, y + 2, y + BADGE_AREA_H - 3, COLOR_BLACK); }

/* Flexible Badge Drawer */
static int tb_badge_at(int x, int y, int bw, const char *label, short int fill, short int txt) {
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

    for (const char *p = label; *p; p++, tx += FONT_ADVANCE) {
        tb_draw_char(tx, f_y0, (unsigned char)*p, txt);
    }
    return x + bw;
}

static int tb_badge(int x, int bw, const char *label, short int fill, short int txt) {
    return tb_badge_at(x, ROW1_Y, bw, label, fill, txt);
}

/* =======================================================================
   Row 1 Icons
   ======================================================================= */
static const unsigned char ICON_PLAY[12][12] = {
    {0,0,1,1,0,0,0,0,0,0,0,0},{0,0,1,1,1,1,0,0,0,0,0,0},{0,0,1,1,1,1,1,1,0,0,0,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},{0,0,1,1,1,1,1,1,1,1,1,1},{0,0,1,1,1,1,1,1,1,1,1,1},
    {0,0,1,1,1,1,1,1,1,1,0,0},{0,0,1,1,1,1,1,1,0,0,0,0},{0,0,1,1,1,1,0,0,0,0,0,0},
    {0,0,1,1,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0}
};

static const unsigned char ICON_PAUSE[12][12] = {
    {0,0,1,1,1,0,0,1,1,1,0,0},{0,0,1,1,1,0,0,1,1,1,0,0},{0,0,1,1,1,0,0,1,1,1,0,0},
    {0,0,1,1,1,0,0,1,1,1,0,0},{0,0,1,1,1,0,0,1,1,1,0,0},{0,0,1,1,1,0,0,1,1,1,0,0},
    {0,0,1,1,1,0,0,1,1,1,0,0},{0,0,1,1,1,0,0,1,1,1,0,0},{0,0,1,1,1,0,0,1,1,1,0,0},
    {0,0,1,1,1,0,0,1,1,1,0,0},{0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0}
};

static const unsigned char ICON_STOP[12][12] = {
    {0,0,1,1,1,1,1,1,1,1,0,0},{0,0,1,1,1,1,1,1,1,1,0,0},{0,0,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},{0,0,1,1,1,1,1,1,1,1,0,0},{0,0,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},{0,0,1,1,1,1,1,1,1,1,0,0},{0,0,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},{0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0}
};

static const unsigned char ICON_RESTART[12][12] = {
    {0,0,0,0,1,1,1,1,1,0,0,0},{0,0,1,1,0,0,0,0,1,1,1,0},{0,1,1,0,0,0,0,1,1,1,1,0},
    {0,1,1,0,0,0,0,0,1,1,1,0},{1,1,0,0,0,0,0,0,0,1,1,0},{1,1,0,0,0,0,0,0,0,0,1,0},
    {1,1,0,0,0,0,0,0,0,0,0,0},{0,1,0,0,0,0,0,0,0,1,1,0},{0,1,1,0,0,0,0,0,1,1,0,0},
    {0,0,1,1,0,0,0,1,1,0,0,0},{0,0,0,1,1,1,1,1,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0}
};

static void tb_draw_icon_12(int x, int y, const unsigned char icon[12][12], short int col) {
    int r, c; 
    for(r = 0; r < 12; r++)
        for(c = 0; c < 12; c++)
            if(icon[r][c] == 1) plot_pixel(x + c, y + r, col);
}

static int tb_transport_badge(int x, const unsigned char icon[12][12], char key_char, 
                              short int fill, short int icon_col, short int key_col) {
    int bw = BADGE_W_TRANS; 
    int div_x  = x + 19; 
    int icon_x = x + (18 - 12) / 2; 
    int b_y0 = (BADGE_AREA_H - BADGE_H) / 2;
    int icon_y = b_y0 + (BADGE_H - 12) / 2; 
    int key_x  = div_x + ( (x + bw - div_x) - SKINNY_FONT_WIDTH ) / 2 + 1;
    int f_y0 = b_y0 + (BADGE_H - SKINNY_FONT_HEIGHT) / 2;

    tb_fill(x+1, b_y0+1, x+bw-2, b_y0 + BADGE_H-2, fill);
    tb_hline(x, x+bw-1, b_y0, COLOR_BLACK); 
    tb_hline(x, x+bw-1, b_y0 + BADGE_H - 1, COLOR_BLACK);
    tb_vline(x, b_y0, b_y0 + BADGE_H - 1, COLOR_BLACK); 
    tb_vline(x+bw-1, b_y0, b_y0 + BADGE_H - 1, COLOR_BLACK);
    
    tb_draw_icon_12(icon_x, icon_y, icon, icon_col);
    tb_vline(div_x, b_y0 + 2, b_y0 + BADGE_H - 3, COLOR_BLACK);
    tb_draw_char(key_x, f_y0, (unsigned char)key_char, key_col);
    
    return x + bw;
}

static int g_note_badge_x0 = 0;
static int g_trans_x0 = 0;
static int g_bpm_badge_x0 = 0;

static int note_badge_x(int i) { return g_note_badge_x0 + i*(BADGE_W1+BADGE_GAP); }

void draw_toolbar(int cur_note_type) {
    int x, i;
    int is_playing = (toolbar_state.playback == TB_STATE_PLAYING);
    int is_paused  = (toolbar_state.playback == TB_STATE_PAUSED);
 
    tb_fill(0, ROW1_Y, FB_WIDTH-1, ROW1_Y + BADGE_AREA_H - 1, TB_BG);
    tb_hline(0, FB_WIDTH-1, ROW1_Y + BADGE_AREA_H - 1, TB_DIV);
 
    x = 4; g_trans_x0 = x;
    x = tb_transport_badge(x, ICON_PLAY, 'Q', is_playing ? TB_PLAY_FILL_A : TB_PLAY_FILL, TB_PLAY_ICO, TB_PLAY_KEY) + BADGE_GAP;
    x = tb_transport_badge(x, ICON_PAUSE, 'E', is_paused ? TB_PAUSE_FILL_A : TB_PAUSE_FILL, TB_PAUSE_ICO, TB_PAUSE_KEY) + BADGE_GAP;
    x = tb_transport_badge(x, ICON_STOP, 'T', TB_STOP_FILL, TB_STOP_ICO, TB_STOP_KEY) + BADGE_GAP;
    x = tb_transport_badge(x, ICON_RESTART, 'R', TB_REST_FILL, TB_REST_ICO, TB_REST_KEY); 
    
    x += GROUP_SEP/2; tb_group_div(x, ROW1_Y); x += GROUP_SEP/2;
    g_note_badge_x0 = x;
    for(i=0; i<TB_NUM_NOTE_TYPES; i++){
        char label[2] = {(char)('1'+i), '\0'};
        short int fill = (i==cur_note_type) ? TB_NOTEA_FILL : TB_NOTE_FILL;
        short int txt  = (i==cur_note_type) ? TB_NOTEA_TXT  : TB_NOTE_TXT;
        x = tb_badge(x, BADGE_W1, label, fill, txt) + BADGE_GAP;
    }
    x -= BADGE_GAP;
    x += GROUP_SEP/2; tb_group_div(x, ROW1_Y); x += GROUP_SEP/2;
    x = tb_badge(x, BADGE_W_TMP_BTN, "-", COLOR_FUCHSIA, COLOR_WHITE) + BADGE_GAP;
    
    /* Save the X coordinate so we can dynamically overwrite the number later */
    g_bpm_badge_x0 = x; 
    
    /* Draw the initial BPM number badge */
    toolbar_set_bpm(toolbar_state.bpm);
    x += BADGE_W_TMP_VAL + BADGE_GAP; 
    tb_badge(x, BADGE_W_TMP_BTN, "+", COLOR_SPEARMINT, COLOR_WHITE);
}

/* =======================================================================
   Row 2: Accidentals & Actions
   ======================================================================= */
void draw_toolbar_row2(int cur_accidental) {
        
    tb_fill(0, ROW2_Y, FB_WIDTH - 1, ROW2_Y + BADGE_AREA_H - 1, TB_BG);
    tb_hline(0, FB_WIDTH - 1, ROW2_Y + BADGE_AREA_H - 1, TB_DIV);
    int x = 4;
    int split_x = x + 38;
    int badge_y = ROW2_Y + 2;
    tb_badge_at(x, ROW2_Y, 52, "", COLOR_MUTED_NEON_BLUE, COLOR_WHITE);
    tb_vline(split_x, badge_y + 3, badge_y + BADGE_H - 3, COLOR_BLACK);
    tb_draw_string(x + 5, badge_y + 3, "CLEAR", COLOR_WHITE);
    tb_draw_string(split_x + 5, badge_y + 3, "N", COLOR_WHITE);
    x += 52 + BADGE_GAP;
    x += GROUP_SEP/2; 
    tb_group_div(x, ROW2_Y); 
    x += GROUP_SEP/2;

    const char* acc_labels[] = {"Z", "X", "C", "V"}; 
    for(int i = 0; i < 4; i++) {
        short int fill = (i == cur_accidental) ? COLOR_FUCHSIA : COLOR_WHITE;
        short int txt  = (i == cur_accidental) ? COLOR_WHITE : COLOR_BLACK;
        x = tb_badge_at(x, ROW2_Y, 14, acc_labels[i], fill, txt) + BADGE_GAP;
    }

    /* Navigation arrows: up/down/left/right (W/S/A/D) */
    x -= BADGE_GAP;
    x += GROUP_SEP/2;
    tb_group_div(x, ROW2_Y);
    x += GROUP_SEP/2;

    x = tb_badge_at(x, ROW2_Y, 14, "W", COLOR_WHITE, COLOR_BLACK) + BADGE_GAP;
    x = tb_badge_at(x, ROW2_Y, 14, "S", COLOR_WHITE, COLOR_BLACK) + BADGE_GAP;
    x = tb_badge_at(x, ROW2_Y, 14, "A", COLOR_WHITE, COLOR_BLACK) + BADGE_GAP;
    x = tb_badge_at(x, ROW2_Y, 14, "D", COLOR_WHITE, COLOR_BLACK) + BADGE_GAP;

    /* Page navigation: < and > (COMMA / PERIOD) */
    x -= BADGE_GAP;
    x += GROUP_SEP/2;
    tb_group_div(x, ROW2_Y);
    x += GROUP_SEP/2;

    x = tb_badge_at(x, ROW2_Y, 14, "<", COLOR_CITRIC, COLOR_BLACK) + BADGE_GAP;
    x = tb_badge_at(x, ROW2_Y, 14, ">", COLOR_CITRIC, COLOR_BLACK);

}

/* =======================================================================
   Menu and Bottom UI Logic
   ======================================================================= */

void tb_draw_string(int x, int y, const char *str, short int col) {
    const char *p;
    int tx = x;
    for(p = str; *p; p++, tx += FONT_ADVANCE) {
        if (*p == ' ') continue; 
        tb_draw_char(tx, y, (unsigned char)*p, col);
    }
}

void draw_page_indicator(int cur_page, int max_pages) {
    int y = FB_HEIGHT - 16;
    char page_str[9];
    page_str[0] = 'P'; page_str[1] = 'A'; page_str[2] = 'G'; page_str[3] = 'E'; page_str[4] = ' ';
    page_str[5] = '0' + (char)cur_page; page_str[6] = '/'; page_str[7] = '0' + (char)max_pages; page_str[8] = '\0';
    
    int page_w = 8 * FONT_ADVANCE;
    /* Logic to center the text exactly in the middle of the screen */
    int cx = (FB_WIDTH - page_w) / 2 + 15; 

    tb_draw_string(cx, y, page_str, COLOR_BLACK);
}

/* Restored bottom-right tab for the Options menu */
void draw_bottom_tab(void) {
    int w = 78;
    int h = 13;
    int x = FB_WIDTH - w - 8;
    int y = FB_HEIGHT - h - 6;

    /* Use the standard toolbar background color */
    tb_fill(x, y, x + w, y + h, TB_BG);
    
    /* Draw border using our 1px outline helper */
    tb_hline(x, x + w, y, COLOR_BLACK);
    tb_hline(x, x + w, y + h, COLOR_BLACK);
    tb_vline(x, y, y + h, COLOR_BLACK);
    tb_vline(x + w, y, y + h, COLOR_BLACK);

    tb_draw_string(x + 6, y + 3, "[M] OPTIONS", COLOR_BLACK);
}

static void menu_draw_row(int y, const char *label, const char *key, short int fill, short int txt, int selected) {
    int bx = MENU_X0 + 15;
    if (selected) {
        tb_fill(bx, y - 2, bx + 12, y + 10, fill);
    } else {
        tb_hline(bx, bx + 12, y - 2, COLOR_BLACK);
        tb_hline(bx, bx + 12, y + 10, COLOR_BLACK);
        tb_vline(bx, y - 2, y + 10, COLOR_BLACK);
        tb_vline(bx + 12, y - 2, y + 10, COLOR_BLACK);
    }
    tb_draw_char(bx + 4, y + 1, key[0], selected ? txt : COLOR_BLACK);
    tb_draw_string(bx + 20, y + 1, label, selected ? fill : COLOR_BLACK);
}

void draw_options_menu(void) {
    tb_fill(MENU_X0 + 4, MENU_Y0 + 4, MENU_X1 + 4, MENU_Y1 + 4, TB_BORDER); /* Shadow */
    tb_fill(MENU_X0, MENU_Y0, MENU_X1, MENU_Y1, COLOR_BLACK);
    tb_fill(MENU_X0 + 2, MENU_Y0 + 2, MENU_X1 - 2, MENU_Y1 - 2, TB_BG);

    tb_draw_string(MENU_X0 + 45, MENU_Y0 + 10, "OPTIONS MENU", TB_PAUSE_FILL);
    tb_hline(MENU_X0 + 10, MENU_X1 - 10, MENU_Y0 + 24, COLOR_BLACK);

    /* Main level: 1=Change Instrument, 2=Go Back to Main Menu */
    menu_draw_row(MENU_Y0 + 45, "CHANGE INSTRUMENT", "1", TB_PLAY_FILL,  COLOR_WHITE, 0);
    menu_draw_row(MENU_Y0 + 75, "MAIN MENU",         "2", TB_STOP_FILL,  COLOR_WHITE, 0);

    tb_draw_string(MENU_X0 + 32, MENU_Y1 - 20, "PRESS M TO CLOSE", TB_STOP_FILL);
}

void draw_options_menu_instrument(void) {
    tb_fill(MENU_X0 + 4, MENU_Y0 + 4, MENU_X1 + 4, MENU_Y1 + 4, TB_BORDER); /* Shadow */
    tb_fill(MENU_X0, MENU_Y0, MENU_X1, MENU_Y1, COLOR_BLACK);
    tb_fill(MENU_X0 + 2, MENU_Y0 + 2, MENU_X1 - 2, MENU_Y1 - 2, TB_BG);

    tb_draw_string(MENU_X0 + 30, MENU_Y0 + 10, "SELECT INSTRUMENT", TB_PAUSE_FILL);
    tb_hline(MENU_X0 + 10, MENU_X1 - 10, MENU_Y0 + 24, COLOR_BLACK);

    int inst = toolbar_state.instrument;
    menu_draw_row(MENU_Y0 + 45, "BEEP",       "1", TB_PLAY_FILL,  COLOR_WHITE, inst == TB_INST_BEEP);
    menu_draw_row(MENU_Y0 + 70, "PIANO",      "2", TB_PAUSE_FILL, COLOR_WHITE, inst == TB_INST_PIANO);
    menu_draw_row(MENU_Y0 + 95, "XYLOPHONE",  "3", TB_PLAY_FILL,  COLOR_WHITE, inst == TB_INST_XYLOPHONE);
    menu_draw_row(MENU_Y0 +120, "TUBA",       "4", TB_PAUSE_FILL, COLOR_WHITE, inst == TB_INST_TUBA);
    menu_draw_row(MENU_Y0 +145, "BACK",       "5", TB_REST_FILL,  COLOR_BLACK, 0);

    tb_draw_string(MENU_X0 + 32, MENU_Y1 - 20, "PRESS M TO CLOSE", TB_STOP_FILL);
}

void toolbar_set_instrument(int inst) {
    toolbar_state.instrument = inst;
    draw_options_menu_instrument();
}

void toolbar_set_bpm(int bpm) {
    char str[4];
    if (bpm > 999) bpm = 999;
    if (bpm < 40) bpm = 40;
    toolbar_state.bpm = bpm;
    str[0] = '0' + (bpm / 100);
    str[1] = '0' + ((bpm / 10) % 10);
    str[2] = '0' + (bpm % 10);
    str[3] = '\0';
    char *display_str = (str[0] == '0') ? &str[1] : str;
    tb_badge_at(g_bpm_badge_x0, ROW1_Y, BADGE_W_TMP_VAL, display_str, COLOR_WHITE, COLOR_BLACK);
}

void toolbar_set_note_type(int cur_note_type) {
    for(int i=0; i<TB_NUM_NOTE_TYPES; i++){
        char label[2] = {(char)('1'+i), '\0'};
        short int fill = (i==cur_note_type) ? TB_NOTEA_FILL : TB_NOTE_FILL;
        short int txt  = (i==cur_note_type) ? TB_NOTEA_TXT  : TB_NOTE_TXT;
        tb_badge(note_badge_x(i), BADGE_W1, label, fill, txt);
    }
}