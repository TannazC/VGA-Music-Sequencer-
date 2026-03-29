/* =======================================================================
   toolbar.c  --  Top keyboard-shortcut toolbar (implementation)
   Optimized for PixelMix (5x7) for maximum legibility.
   Uses flat-pointer arithmetic to bypass VLA compiler restrictions.
   ======================================================================= */

#include "toolbar.h"
#include "background.h"
#include "skinny_font.h"

/* plot_pixel is defined in main.c */
extern void plot_pixel(int x, int y, short int c);

/* =======================================================================
   Global state
   ======================================================================= */
ToolbarState toolbar_state = {
    TB_WAVE_SQUARE, 120, 0, TB_STATE_STOPPED
};

/* =======================================================================
   Layout geometry
   ======================================================================= */
#define BADGE_AREA_H   26          
#define BADGE_H        20          
#define BADGE_Y0       (TOOLBAR_TOP + (BADGE_AREA_H - BADGE_H) / 2)   
#define BADGE_Y1       (BADGE_Y0 + BADGE_H - 1)                        

/* Automatically centers the font vertically based on the atlas height */
#define FONT_Y0        (BADGE_Y0 + (BADGE_H - SKINNY_FONT_HEIGHT) / 2)                  
#define FONT_ADVANCE   6

/* Widths */
#define BADGE_W_TRANS   31   
#define BADGE_W1        12   
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
 
/* Action badges */
#define TB_SPC_FILL     COLOR_SPEARMINT  
#define TB_SPC_TXT      COLOR_WHITE
#define TB_DEL_FILL     COLOR_FUCHSIA
#define TB_DEL_TXT      COLOR_WHITE

/* =======================================================================
   Unified Rendering Logic (Engineering Grade - No VLAs)
   ======================================================================= */

/**
 * Renders a character from a 3D atlas passed as a flat pointer.
 * Offset math: (char_idx * height * stride) + (row * stride) + byte_col
 */
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

static void tb_group_div(int x) { tb_vline(x, TOOLBAR_TOP + 2, BADGE_Y1 - 2, COLOR_BLACK); }

static int tb_badge(int x, int bw, const char *label, short int fill, short int txt) {
    int llen = 0;
    for (const char *p = label; *p; p++) llen++;

    /* Centering calculation: nudged +1 pixel to the right for visual balance */
    int text_w = llen * SKINNY_FONT_WIDTH; 
    int tx = x + (bw - text_w) / 2 + 3; 

    tb_fill(x + 1, BADGE_Y0 + 1, x + bw - 2, BADGE_Y1 - 1, fill);
    tb_hline(x, x + bw - 1, BADGE_Y0, COLOR_BLACK); 
    tb_hline(x, x + bw - 1, BADGE_Y1, COLOR_BLACK);
    tb_vline(x, BADGE_Y0, BADGE_Y1, COLOR_BLACK); 
    tb_vline(x + bw - 1, BADGE_Y0, BADGE_Y1, COLOR_BLACK);

    for (const char *p = label; *p; p++, tx += SKINNY_FONT_WIDTH) {
        tb_draw_char(tx, FONT_Y0, (unsigned char)*p, txt);
    }
    return x + bw;
}


/* =======================================================================
   Unified 12x12 Transport Icons (1 = draw, 0 = transparent)
   ======================================================================= */
static const unsigned char ICON_PLAY[12][12] = {
    {0,0,1,1,0,0,0,0,0,0,0,0},
    {0,0,1,1,1,1,0,0,0,0,0,0},
    {0,0,1,1,1,1,1,1,0,0,0,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,1,1,1,1},
    {0,0,1,1,1,1,1,1,1,1,1,1},
    {0,0,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,0,0,0,0},
    {0,0,1,1,1,1,0,0,0,0,0,0},
    {0,0,1,1,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0}
};

