TARGET  = vga_music.elf
CC      = arm-linux-gnueabihf-gcc   # adjust for your toolchain
CFLAGS  = -O2 -Wall -Wextra -ffreestanding -nostdlib

OBJS =  main.o          \
        graphics.o      \
        charbuf.o       \
        background.o    \
        treble_clef_bitmap.o \
        cursor.o        \
        ps2.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Header dependencies
main.o:               main.c vga_music.h graphics.h background.h cursor.h ps2.h
graphics.o:           graphics.c graphics.h vga_music.h
charbuf.o:            charbuf.c charbuf.h vga_music.h
background.o:         background.c background.h vga_music.h graphics.h charbuf.h treble_clef_bitmap.h
treble_clef_bitmap.o: treble_clef_bitmap.c treble_clef_bitmap.h
cursor.o:             cursor.c cursor.h vga_music.h graphics.h background.h
ps2.o:                ps2.c ps2.h vga_music.h

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: clean
