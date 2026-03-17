# Makefile — Build system for the music sequencer project
#
# Responsibilities:
#   - Define the compiler: niosv-elf-gcc (or gcc for CPUlator simulation)
#   - Define CFLAGS: -O1 -Wall -std=c99 -I. (include root for config.h)
#   - List all source files:
#       main.c interrupts.c
#       drivers/ps2.c drivers/timer.c
#       graphics/vga.c graphics/draw.c graphics/bg.c
#       audio/synth.c audio/audio.c
#       sequencer/pattern.c sequencer/engine.c sequencer/ui.c
#   - Define the output target: sequencer.elf
#   - Define a 'clean' target that removes .o files and the .elf
#   - Define an 'all' target that builds sequencer.elf
#   - Optionally define a 'flash' target that uses the Nios V download tool
#     to program the .elf onto the DE1-SoC via JTAG
#
# Notes:
#   - bg_data.h must exist before compiling graphics/bg.c —
#     run 'python3 assets/convert.py assets/bg.png assets/bg_data.h' first
#   - Add -lm to LDFLAGS if using math.h (e.g., for floating-point freq calculations)
#   - CPUlator does not require the niosv-elf-gcc cross-compiler; use it for debugging
