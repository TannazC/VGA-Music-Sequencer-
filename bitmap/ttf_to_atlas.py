import os
import argparse
from PIL import Image, ImageDraw, ImageFont

def generate_font_atlas(font_path, font_size, array_name, prefix, output_base):
    chars = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
    
    try:
        font = ImageFont.truetype(font_path, font_size)
    except Exception as e:
        print(f"Error: {e}")
        return

    # Create a dummy image to measure character bounds
    dummy = Image.new('1', (1, 1))
    draw = ImageDraw.Draw(dummy)
    
    max_w, max_h = 0, 0
    for c in chars:
        try:
            bbox = draw.textbbox((0, 0), c, font=font)
            max_w = max(max_w, bbox[2] - bbox[0])
            max_h = max(max_h, bbox[3] - bbox[1])
        except AttributeError:
            w, h = draw.textsize(c, font=font)
            max_w = max(max_w, w)
            max_h = max(max_h, h)

    # Use found dimensions (adding 1px buffer)
    char_w, char_h = max_w + 1, max_h + 1
    stride = (char_w + 7) // 8

    lines = [
        f"/* Font Atlas: {array_name} - Scaled to {font_size}pt */",
        f"#ifndef {prefix}_H",
        f"#define {prefix}_H\n",
        f"#define {prefix}_WIDTH  {char_w}",
        f"#define {prefix}_HEIGHT {char_h}",
        f"#define {prefix}_STRIDE {stride}\n",
        f"static const unsigned char {array_name}_atlas[{len(chars)}][{char_h}][{stride}] = {{"
    ]

    for c in chars:
        img = Image.new('1', (char_w, char_h), color=0)
        draw = ImageDraw.Draw(img)
        draw.text((0, 0), c, font=font, fill=1)
        pixels = list(img.getdata())
        
        lines.append(f"    {{ /* '{c}' */")
        for r in range(char_h):
            row_bytes = []
            for s in range(stride):
                byte_val = 0
                for bit in range(8):
                    px_x = s * 8 + bit
                    if px_x < char_w:
                        if pixels[r * char_w + px_x] == 1:
                            byte_val |= (0x80 >> bit)
                row_bytes.append(f"0x{byte_val:02x}")
            comma = "," if r < char_h - 1 else ""
            lines.append(f"        {{{', '.join(row_bytes)}}}{comma}")
        char_comma = "," if c != chars[-1] else ""
        lines.append(f"    }}{char_comma}")

    lines.append("};\n")
    lines.append(f"static inline int get_{array_name}_index(char c) {{")
    lines.append(f"    if (c < 32 || c > 126) return 0;")
    lines.append(f"    return (int)c - 32;")
    lines.append("}\n")
    lines.append("#endif")

    with open(f"{output_base}.h", "w") as f:
        f.write("\n".join(lines))
    
    print(f"Success! Created {output_base}.h ({char_w}x{char_h}, stride {stride})")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("font_file")
    parser.add_argument("font_size", type=int)
    parser.add_argument("array_name")
    parser.add_argument("prefix")
    parser.add_argument("output_base")
    args = parser.parse_args()
    generate_font_atlas(args.font_file, args.font_size, args.array_name, args.prefix, args.output_base)