/* ═══════════════════════════════════════════════════════════════════════
   toolbar.c  —  Top keyboard-shortcut toolbar  (implementation)
   ═══════════════════════════════════════════════════════════════════════
   Dependencies (all defined in main.c / background.h):
       void  plot_pixel(int x, int y, short int c);
       int   FB_WIDTH, FB_HEIGHT                    (from background.h)
   No other external state is read or written.
   ═══════════════════════════════════════════════════════════════════════ */

#include "toolbar.h"
#include "background.h"    /* FB_WIDTH, FB_HEIGHT */

/* plot_pixel is defined in main.c */
extern void plot_pixel(int x, int y, short int c);

/* ═══════════════════════════════════════════════════════════════════════
   Global state  (extern in toolbar.h)
   ═══════════════════════════════════════════════════════════════════════ */
ToolbarState toolbar_state = {
    TB_WAVE_SQUARE,   /* waveform */
    120,              /* bpm      */
    0,                /* muted    */
    TB_STATE_STOPPED  /* playback */
};


/* ═══════════════════════════════════════════════════════════════════════
   Layout geometry
   ──────────────────────────────────────────────────────────────────────
   The toolbar occupies rows 0-25 (26 px).
   Top 20 px = badge row.  Bottom 6 px = progress bar + legend line.
   ═══════════════════════════════════════════════════════════════════════ */
#define BADGE_AREA_H   20          /* px for the badge row              */
#define BADGE_H        14          /* badge box height                  */
/* Centre badge vertically within the 20-px badge area */
#define BADGE_Y0       (TOOLBAR_TOP + (BADGE_AREA_H - BADGE_H) / 2)   /* 3  */
#define BADGE_Y1       (BADGE_Y0 + BADGE_H - 1)                        /* 16 */
/* 5x7 font: centre vertically inside badge */
#define FONT_Y0        (BADGE_Y0 + (BADGE_H - 7) / 2)                  /* 6  */
 
/* Widths */
#define BADGE_W_TRANS  24   /* transport:  icon(7) + div(1) + key(5) + margins */
#define BADGE_W1       14   /* note-type:  single digit                        */
#define BADGE_W_ACT    20   /* action:     SPC / DEL  (2-3 char)               */
#define BADGE_GAP       2   /* gap between badges in same group                */
#define GROUP_SEP       8   /* gap + divider between groups                    */
 
/* Progress bar: 4 rows at the very bottom of the toolbar */
#define PROG_Y0        (TOOLBAR_BOT - 3)
#define PROG_Y1        (TOOLBAR_BOT)
#define PROG_X0         4
#define PROG_X1        (FB_WIDTH - 5)
#define PROG_STEPS     14   /* TOTAL_COLS - FIRST_COL = 16-2 */
 
/* ═══════════════════════════════════════════════════════════════════════
   Colour palette  (RGB 5-6-5)
   ═══════════════════════════════════════════════════════════════════════ */
#define TB_BG           ((short int)0xDEFB)  /* strip background  RGB(220,220,220) */
#define TB_BORDER       ((short int)0x4208)  /* badge border      RGB( 64, 64, 64) */
#define TB_DIV          ((short int)0x8410)  /* divider / rule    RGB(128,128,128) */
 
/* Transport — Play: green tint */
#define TB_PLAY_FILL    ((short int)0x2D06)  /* RGB( 40,168, 48) dark green      */
#define TB_PLAY_FILL_A  ((short int)0x07C0)  /* RGB(  0,248,  0) active green    */
#define TB_PLAY_ICO     ((short int)0xFFFF)  /* white icon                       */
#define TB_PLAY_KEY     ((short int)0xFFFF)
 
/* Transport — Pause: amber */
#define TB_PAUSE_FILL   ((short int)0x7320)  /* RGB(112,100,  0) dark amber      */
#define TB_PAUSE_FILL_A ((short int)0xFEA0)  /* RGB(255,212,  0) active gold     */
#define TB_PAUSE_ICO    ((short int)0xFFFF)
#define TB_PAUSE_KEY    ((short int)0xFFFF)
 
