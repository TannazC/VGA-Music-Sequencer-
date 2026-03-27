/* =======================================================================
   toolbar.c  --  Top keyboard-shortcut toolbar  (implementation)
   ======================================================================= */

#include "toolbar.h"
#include "background.h"    /* FB_WIDTH, FB_HEIGHT */

/* plot_pixel is defined in main.c */
extern void plot_pixel(int x, int y, short int c);

/* =======================================================================
   Global state  (extern in toolbar.h)
   ======================================================================= */
ToolbarState toolbar_state = {
    TB_WAVE_SQUARE,   /* waveform */
    120,              /* bpm      */
    0,                /* muted    */
    TB_STATE_STOPPED  /* playback */
};

/* =======================================================================
   Layout geometry
   ======================================================================= */
#define BADGE_AREA_H   26          
#define BADGE_H        20          
#define BADGE_Y0       (TOOLBAR_TOP + (BADGE_AREA_H - BADGE_H) / 2)   
#define BADGE_Y1       (BADGE_Y0 + BADGE_H - 1)                        
#define FONT_Y0        (BADGE_Y0 + (BADGE_H - 7) / 2)                  
 
/* Widths */
#define BADGE_W_TRANS  30   /* Uniform 30px width for ALL transport buttons */
#define BADGE_W1       11   
#define BADGE_W_TMP_BTN 14  /* Square +/- buttons */
#define BADGE_W_TMP_VAL 26  /* Wide enough to hold a 3-digit number */
#define BADGE_GAP       2   
#define GROUP_SEP       8
 
/* =======================================================================
   Custom Brand Palette (Converted to RGB 5-6-5)
   ======================================================================= */
#define RGB565(r, g, b)  ((short int)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)))

#define COLOR_SPEARMINT       RGB565(75, 145, 125) 
#define COLOR_NEON_SPEARMINT  RGB565(87, 200, 160) 
#define COLOR_FUCHSIA         RGB565(240, 55, 165) 
#define COLOR_CITRIC          RGB565(205, 245, 100) 
#define COLOR_MUTED_NEON_BLUE RGB565(69, 185, 220) 
#define COLOR_WHITE           RGB565(255, 255, 255) 
#define COLOR_BLACK           RGB565(0, 0, 0)

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
   5x7 pixel font
   ======================================================================= */
