#include "vga_music.h"
#include "charbuf.h"

void char_buf_put(int cx, int cy, char ch)
{
    volatile char *char_buf = (volatile char *)CHAR_BUF_BASE;
    *(char_buf + (cy << 7) + cx) = ch;   /* stride = 128 = 2^7 */
}

void char_buf_puts(int cx, int cy, const char *s)
{
    volatile char *char_buf = (volatile char *)CHAR_BUF_BASE;
    int offset = (cy << 7) + cx;
    while (*s) {
        *(char_buf + offset) = *s;
        ++s;
        ++offset;
    }
}

void char_buf_clear(void)
{
    volatile char *char_buf = (volatile char *)CHAR_BUF_BASE;
    int row, col;
    for (row = 0; row < CHAR_BUF_ROWS; row++)
        for (col = 0; col < CHAR_BUF_COLS; col++)
            *(char_buf + (row << 7) + col) = ' ';
}
