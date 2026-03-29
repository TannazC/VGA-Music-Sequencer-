import os
import sys
import argparse
from PIL import Image, ImageDraw, ImageFont

def generate_font_atlas(font_path, font_size, array_name, prefix, output_base):
    chars = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
    
    if "m5x7" in font_path.lower() or font_size <= 8:
        char_w, char_h = 5, 8
    else:
        char_w, char_h = 12, 16 

    stride = (char_w + 7) // 8

    try:
        font = ImageFont.truetype(font_path, font_size)
    except Exception as e:
        print(f"Error: {e}")
        return

    # --- GENERATE HEADER FILE (.h) ---
    h_lines = [
        f"#ifndef {prefix}_H",
        f"#define {prefix}_H\n",
        f"#define {prefix}_WIDTH  {char_w}",
        f"#define {prefix}_HEIGHT {char_h}",
        f"#define {prefix}_STRIDE {stride}\n",
        f"extern const unsigned char {array_name}_atlas[{len(chars)}][{char_h}][{stride}];\n",
        f"static inline int get_{array_name}_index(char c) {{",
        f"    if (c < 32 || c > 126) return 0;",
        f"    return (int)c - 32;",
        f"}}\n",
        f"#endif"
    ]

    with open(f"{output_base}.h", "w") as f:
        f.write("\n".join(h_lines))

    # --- GENERATE SOURCE FILE (.c) ---
    c_lines = [
        f'#include "{output_base}.h"\n',
        f"const unsigned char {array_name}_atlas[{len(chars)}][{char_h}][{stride}] = {{"
    ]

    for c in chars:
        img = Image.new('1', (char_w, char_h), color=0)
        draw = ImageDraw.Draw(img)
        try:
            bbox = draw.textbbox((0, 0), c, font=font)
            w, h = bbox[2] - bbox[0], bbox[3] - bbox[1]
        except AttributeError:
            w, h = draw.textsize(c, font=font)
            
        draw.text(((char_w - w)//2, (char_h - h)//2 - 1), c, font=font, fill=1)
        pixels = list(img.getdata())
        
        c_lines.append(f"    {{ /* '{c}' */")
        for r in range(char_h):
            row_bytes = [f"0x{0:02x}"] * stride
            for s in range(stride):
                byte_val = 0
                for bit in range(8):
                    px_x = s * 8 + bit
                    if px_x < char_w:
                        if pixels[r * char_w + px_x] == 1:
                            byte_val |= (0x80 >> bit)
                row_bytes[s] = f"0x{byte_val:02x}"
            comma = "," if r < char_h - 1 else ""
            c_lines.append(f"        {{{', '.join(row_bytes)}}}{comma}")
        c_lines.append(f"    }}{',' if c != chars[-1] else ''}")

    c_lines.append("};")

    with open(f"{output_base}.c", "w") as f:
        f.write("\n".join(c_lines))
    
    print(f"Success! Created {output_base}.h and {output_base}.c")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("font_file")
    parser.add_argument("font_size", type=int)
    parser.add_argument("array_name")
    parser.add_argument("prefix")
    parser.add_argument("output_base")
    args = parser.parse_args()
    generate_font_atlas(args.font_file, args.font_size, args.array_name, args.prefix, args.output_base)