static const unsigned char *get_glyph(unsigned char c)
{
    static const unsigned char G_0[7]={0x0E,0x11,0x13,0x15,0x19,0x11,0x0E};
    static const unsigned char G_1[7]={0x04,0x0C,0x04,0x04,0x04,0x04,0x0E};
    static const unsigned char G_2[7]={0x0E,0x11,0x01,0x02,0x04,0x08,0x1F};
    static const unsigned char G_3[7]={0x0E,0x11,0x01,0x06,0x01,0x11,0x0E};
    static const unsigned char G_4[7]={0x02,0x06,0x0A,0x12,0x1F,0x02,0x02};
    static const unsigned char G_5[7]={0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E};
    static const unsigned char G_6[7]={0x0E,0x11,0x10,0x1E,0x11,0x11,0x0E};
    static const unsigned char G_7[7]={0x1F,0x01,0x02,0x04,0x08,0x10,0x10};
    static const unsigned char G_8[7]={0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E};
    static const unsigned char G_9[7]={0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E};
    static const unsigned char G_PLUS[7]={0x00,0x04,0x04,0x1F,0x04,0x04,0x00};
    static const unsigned char G_MINUS[7]={0x00,0x00,0x00,0x1F,0x00,0x00,0x00};
    
    static const unsigned char G_A[7]={0x0E,0x11,0x11,0x1F,0x11,0x11,0x11};
    static const unsigned char G_B[7]={0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E};
    static const unsigned char G_C[7]={0x0E,0x11,0x10,0x10,0x10,0x11,0x0E};
    static const unsigned char G_D[7]={0x1E,0x11,0x11,0x11,0x11,0x11,0x1E};
    static const unsigned char G_E[7]={0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F};
    static const unsigned char G_K[7]={0x11,0x12,0x14,0x18,0x14,0x12,0x11};
    static const unsigned char G_L[7]={0x10,0x10,0x10,0x10,0x10,0x10,0x1F};
    static const unsigned char G_P[7]={0x1E,0x11,0x11,0x1E,0x10,0x10,0x10};
    static const unsigned char G_Q[7]={0x0E,0x11,0x11,0x11,0x15,0x13,0x0F};
    static const unsigned char G_R[7]={0x1E,0x11,0x11,0x1E,0x14,0x12,0x11};
    static const unsigned char G_S[7]={0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E};
    static const unsigned char G_T[7]={0x1F,0x04,0x04,0x04,0x04,0x04,0x04};
    static const unsigned char G_G[7]={0x0E,0x11,0x10,0x17,0x11,0x11,0x0E};
    static const unsigned char G_I[7]={0x0E,0x04,0x04,0x04,0x04,0x04,0x0E};
    static const unsigned char G_M[7]={0x11,0x1B,0x15,0x11,0x11,0x11,0x11};
    static const unsigned char G_N[7]={0x11,0x19,0x15,0x13,0x11,0x11,0x11};
    static const unsigned char G_O[7]={0x0E,0x11,0x11,0x11,0x11,0x11,0x0E};
    static const unsigned char G_U[7]={0x11,0x11,0x11,0x11,0x11,0x11,0x0E};
    static const unsigned char G_H[7]={0x11,0x11,0x11,0x1F,0x11,0x11,0x11};
    static const unsigned char G_W[7]={0x11,0x11,0x11,0x11,0x15,0x15,0x0A};
    static const unsigned char G_DASH[7]={0x00,0x00,0x00,0x0E,0x00,0x00,0x00};
    static const unsigned char G_LBRACKET[7]={0x0E,0x08,0x08,0x08,0x08,0x08,0x0E};
    static const unsigned char G_RBRACKET[7]={0x0E,0x02,0x02,0x02,0x02,0x02,0x0E};
 
    switch(c){
        case '1': return G_1; case '2': return G_2; case '3': return G_3;
        case '4': return G_4; case '5': return G_5; case '6': return G_6;
        case '7': return G_7;
        case 'A': return G_A; case 'B': return G_B; case 'C': return G_C;
        case 'D': return G_D; case 'E': return G_E; case 'K': return G_K; 
        case 'L': return G_L; case 'P': return G_P; case 'Q': return G_Q; 
        case 'R': return G_R; case 'S': return G_S; case 'T': return G_T;
        case 'G': return G_G; case 'I': return G_I; case 'M': return G_M;
        case 'N': return G_N; case 'O': return G_O; case 'U': return G_U;
        case '[': return G_LBRACKET; case ']': return G_RBRACKET;
        case 'H': return G_H; case 'W': return G_W; case '-': return G_DASH;
        case '0': return G_0; case '8': return G_8; case '9': return G_9;
        case '+': return G_PLUS; 
        default:  return 0;
    }
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


/* =======================================================================
   Low-level pixel helpers
   ======================================================================= */
static void tb_fill(int x0,int y0,int x1,int y1,short int c) {
    int x,y; for(y=y0;y<=y1;y++) for(x=x0;x<=x1;x++) plot_pixel(x,y,c);
}
static void tb_hline(int x0,int x1,int y,short int c) { int x; for(x=x0;x<=x1;x++) plot_pixel(x,y,c); }
static void tb_vline(int x,int y0,int y1,short int c) { int y; for(y=y0;y<=y1;y++) plot_pixel(x,y,c); }
 
static void tb_draw_char(int x,int y,unsigned char c,short int col) {
    int row,bit; const unsigned char *g=get_glyph(c); if(!g) return;
    for(row=0;row<7;row++){
        unsigned char bits=g[row];
        for(bit=4;bit>=0;bit--) if(bits&(1<<bit)) plot_pixel(x+(4-bit),y+row,col);
    }
}

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
 
static int tb_badge(int x,int bw,const char *label, short int fill,short int txt) {
    int llen=0,text_w,tx; const char *p;
    tb_fill(x+1,BADGE_Y0+1,x+bw-2,BADGE_Y1-1,fill);
    tb_hline(x,x+bw-1,BADGE_Y0,TB_BORDER); tb_hline(x,x+bw-1,BADGE_Y1,TB_BORDER);
    tb_vline(x,BADGE_Y0,BADGE_Y1,TB_BORDER); tb_vline(x+bw-1,BADGE_Y0,BADGE_Y1,TB_BORDER);
    for(p=label;*p;p++) llen++; text_w = llen*6-1; tx = x+(bw-text_w)/2;
    for(p=label;*p;p++,tx+=6) tb_draw_char(tx,FONT_Y0,(unsigned char)*p,txt);
    return x+bw;
}
 
/* Single, unified transport badge for ALL 4 buttons */
static int tb_transport_badge(int x, const unsigned char icon[12][12], char key_char, 
                              short int fill, short int icon_col, short int key_col) {
    int bw = BADGE_W_TRANS; 
    int icon_x = x + 3; 
    int icon_y = BADGE_Y0 + (BADGE_H - 12) / 2; /* Centers the 12x12 icon perfectly */
    int div_x  = x + 18; 
    int key_x  = x + 21;

    tb_fill(x+1, BADGE_Y0+1, x+bw-2, BADGE_Y1-1, fill);
    tb_hline(x, x+bw-1, BADGE_Y0, TB_BORDER); 
    tb_hline(x, x+bw-1, BADGE_Y1, TB_BORDER);
    tb_vline(x, BADGE_Y0, BADGE_Y1, TB_BORDER); 
    tb_vline(x+bw-1, BADGE_Y0, BADGE_Y1, TB_BORDER);
    
    tb_draw_icon_12(icon_x, icon_y, icon, icon_col); 
    tb_vline(div_x, BADGE_Y0+2, BADGE_Y1-2, TB_DIV);
    tb_draw_char(key_x, FONT_Y0, (unsigned char)key_char, key_col);
    
    return x + bw;
}

static void tb_group_div(int x) { tb_vline(x,TOOLBAR_TOP+2,BADGE_Y1,TB_DIV); }
 
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
    for(p = str; *p; p++, tx += 6) {
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