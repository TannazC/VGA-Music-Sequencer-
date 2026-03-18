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

def generate_treble_bitmap(input_path, output_path, threshold=128):
    try:
        # 1. Open and ensure image is in Grayscale ('L' mode)
        # This simplifies thresholding.
        img = Image.open(input_path).convert('L')
        
        # 2. Basic automatic cropping to remove the black bars.
        # This works if the bars are perfectly dark.
        bbox = img.getbbox() # Returns (left, top, right, bottom)
        if bbox:
            # Add a small padding (5 pixels) so the symbol isn't right on the edge
            padding = 5
            bbox_with_padding = (
                max(0, bbox[0] - padding),
                max(0, bbox[1] - padding),
                min(img.width, bbox[2] + padding),
                min(img.height, bbox[3] + padding)
            )
            img = img.crop(bbox_with_padding)
        
        # 3. Resize to VGA dimensions (maintaining the aspect ratio)
        # If we resize the cropped image to 320x240, we might stretch the clef.
        # Let's resize it so the HEIGHT is 240, and the WIDTH is scaled.
        original_width, original_height = img.size
        new_height = 240
        new_width = int(original_width * (new_height / original_height))
        
        # Cap the new width if it exceeds 320
        if new_width > 320:
            new_width = 320
            new_height = int(original_height * (new_width / original_width))
        
        img = img.resize((new_width, new_height), Image.NEAREST)
        # Re-capture dimensions for the C header
        actual_width, actual_height = img.size
        
        pixels = img.load()
        
        with open(output_path, 'w') as f:
            # Write Header Guard (Updated Name)
            f.write("#ifndef TREBLE_DATA_H\n")
            f.write("#define TREBLE_DATA_H\n\n")
            f.write("#include <stdint.h>\n\n")
            f.write(f"#define TREBLE_WIDTH  {actual_width}\n")
            f.write(f"#define TREBLE_HEIGHT {actual_height}\n\n")
            # Size must match the actual width/height
            f.write(f"static const uint16_t treble_pixels[{actual_width} * {actual_height}] = {{\n")
            
            hex_data = []
            for y in range(actual_height):
                for x in range(actual_width):
                    # Get grayscale value (0-255)
                    gray_val = pixels[x, y]
                    
                    # 4. Binary Thresholding
                    # If pixel is dark, make it full bright white (RGB565 0xFFFF)
                    # If pixel is light, make it full black (RGB565 0x0000)
                    if gray_val < threshold:
                        rgb565 = 0xFFFF # WHITE
                    else:
                        rgb565 = 0x0000 # BLACK
                    
                    hex_data.append(f"0x{rgb565:04X}")
                
                # Write one row at a time
                f.write("    " + ", ".join(hex_data) + ",\n")
                hex_data = []

            f.write("};\n\n")
            f.write("#endif\n")
            
        print(f"Success: {output_path} (isolated treble bitmap) generated from {input_path}")

    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python3 convert.py <input_image> <output_header>")
    else:
        # Run with default threshold of 128
        generate_treble_bitmap(sys.argv[1], sys.argv[2], threshold=128)