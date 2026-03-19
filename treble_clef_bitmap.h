#ifndef TREBLE_CLEF_BITMAP_H
#define TREBLE_CLEF_BITMAP_H

/* ═══════════════════════════════════════════════════════════════════════
   Treble clef bitmap  —  12 columns × 36 rows
   ───────────────────────────────────────────────────────────────────────
   Derived from uploaded treble clef image, scaled to fit the staff layout:
     STAFF_SPACING = 6 px, LINES_PER_STAFF = 5
     Glyph spans: 1 space above top line → 1 space below bottom line
                  = 6 + (4×6) + 6 = 36 px total height

   Each entry is one row. Bit 11 = column 0 (leftmost), bit 0 = column 11.
   # = black pixel drawn,  . = transparent (background shows through)
   ═══════════════════════════════════════════════════════════════════════ */

#define CLEF_BMP_W  12
#define CLEF_BMP_H  36

static const unsigned short treble_clef_bmp[CLEF_BMP_H] = {
    /* row  0 */  0x038,  /* ......###... */
    /* row  1 */  0x078,  /* .....####... */
    /* row  2 */  0x06C,  /* .....##.##.. */
    /* row  3 */  0x044,  /* .....#...#.. */
    /* row  4 */  0x044,  /* .....#...#.. */
    /* row  5 */  0x0C4,  /* ....##...#.. */
    /* row  6 */  0x0CC,  /* ....##..##.. */
    /* row  7 */  0x08C,  /* ....#...##.. */
    /* row  8 */  0x09C,  /* ....#..###.. */
    /* row  9 */  0x058,  /* .....#.##... */
    /* row 10 */  0x070,  /* .....###.... */
    /* row 11 */  0x070,  /* .....###.... */
    /* row 12 */  0x0E0,  /* ....###..... */
    /* row 13 */  0x1E0,  /* ...####..... */
    /* row 14 */  0x3C0,  /* ..####...... */
    /* row 15 */  0x3E0,  /* ..#####..... */
    /* row 16 */  0x720,  /* .###..#..... */
    /* row 17 */  0xE38,  /* ###...###... */
    /* row 18 */  0xE7C,  /* ###..#####.. */
    /* row 19 */  0xCFE,  /* ##..#######. */
    /* row 20 */  0xCF7,  /* ##..####.### */
    /* row 21 */  0xC93,  /* ##..#..#..## */
    /* row 22 */  0xC93,  /* ##..#..#..## */
    /* row 23 */  0x493,  /* .#..#..#..## */
    /* row 24 */  0x413,  /* .#.....#..## */
    /* row 25 */  0x202,  /* ..#.......#. */
    /* row 26 */  0x18C,  /* ...##...##.. */
    /* row 27 */  0x078,  /* .....####... */
    /* row 28 */  0x008,  /* ........#... */
    /* row 29 */  0x000,  /* .........#.. */
    /* row 30 */  0x0C4,  /* ....##...#.. */
    /* row 31 */  0x1E4,  /* ...####..#.. */
    /* row 32 */  0x1E4,  /* ...####..#.. */
    /* row 33 */  0x1E4,  /* ...####..#.. */
    /* row 34 */  0x1C8,  /* ...###..#... */
    /* row 35 */  0x0F0,  /* ....####.... */
};

#endif /* TREBLE_CLEF_BITMAP_H */