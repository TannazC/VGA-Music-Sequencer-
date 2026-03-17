# convert.py — Convert bg.png to a C header file containing a VGA-ready pixel array
#
# Responsibilities:
#   - Open bg.png using Pillow (pip install Pillow)
#   - Resize the image to 320x240 (VGA pixel buffer dimensions)
#   - Convert each pixel from RGB888 to RGB565:
#       r5 = (r >> 3) & 0x1F
#       g6 = (g >> 2) & 0x3F
#       b5 = (b >> 3) & 0x1F
#       rgb565 = (r5 << 11) | (g6 << 5) | b5
#   - Write output to bg_data.h in this format:
#       #ifndef BG_DATA_H
#       #define BG_DATA_H
#       #include <stdint.h>
#       #define BG_WIDTH  320
#       #define BG_HEIGHT 240
#       static const uint16_t bg_pixels[320 * 240] = { 0x..., 0x..., ... };
#       #endif
#   - Pixels should be written in row-major order (left-to-right, top-to-bottom)
#     matching the VGA pixel buffer row stride
#
# Usage:
#   python3 convert.py bg.png bg_data.h
#
# Notes:
#   - bg_data.h is auto-generated — add it to .gitignore or commit it as-is
#   - Choose a dark background image so the grid cells and text are readable
#   - If Pillow is unavailable on the DE1-SoC, run this script on your laptop
#     and copy bg_data.h into the project before compiling
