# python ttf_to_atlas.py upheavtt.ttf 12 fat_font FAT_FONT fat_font.h 
# python ttf_to_atlas.py m5x7.ttf 7 skinny_font SKINNY_FONT skinny_font.h

""" =====================================================================
    font_to_bitmap.py -- Renders text using a TTF font directly to a C array
    Optimized for NIOS V (RISC-V) and VGA buffers.
    ===================================================================== """

import os
import argparse
import sys
from PIL import Image, ImageDraw, ImageFont

def generate_text_header(text, font_path, font_size, array_name, width, height):
    # 1. Create a pure black canvas (1-bit pixels)
    # '1' mode means 1-bit pixels. 0 is black (background), 1 is white (text).
    img = Image.new('1', (width, height), color=0)
    draw = ImageDraw.Draw(img)
    
    # 2. Load the custom TTF font
    try:
        font = ImageFont.truetype(font_path, font_size)
    except Exception as e:
        print(f"Error loading font '{font_path}': {e}")
        sys.exit(1)
        
    # 3. Calculate text size to perfectly center it
    try:
        # Newer Pillow versions
        bbox = draw.textbbox((0, 0), text, font=font)
        text_w = bbox[2] - bbox[0]
        text_h = bbox[3] - bbox[1]
    except AttributeError:
        # Older Pillow versions
        text_w, text_h = draw.textsize(text, font=font)
        
    x = (width - text_w) // 2
    # Nudge the Y coordinate slightly up for visual balance in Upheaval
    y = (height - text_h) // 2 - 2 
    
    # 4. Draw the text in white (1) onto the black (0) background
    draw.text((x, y), text, font=font, fill=1)
    
    # 5. Extract pixels and pack to bytes (Horizontal stride, MSB left)
    stride = (width + 7) // 8
    pixels = list(img.getdata())
    packed_data = []
    
    for row in range(height):
        for x_stride in range(stride):
            current_byte = 0
            for bit_offset in range(8):
                px_x = (x_stride * 8) + bit_offset
                if px_x < width:
                    pixel_val = pixels[row * width + px_x]
                    if pixel_val == 1: # If text pixel
                        current_byte |= (1 << (7 - bit_offset))
            packed_data.append(current_byte)
            
    # 6. Generate the cohesive C header file
    lines = []
    lines.append("/* =======================================================================")
    lines.append(f"   {array_name.upper()} standard bitmap data")
    lines.append(f"   Font used: {os.path.basename(font_path)}")
    lines.append(f"   Text rendered: '{text}'")
    lines.append(f"   Dimensions: {width} x {height}")
    lines.append("   ======================================================================= */\n")
    
    guard = f"{array_name.upper()}_LOGO_H"
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}\n")
    
    lines.append(f"#define {array_name.upper()}_W  {width}")
    lines.append(f"#define {array_name.upper()}_H  {height}")
    lines.append(f"#define {array_name.upper()}_STRIDE {stride}\n")
    
    lines.append(f"static const unsigned char {array_name}_bmp[{height}][{stride}] = {{")

    for row in range(height):
        row_hex = []
        for x_stride in range(stride):
            idx = row * stride + x_stride
            row_hex.append(f"0x{packed_data[idx]:02x}")
        comma = "," if row < height - 1 else ""
        lines.append(f"    {{{', '.join(row_hex)}}}{comma} /* row {row} */")

    lines.append("};") # Safe closing brace without f-string interference
    
    lines.append("\n/* Example C Drawing Boilerplate */")
    lines.append("/*")
    lines.append(f"static void draw_{array_name}(int start_y, short int color) {{")
    lines.append("    int row, col_byte, bit;")
    lines.append(f"    int start_x = (320 - {array_name.upper()}_W) / 2;\n")
    lines.append(f"    for (row = 0; row < {array_name.upper()}_H; row++) {{")
    lines.append(f"        for (col_byte = 0; col_byte < {array_name.upper()}_STRIDE; col_byte++) {{")
    lines.append(f"            unsigned char bits = {array_name}_bmp[row][col_byte];")
    lines.append("            for (bit = 0; bit < 8; bit++) {")
    lines.append("                if (bits & (0x80 >> bit)) {")
    lines.append("                    plot_pixel(start_x + col_byte * 8 + bit, start_y + row, color);")
    lines.append("                }")
    lines.append("            }")
    lines.append("        }")
    lines.append("    }")
    lines.append("}")
    lines.append("*/\n")
    lines.append(f"#endif /* {guard} */")
    
    return "\n".join(lines)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Render TTF text directly to a C array.")
    parser.add_argument("text", help="The string of text to render")
    parser.add_argument("font_path", help="Path to the .ttf font file")
    parser.add_argument("array_name", help="C array prefix (e.g., start_logo)")
    parser.add_argument("--font_size", type=int, default=24, help="Size of the font")
    parser.add_argument("--width", type=int, default=240, help="Canvas width")
    parser.add_argument("--height", type=int, default=32, help="Canvas height")
    
    args = parser.parse_args()

    header_data = generate_text_header(args.text, args.font_path, args.font_size, args.array_name, args.width, args.height)
    
    out_file = f"{args.array_name}_logo.h"
    with open(out_file, "w") as f:
        f.write(header_data)

    print(f"Success! Generated {out_file} using text '{args.text}'")