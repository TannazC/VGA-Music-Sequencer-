#ifndef SPRITES_H
#define SPRITES_H

/* ── Filled oval: tilted ~20 deg CCW (quarter / eighth / 16th heads) ── */
#define OVAL_W 11
#define OVAL_H 7

static const unsigned char FILLED_OVAL[7][11] = {
    {0,0,0,0,0,1,1,1,0,0,0},
    {0,0,0,1,1,1,1,1,1,1,0},
    {0,0,1,1,1,1,1,1,1,1,0},
    {0,0,1,1,1,1,1,1,1,1,0},
    {0,0,1,1,1,1,1,1,1,1,0},
    {0,0,0,1,1,1,1,1,1,0,0},
    {0,0,0,0,1,1,1,0,0,0,0},
};

/* ── Open oval: upright (whole / half heads) ── */
#define OPEN_OVAL_W 13
#define OPEN_OVAL_H 7

static const unsigned char OPEN_OVAL[7][13] = {
    {0,0,0,0,1,1,1,1,1,0,0,0,0},
    {0,0,0,1,1,1,1,1,1,1,0,0,0},
    {0,0,1,1,1,0,0,0,1,1,1,0,0},
    {0,1,1,1,0,0,0,0,0,1,1,1,0},
    {0,0,1,1,1,0,0,0,1,1,1,0,0},
    {0,0,0,1,1,1,1,1,1,1,0,0,0},
    {0,0,0,0,1,1,1,1,1,0,0,0,0},
};

#endif /* SPRITES_H */