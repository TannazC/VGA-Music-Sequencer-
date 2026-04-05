import os

# Define the exact order files need to be combined.
# Headers must come before the C files that use them.
FILES_TO_COMBINE = [
    "treble_clef_bitmap.h",
    "skinny_font.h",
    "background.h",
    "toolbar.h",
    "sprites.h",
    "sequencer_audio.h",
    "start_menu.h",
    "piano_samples.h",
    "xylophone_samples.h",
    "start_menu.c",
    "background.c",
    "toolbar.c",
    "sequencer_audio.c",
    "main.c"
]

OUTPUT_FILE = "combined.c"

def combine_files(input_files, output_file):
    with open(output_file, 'w', encoding='utf-8') as outfile:
        outfile.write("/* Auto-generated combined C file */\n\n")

        for filename in input_files:
            if not os.path.exists(filename):
                print(f"Warning: {filename} not found in the current directory. Skipping.")
                continue

            outfile.write(f"/* =========================================\n")
            outfile.write(f"   Start of {filename}\n")
            outfile.write(f"   ========================================= */\n")

            with open(filename, 'r', encoding='utf-8') as infile:
                for line in infile:
                    # Comment out local includes to prevent compilation errors
                    if line.strip().startswith('#include "'):
                        outfile.write(f"// Skipped local include by merge script: {line}")
                    else:
                        outfile.write(line)

            outfile.write(f"\n/* =========================================\n")
            outfile.write(f"   End of {filename}\n")
            outfile.write(f"   ========================================= */\n\n")

    print(f"Success! Combined {len(input_files)} files into {output_file}.")

if __name__ == "__main__":
    combine_files(FILES_TO_COMBINE, OUTPUT_FILE)