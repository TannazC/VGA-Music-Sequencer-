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
import sys
from PIL import Image

def generate_exact_clef(input_path, output_path):
    try:
        # Open and convert to grayscale
        img = Image.open(input_path).convert('L')
        
        # 1. Auto-crop to the clef boundaries
        bbox = img.getbbox()
        if bbox:
            img = img.crop(bbox)
        
        # 2. Resize to EXACTLY the dimensions used in your C code
        # This prevents the "chopped" look caused by HPS integer division
        target_w, target_h = 16, 37
        img = img.resize((target_w, target_h), Image.LANCZOS)
        
        pixels = img.load()
        
        with open(output_path, 'w') as f:
            f.write("#ifndef TREBLE_DATA_H\n#define TREBLE_DATA_H\n\n")
            f.write("#include <stdint.h>\n\n")
            f.write(f"#define TREBLE_WIDTH  {target_w}\n")
            f.write(f"#define TREBLE_HEIGHT {target_h}\n\n")
            f.write(f"static const uint16_t treble_pixels[{target_w} * {target_h}] = {{\n")
            
            for y in range(target_h):
                row_data = []
                for x in range(target_w):
                    # Threshold: if dark (clef), make it 0xFFFF (White in source)
                    # We use white here so your C logic 'if (px == 0xFFFF)' works
                    val = 0xFFFF if pixels[x, y] < 128 else 0x0000
                    row_data.append(f"0x{val:04X}")
                f.write("    " + ", ".join(row_data) + ",\n")
            
            f.write("};\n\n#endif\n")
            
        print(f"Success: Generated {target_w}x{target_h} bitmap in {output_path}")

    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python convert.py <input> <output>")
    else:
        generate_exact_clef(sys.argv[1], sys.argv[2])