static const unsigned char ICON_PAUSE[12][12] = {
    {0,0,1,1,1,0,0,1,1,1,0,0},
    {0,0,1,1,1,0,0,1,1,1,0,0},
    {0,0,1,1,1,0,0,1,1,1,0,0},
    {0,0,1,1,1,0,0,1,1,1,0,0},
    {0,0,1,1,1,0,0,1,1,1,0,0},
    {0,0,1,1,1,0,0,1,1,1,0,0},
    {0,0,1,1,1,0,0,1,1,1,0,0},
    {0,0,1,1,1,0,0,1,1,1,0,0},
    {0,0,1,1,1,0,0,1,1,1,0,0},
    {0,0,1,1,1,0,0,1,1,1,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0}
};

static const unsigned char ICON_STOP[12][12] = {
    {0,0,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0}
};

static const unsigned char ICON_RESTART[12][12] = {
    {0,0,0,0,1,1,1,1,1,0,0,0},
    {0,0,1,1,0,0,0,0,1,1,1,0},
    {0,1,1,0,0,0,0,1,1,1,1,0},
    {0,1,1,0,0,0,0,0,1,1,1,0},
    {1,1,0,0,0,0,0,0,0,1,1,0},
    {1,1,0,0,0,0,0,0,0,0,1,0},
    {1,1,0,0,0,0,0,0,0,0,0,0},
    {0,1,0,0,0,0,0,0,0,1,1,0},
    {0,1,1,0,0,0,0,0,1,1,0,0},
    {0,0,1,1,0,0,0,1,1,0,0,0},
    {0,0,0,1,1,1,1,1,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0}
};


/* Draws the unified 12x12 icon grid */
static void tb_draw_icon_12(int x, int y, const unsigned char icon[12][12], short int col) {
    int row, col_idx; 
    for(row = 0; row < 12; row++){
        for(col_idx = 0; col_idx < 12; col_idx++){
            if(icon[row][col_idx] == 1) {
                plot_pixel(x + col_idx, y + row, col);
            }
        }
    }
}

 
/* Single, unified transport badge for ALL 4 buttons */
static int tb_transport_badge(int x, const unsigned char icon[12][12], char key_char, 
                              short int fill, short int icon_col, short int key_col) {
    int bw = BADGE_W_TRANS; 
    int div_x  = x + 19; 
    
    // Center the 12x12 icon in the left side (18px wide)
    int icon_x = x + (18 - 12) / 2; 
    int icon_y = BADGE_Y0 + (BADGE_H - 12) / 2; 
    
    // Center the single char in the right side (14px wide), nudged +1 pixel to the right
    int key_x  = div_x + ( (x + bw - div_x) - SKINNY_FONT_WIDTH ) / 2 + 1;

    tb_fill(x+1, BADGE_Y0+1, x+bw-2, BADGE_Y1-1, fill);
    tb_hline(x, x+bw-1, BADGE_Y0, COLOR_BLACK); 
    tb_hline(x, x+bw-1, BADGE_Y1, COLOR_BLACK);
    tb_vline(x, BADGE_Y0, BADGE_Y1, COLOR_BLACK); 
    tb_vline(x+bw-1, BADGE_Y0, BADGE_Y1, COLOR_BLACK);
    
    for(int r=0; r<12; r++)
        for(int c=0; c<12; c++)
            if(icon[r][c]) plot_pixel(icon_x+c, icon_y+r, icon_col);

    tb_vline(div_x, BADGE_Y0 + 2, BADGE_Y1 - 2, COLOR_BLACK);
    tb_draw_char(key_x, FONT_Y0, (unsigned char)key_char, key_col);
    
    return x + bw;
}

//static void tb_group_div(int x) { tb_vline(x,TOOLBAR_TOP+2,BADGE_Y1,TB_DIV); }
 
/* =======================================================================
   Internal state saved for partial redraws
   ======================================================================= */
static int g_note_badge_x0 = 0;
static int g_trans_x0 = 0;
static int g_bpm_badge_x0 = 0;

static int note_badge_x(int i) { return g_note_badge_x0 + i*(BADGE_W1+BADGE_GAP); }
 
/* =======================================================================
   draw_toolbar  — full first-time draw
   ======================================================================= */