/* Transport — Stop: red */
#define TB_STOP_FILL    ((short int)0x9000)  /* RGB(144,  0,  0) dark red        */
#define TB_STOP_FILL_A  ((short int)0xF800)  /* RGB(248,  0,  0) active red      */
#define TB_STOP_ICO     ((short int)0xFFFF)
#define TB_STOP_KEY     ((short int)0xFFFF)
 
/* Transport — Restart: blue */
#define TB_REST_FILL    ((short int)0x0018)  /* RGB(  0,  0,192) dark blue       */
#define TB_REST_FILL_A  ((short int)0x051F)  /* RGB(  0,163,248) active blue     */
#define TB_REST_ICO     ((short int)0xFFFF)
#define TB_REST_KEY     ((short int)0xFFFF)
 
/* Note-type badges: inactive periwinkle / active blue */
#define TB_NOTE_FILL    ((short int)0xB63B)
#define TB_NOTE_TXT     ((short int)0x0000)
#define TB_NOTEA_FILL   ((short int)0x1C9F)
#define TB_NOTEA_TXT    ((short int)0xFFFF)
 
/* Action badges: Place=green, Delete=red */
#define TB_SPC_FILL     ((short int)0x2D26)  /* soft green   */
#define TB_SPC_TXT      ((short int)0xFFFF)
#define TB_DEL_FILL     ((short int)0x9000)
#define TB_DEL_TXT      ((short int)0xFFFF)
 
/* Progress bar */
#define PROG_TRACK      ((short int)0xC618)  /* empty track — mid grey */
#define PROG_FILL       ((short int)0x07E0)  /* filled — bright green  */

/* ═══════════════════════════════════════════════════════════════════════
   5×7 pixel font
   Each glyph: 7 bytes, bit4=leftmost, bit0=rightmost.
   ═══════════════════════════════════════════════════════════════════════ */
