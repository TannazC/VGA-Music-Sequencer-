#ifndef CHARBUF_H
#define CHARBUF_H

/* Write one character at character-grid position (cx, cy). */
void char_buf_put(int cx, int cy, char ch);

/* Write a null-terminated string starting at (cx, cy). */
void char_buf_puts(int cx, int cy, const char *s);

/* Fill the entire character buffer with spaces. */
void char_buf_clear(void);

#endif /* CHARBUF_H */