void draw_toolbar(int cur_note_type)
{
    int x,i; char label[2]; short int fill,txt;
    int is_playing = (toolbar_state.playback == TB_STATE_PLAYING);
    int is_paused  = (toolbar_state.playback == TB_STATE_PAUSED);
 
    tb_fill(0,TOOLBAR_TOP,FB_WIDTH-1,TOOLBAR_BOT,TB_BG);
    tb_hline(0,FB_WIDTH-1,BADGE_AREA_H,TB_DIV);
 
    x = 4; g_trans_x0 = x;
 
    /* Group 1: Transport */
    fill = is_playing ? TB_PLAY_FILL_A : TB_PLAY_FILL;
    x = tb_transport_badge(x, ICON_PLAY, 'Q', fill, TB_PLAY_ICO, TB_PLAY_KEY) + BADGE_GAP;
    fill = is_paused ? TB_PAUSE_FILL_A : TB_PAUSE_FILL;
    x = tb_transport_badge(x, ICON_PAUSE, 'E', fill, TB_PAUSE_ICO, TB_PAUSE_KEY) + BADGE_GAP;
    x = tb_transport_badge(x, ICON_STOP, 'T', TB_STOP_FILL, TB_STOP_ICO, TB_STOP_KEY) + BADGE_GAP;
    fill = TB_REST_FILL;
    x = tb_transport_badge(x, ICON_RESTART, 'R', fill, TB_REST_ICO, TB_REST_KEY); 
    
    x += GROUP_SEP/2; tb_group_div(x); x += GROUP_SEP/2;
 
    /* Group 2: Note type 1-7 */
    g_note_badge_x0 = x; label[1] = '\0';
    for(i=0;i<TB_NUM_NOTE_TYPES;i++){
        label[0]=(char)('1'+i);
        fill = (i==cur_note_type) ? TB_NOTEA_FILL : TB_NOTE_FILL;
        txt  = (i==cur_note_type) ? TB_NOTEA_TXT  : TB_NOTE_TXT;
        x = tb_badge(x,BADGE_W1,label,fill,txt)+BADGE_GAP;
    }
    x -= BADGE_GAP;
 
    x += GROUP_SEP/2; tb_group_div(x); x += GROUP_SEP/2;
 
    /* Group 3: Tempo Control [-] [120] [+] */
    x = tb_badge(x, BADGE_W_TMP_BTN, "-", COLOR_FUCHSIA, COLOR_WHITE) + BADGE_GAP;
    
    /* Save the X coordinate so we can dynamically overwrite the number later */
    g_bpm_badge_x0 = x; 
    
    /* Draw the initial BPM number badge */
    toolbar_set_bpm(toolbar_state.bpm);
    x += BADGE_W_TMP_VAL + BADGE_GAP; 
    
    x = tb_badge(x, BADGE_W_TMP_BTN, "+", COLOR_SPEARMINT, COLOR_WHITE);
}
 
/* =======================================================================
   toolbar_set_note_type
   ======================================================================= */
void toolbar_set_note_type(int cur_note_type)
{
    int i; char label[2]; short int fill,txt; label[1]='\0';
    for(i=0;i<TB_NUM_NOTE_TYPES;i++){
        label[0]=(char)('1'+i);
        fill = (i==cur_note_type) ? TB_NOTEA_FILL : TB_NOTE_FILL;
        txt  = (i==cur_note_type) ? TB_NOTEA_TXT  : TB_NOTE_TXT;
        tb_badge(note_badge_x(i),BADGE_W1,label,fill,txt);
    }
}
 
/* =======================================================================
   toolbar_set_playback
   ======================================================================= */
