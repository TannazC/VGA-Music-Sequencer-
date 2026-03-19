#ifndef TREBLE_CLEF_BITMAP_H
#define TREBLE_CLEF_BITMAP_H

/*
 * Raw 1-bpp bitmap of the full sheet-music image.
 * Dimensions: 630 × 480 pixels.
 * Row stride : 79 bytes  (ceil(630/8) = 79, bit 7 of byte 0 = leftmost pixel).
 * Total size : 79 × 480 = 37 920 bytes.
 *
 * The treble-clef glyph sits at approximately:
 *   columns  38 .. 81   (44 px wide in source)
 *   rows     10 .. 102  (93 rows tall in source)
 */
#define BITMAP_BYTES_PER_ROW  79

extern const unsigned char sheet_music_bitmap[];

#endif /* TREBLE_CLEF_BITMAP_H */