static const unsigned char *get_glyph(unsigned char c)
{
    static const unsigned char G_1[7]={0x04,0x0C,0x04,0x04,0x04,0x04,0x0E};
    static const unsigned char G_2[7]={0x0E,0x11,0x01,0x02,0x04,0x08,0x1F};
    static const unsigned char G_3[7]={0x0E,0x11,0x01,0x06,0x01,0x11,0x0E};
    static const unsigned char G_4[7]={0x02,0x06,0x0A,0x12,0x1F,0x02,0x02};
    static const unsigned char G_5[7]={0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E};
    static const unsigned char G_A[7]={0x0E,0x11,0x11,0x1F,0x11,0x11,0x11};
    static const unsigned char G_D[7]={0x1E,0x11,0x11,0x11,0x11,0x11,0x1E};
    static const unsigned char G_E[7]={0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F};
    static const unsigned char G_L[7]={0x10,0x10,0x10,0x10,0x10,0x10,0x1F};
    static const unsigned char G_P[7]={0x1E,0x11,0x11,0x1E,0x10,0x10,0x10};
    static const unsigned char G_Q[7]={0x0E,0x11,0x11,0x11,0x15,0x13,0x0F};
    static const unsigned char G_R[7]={0x1E,0x11,0x11,0x1E,0x14,0x12,0x11};
    static const unsigned char G_S[7]={0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E};
    static const unsigned char G_T[7]={0x1F,0x04,0x04,0x04,0x04,0x04,0x04};
 
    switch(c){
        case '1': return G_1; case '2': return G_2; case '3': return G_3;
        case '4': return G_4; case '5': return G_5;
        case 'A': return G_A; case 'D': return G_D; case 'E': return G_E;
        case 'L': return G_L; case 'P': return G_P; case 'Q': return G_Q;
        case 'R': return G_R; case 'S': return G_S; case 'T': return G_T;
        default:  return 0;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   5×5 transport icon bitmaps
   Bit 4 = leftmost pixel, bit 0 = rightmost.  5 rows tall.
   ═══════════════════════════════════════════════════════════════════════ */
 
/* ▶  Play triangle — right-pointing */
static const unsigned char ICON_PLAY[5]  = {
    0x10,  /* #....  */
    0x18,  /* ##...  */
    0x1C,  /* ###..  */
    0x18,  /* ##...  */
    0x10   /* #....  */
};
 
/* ⏸  Pause — two vertical bars */
static const unsigned char ICON_PAUSE[5] = {
    0x1B,  /* ##.##  */
    0x1B,  /* ##.##  */
    0x1B,  /* ##.##  */
    0x1B,  /* ##.##  */
    0x1B   /* ##.##  */
};
 
/* ■  Stop — filled square */
static const unsigned char ICON_STOP[5]  = {
    0x1F,  /* #####  */
    0x1F,  /* #####  */
    0x1F,  /* #####  */
    0x1F,  /* #####  */
    0x1F   /* #####  */
};
 
/* ↺  Restart — arc with a left arrow on top */
static const unsigned char ICON_RESTART[5] = {
    0x1C,  /* ###..  */
    0x10,  /* #....  */
    0x17,  /* #.###  */
    0x01,  /* ....#  */
    0x0E   /* .###.  */
};
/* ═══════════════════════════════════════════════════════════════════════
   Low-level pixel helpers
   ═══════════════════════════════════════════════════════════════════════ */
static void tb_fill(int x0,int y0,int x1,int y1,short int c)
{
    int x,y;
    for(y=y0;y<=y1;y++) for(x=x0;x<=x1;x++) plot_pixel(x,y,c);
}
static void tb_hline(int x0,int x1,int y,short int c)
{ int x; for(x=x0;x<=x1;x++) plot_pixel(x,y,c); }
static void tb_vline(int x,int y0,int y1,short int c)
{ int y; for(y=y0;y<=y1;y++) plot_pixel(x,y,c); }
 
/* Draw one 5×7 character glyph, top-left at (x,y) */
static void tb_draw_char(int x,int y,unsigned char c,short int col)
{
    int row,bit;
    const unsigned char *g=get_glyph(c);
    if(!g) return;
    for(row=0;row<7;row++){
        unsigned char bits=g[row];
        for(bit=4;bit>=0;bit--)
            if(bits&(1<<bit)) plot_pixel(x+(4-bit),y+row,col);
    }
}
 
/* Draw a 5×5 transport icon, top-left at (x,y) */
static void tb_draw_icon(int x,int y,const unsigned char *icon,short int col)
{
    int row,bit;
    for(row=0;row<5;row++){
        unsigned char bits=icon[row];
        for(bit=4;bit>=0;bit--)
            if(bits&(1<<bit)) plot_pixel(x+(4-bit),y+row,col);
    }
}
 
/* Standard badge: filled box with border and centred text label.
   Returns x just past the right edge.                                  */
static int tb_badge(int x,int bw,const char *label,
                     short int fill,short int txt)
{
    int llen=0,text_w,tx;
    const char *p;
 
    tb_fill(x+1,BADGE_Y0+1,x+bw-2,BADGE_Y1-1,fill);
    tb_hline(x,x+bw-1,BADGE_Y0,TB_BORDER);
    tb_hline(x,x+bw-1,BADGE_Y1,TB_BORDER);
    tb_vline(x,BADGE_Y0,BADGE_Y1,TB_BORDER);
    tb_vline(x+bw-1,BADGE_Y0,BADGE_Y1,TB_BORDER);
 
    for(p=label;*p;p++) llen++;
    text_w = llen*6-1;
    tx = x+(bw-text_w)/2;
    for(p=label;*p;p++,tx+=6) tb_draw_char(tx,FONT_Y0,(unsigned char)*p,txt);
 
    return x+bw;
}
 
/* Transport badge: [ICON | thin line | KEY CHAR]
   icon       : 5×5 bitmap
   key_char   : single uppercase letter
   fill       : background colour (different when active)
   Returns x just past right edge.                                       */
static int tb_transport_badge(int x,
                               const unsigned char *icon,
                               char key_char,
                               short int fill,
                               short int icon_col,
                               short int key_col)
{
    int bw = BADGE_W_TRANS;
    int icon_x = x + 2;
    int icon_y = BADGE_Y0 + (BADGE_H - 5) / 2;   /* vertically centre 5px icon */
    int div_x  = x + 9;
    int key_x  = x + 12;
 
    /* Background + border */
    tb_fill(x+1,BADGE_Y0+1,x+bw-2,BADGE_Y1-1,fill);
    tb_hline(x,x+bw-1,BADGE_Y0,TB_BORDER);
    tb_hline(x,x+bw-1,BADGE_Y1,TB_BORDER);
    tb_vline(x,BADGE_Y0,BADGE_Y1,TB_BORDER);
    tb_vline(x+bw-1,BADGE_Y0,BADGE_Y1,TB_BORDER);
 
    /* Icon */
    tb_draw_icon(icon_x,icon_y,icon,icon_col);
 
    /* Thin vertical divider between icon and key */
    tb_vline(div_x,BADGE_Y0+2,BADGE_Y1-2,TB_DIV);
 
    /* Key character */
    tb_draw_char(key_x,FONT_Y0,(unsigned char)key_char,key_col);
 
    return x+bw;
}
 
static void tb_group_div(int x)
{ tb_vline(x,TOOLBAR_TOP+2,BADGE_Y1,TB_DIV); }
 
/* ═══════════════════════════════════════════════════════════════════════
   Internal state saved for partial redraws
   ═══════════════════════════════════════════════════════════════════════ */
static int g_note_badge_x0 = 0;   /* x of first note-type badge         */
static int g_trans_x0      = 0;   /* x of first transport badge         */
 
static int note_badge_x(int i)
{ return g_note_badge_x0 + i*(BADGE_W1+BADGE_GAP); }
 
/* ═══════════════════════════════════════════════════════════════════════
   draw_toolbar  — full first-time draw
   ═══════════════════════════════════════════════════════════════════════ */
void draw_toolbar(int cur_note_type)
{
    int x,i;
    char label[2];
    short int fill,txt;
    int is_playing = (toolbar_state.playback == TB_STATE_PLAYING);
    int is_paused  = (toolbar_state.playback == TB_STATE_PAUSED);
 
    /* Full background strip */
    tb_fill(0,TOOLBAR_TOP,FB_WIDTH-1,TOOLBAR_BOT,TB_BG);
    /* Thin rule under badge row */
    tb_hline(0,FB_WIDTH-1,BADGE_AREA_H,TB_DIV);
 
    x = 4;
 
    /* ── Group 1: Transport ──────────────────────────────────────────
       ▶ Q  ⏸ E  ■ T  ↺ R
       Active transport button gets the bright fill colour.            */
    g_trans_x0 = x;
 
    /* Play */
    fill = is_playing ? TB_PLAY_FILL_A : TB_PLAY_FILL;
    x = tb_transport_badge(x,ICON_PLAY,'Q',fill,TB_PLAY_ICO,TB_PLAY_KEY)+BADGE_GAP;
 
    /* Pause */
    fill = is_paused ? TB_PAUSE_FILL_A : TB_PAUSE_FILL;
    x = tb_transport_badge(x,ICON_PAUSE,'E',fill,TB_PAUSE_ICO,TB_PAUSE_KEY)+BADGE_GAP;
 
    /* Stop */
    fill = TB_STOP_FILL;
    x = tb_transport_badge(x,ICON_STOP,'T',fill,TB_STOP_ICO,TB_STOP_KEY)+BADGE_GAP;
 
    /* Restart */
    fill = TB_REST_FILL;
    x = tb_transport_badge(x,ICON_RESTART,'R',fill,TB_REST_ICO,TB_REST_KEY);
 
    /* Group divider */
    x += GROUP_SEP/2; tb_group_div(x); x += GROUP_SEP/2;
 
    /* ── Group 2: Note type  1-5 ─────────────────────────────────────
       1=Whole  2=Half  3=Quarter  4=8th  5=16th                       */
    g_note_badge_x0 = x;
    label[1] = '\0';
    for(i=0;i<TB_NUM_NOTE_TYPES;i++){
        label[0]=(char)('1'+i);
        fill = (i==cur_note_type) ? TB_NOTEA_FILL : TB_NOTE_FILL;
        txt  = (i==cur_note_type) ? TB_NOTEA_TXT  : TB_NOTE_TXT;
        x = tb_badge(x,BADGE_W1,label,fill,txt)+BADGE_GAP;
    }
    x -= BADGE_GAP;
 
    /* Group divider */
    x += GROUP_SEP/2; tb_group_div(x); x += GROUP_SEP/2;
 
    /* ── Group 3: Actions ────────────────────────────────────────────
       [+ SPC]  [✕ DEL]  — icon drawn as part of label below          */
    x = tb_badge(x,BADGE_W_ACT,"SPC",TB_SPC_FILL,TB_SPC_TXT)+BADGE_GAP;
    tb_badge(x,BADGE_W_ACT,"DEL",TB_DEL_FILL,TB_DEL_TXT);
 
    /* ── Progress bar: draw empty track ─────────────────────────────── */
    tb_fill(PROG_X0,PROG_Y0,PROG_X1,PROG_Y1,PROG_TRACK);
}
 
/* ═══════════════════════════════════════════════════════════════════════
   toolbar_set_note_type  — redraw only the 5 note badges
   ═══════════════════════════════════════════════════════════════════════ */
void toolbar_set_note_type(int cur_note_type)
{
    int i;
    char label[2];
    short int fill,txt;
 
    label[1]='\0';
    for(i=0;i<TB_NUM_NOTE_TYPES;i++){
        label[0]=(char)('1'+i);
        fill = (i==cur_note_type) ? TB_NOTEA_FILL : TB_NOTE_FILL;
        txt  = (i==cur_note_type) ? TB_NOTEA_TXT  : TB_NOTE_TXT;
        tb_badge(note_badge_x(i),BADGE_W1,label,fill,txt);
    }
}
 
/* ═══════════════════════════════════════════════════════════════════════
   toolbar_set_playback  — redraw only the 4 transport badges
   ═══════════════════════════════════════════════════════════════════════ */
void toolbar_set_playback(int state)
{
    int x = g_trans_x0;
    short int fill;
    int is_playing = (state == TB_STATE_PLAYING);
    int is_paused  = (state == TB_STATE_PAUSED);
 
    toolbar_state.playback = state;
 
    fill = is_playing ? TB_PLAY_FILL_A : TB_PLAY_FILL;
    x = tb_transport_badge(x,ICON_PLAY,'Q',fill,TB_PLAY_ICO,TB_PLAY_KEY)+BADGE_GAP;
 
    fill = is_paused ? TB_PAUSE_FILL_A : TB_PAUSE_FILL;
    x = tb_transport_badge(x,ICON_PAUSE,'E',fill,TB_PAUSE_ICO,TB_PAUSE_KEY)+BADGE_GAP;
 
    fill = TB_STOP_FILL;
    x = tb_transport_badge(x,ICON_STOP,'T',fill,TB_STOP_ICO,TB_STOP_KEY)+BADGE_GAP;
 
    fill = TB_REST_FILL;
    tb_transport_badge(x,ICON_RESTART,'R',fill,TB_REST_ICO,TB_REST_KEY);
}
 
/* ═══════════════════════════════════════════════════════════════════════
   toolbar_update_step  — advance progress bar during playback
   step = 0 .. PROG_STEPS-1
   ═══════════════════════════════════════════════════════════════════════ */
void toolbar_update_step(int step)
{
    int bar_w   = PROG_X1 - PROG_X0 + 1;
    int step_px = bar_w / PROG_STEPS;
    int fill_x, x;
 
    /* On first step wipe the bar clean */
    if(step == 0)
        tb_fill(PROG_X0,PROG_Y0,PROG_X1,PROG_Y1,PROG_TRACK);
 
    fill_x = PROG_X0 + (step+1)*step_px - 1;
    if(fill_x > PROG_X1) fill_x = PROG_X1;
 
    for(x=PROG_X0; x<=fill_x; x++){
        plot_pixel(x,PROG_Y0,PROG_FILL);
        plot_pixel(x,PROG_Y0+1,PROG_FILL);
        plot_pixel(x,PROG_Y0+2,PROG_FILL);
        plot_pixel(x,PROG_Y1,PROG_FILL);
    }
}