void toolbar_set_playback(int state)
{
    int x = g_trans_x0; short int fill;
    int is_playing = (state == TB_STATE_PLAYING);
    int is_paused  = (state == TB_STATE_PAUSED);
    toolbar_state.playback = state;
 
    fill = is_playing ? TB_PLAY_FILL_A : TB_PLAY_FILL;
    x = tb_transport_badge(x, ICON_PLAY, 'Q', fill, TB_PLAY_ICO, TB_PLAY_KEY) + BADGE_GAP;
    fill = is_paused ? TB_PAUSE_FILL_A : TB_PAUSE_FILL;
    x = tb_transport_badge(x, ICON_PAUSE, 'E', fill, TB_PAUSE_ICO, TB_PAUSE_KEY) + BADGE_GAP;
    x = tb_transport_badge(x, ICON_STOP, 'T', TB_STOP_FILL, TB_STOP_ICO, TB_STOP_KEY) + BADGE_GAP;
    tb_transport_badge(x, ICON_RESTART, 'R', TB_REST_FILL, TB_REST_ICO, TB_REST_KEY);
}
/* =======================================================================
   Menu and Overlay Logic
   ======================================================================= */

/* Helper to draw full words instead of single characters */
void tb_draw_string(int x, int y, const char *str, short int col) {
    const char *p;
    int tx = x;
    for(p = str; *p; p++, tx += FONT_ADVANCE) {
        if (*p == ' ') continue; /* Skip drawing, just advance X coordinate */
        tb_draw_char(tx, y, (unsigned char)*p, col);
    }
}

/* Draws a small tab at the bottom right of the screen */
void draw_bottom_tab(void) {
    int w = 75;
    int h = 14;
    int x = FB_WIDTH - w - 10;
    int y = FB_HEIGHT - h - 10;
    
    tb_fill(x, y, x+w, y+h, TB_BG);
    tb_hline(x, x+w, y, TB_BORDER);
    tb_vline(x, y, y+h, TB_BORDER);
    tb_vline(x+w, y, y+h, TB_BORDER);
    tb_hline(x, x+w, y+h, TB_BORDER);
    
    tb_draw_string(x + 5, y + 4, "[M] OPTIONS", TB_BORDER);
}

/* Draws a large pop-up menu in the center of the screen */
void draw_options_menu(void) {
    int x0 = 70, y0 = 50, x1 = 250, y1 = 190;
    
    /* Draw shadow/border and inner background */
    tb_fill(x0, y0, x1, y1, TB_BORDER);
    tb_fill(x0+2, y0+2, x1-2, y1-2, TB_BG);
    
    /* Menu Title */
    tb_draw_string(x0 + 55, y0 + 10, "OPTIONS MENU", TB_PAUSE_FILL);
    tb_hline(x0+10, x1-10, y0 + 22, TB_DIV);
    
    /* Dummy Options */
    tb_draw_string(x0 + 20, y0 + 40, "1 - TEMPO UP", TB_NOTE_TXT);
    tb_draw_string(x0 + 20, y0 + 60, "2 - TEMPO DOWN", TB_NOTE_TXT);
    tb_draw_string(x0 + 20, y0 + 80, "3 - CHANGE INSTRUMENT", TB_NOTE_TXT);
    
    /* Footer */
    tb_draw_string(x0 + 45, y1 - 20, "PRESS M TO CLOSE", TB_STOP_FILL);
}

/* =======================================================================
   Dynamic Tempo Badge
   ======================================================================= */

void toolbar_set_bpm(int bpm) {
    char str[4];
    
    /* Clamp the tempo FIRST so it never drops below 40 or above 999 */
    if (bpm > 999) bpm = 999;
    if (bpm < 40) bpm = 40;
    
    /* THEN save the safe, clamped number to the system state */
    toolbar_state.bpm = bpm;
    
    /* Convert integer to characters manually to save memory */
    str[0] = '0' + (bpm / 100);
    str[1] = '0' + ((bpm / 10) % 10);
    str[2] = '0' + (bpm % 10);
    str[3] = '\0';
    
    /* Drop the leading zero if the BPM is under 100 (e.g., ' 90') */
    char *display_str = str;
    if (str[0] == '0') {
        display_str = &str[1];
    }
    
    /* Redraw just the middle number badge */
    tb_badge(g_bpm_badge_x0, BADGE_W_TMP_VAL, display_str, COLOR_WHITE, COLOR_BLACK);
}