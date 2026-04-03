# VGA Music Sequencer — DE1-SoC
Co-Authors: Tannaz Chowdhury & Dareen Nasreldin

An embedded step sequencer implemented in C on the DE1-SoC FPGA board. Compose music on a sheet-music-style grid rendered over VGA, play it back in real time through the onboard audio output, and edit notes live using a PS/2 keyboard.

---
<img width="1184" height="822" alt="image" src="https://github.com/user-attachments/assets/6eace175-08ad-4ec7-a824-d519e79dc17d" />
<img width="1182" height="891" alt="image" src="https://github.com/user-attachments/assets/d39b6272-a281-4c03-a4f4-3fe4514d9dab" />


## Features

- Sheet-music grid with 4 staves, up to 8 pages, and 16 columns per staff
- Multiple note durations: whole, half, quarter, 8th, 16th, and rest
- Sharps, flats, and naturals per note
- Three instrument modes: **Beep** (square-wave oscillator), **Piano** (PCM samples with sustain loop), **Xylophone** (PCM samples, one-shot)
- Adjustable BPM (40–999) with live tempo control
- Animated green playhead tracking playback column by column
- Preloaded song templates to load and edit
- Pause, stop, and restart during playback without losing your composition

---

## Hardware Requirements

- Altera DE1-SoC board
- PS/2 keyboard
- VGA display
- Audio output (headphones or speakers via 3.5 mm jack)

---

## Building and Running

> **Important:** Follow these steps exactly every time. The GDB server must be restarted when switching between programs — skipping this causes the server to cache the old `.elf` and the board will not update.

### First time (or after switching from another program)

**Tab 1:**
```
cd path\to\vga_music_v2
./gmake DE1-SoC
./gmake DETECT_DEVICES
./gmake GDB_SERVER        ← leave this tab open
```

**Tab 2 (new PowerShell window):**
```
cd path\to\vga_music_v2
./gmake
./gmake GDB_CLIENT
```

### Switching from another program

1. Close the GDB_CLIENT tab (`Ctrl+C` or close the window)
2. Close the GDB_SERVER tab (`Ctrl+C` or close the window) — **this step is mandatory**
3. In Tab 1: `cd` into `vga_music_v2`, then `./gmake GDB_SERVER`
4. In Tab 2: `cd` into `vga_music_v2`, then `./gmake` and `./gmake GDB_CLIENT`

> If the keyboard does not respond after loading, the GDB server was not restarted. The server caches the old `.elf` in memory. Always restart it when changing programs.

---

## Keyboard Controls

### Cursor movement
| Key | Action |
|-----|--------|
| `W` `A` `S` `D` | Move cursor up / left / down / right |

### Note placement and deletion
| Key | Action |
|-----|--------|
| `Space` | Place note at cursor |
| `Delete` | Remove note at cursor |
| `↑` / `↓` | Shift placed note up or down one pitch slot |

### Note duration
| Key | Duration |
|-----|----------|
| `1` | Whole note |
| `2` | Half note |
| `3` | Quarter note |
| `4` | Beamed pair of 8th notes |
| `5` | Beamed group of 4 sixteenth notes |
| `6` | Beamed pair of 16th notes |
| `7` | Single 16th note |
| `8` | Rest |

### Accidentals
| Key | Accidental |
|-----|------------|
| `Z` | None |
| `X` | Sharp (toggle) |
| `C` | Flat (toggle) |
| `V` | Natural (toggle) |

### Playback
| Key | Action |
|-----|--------|
| `Q` | Play from page 1 |
| `R` | Restart playback |
| `E` (during playback) | Pause / resume |
| `T` (during playback) | Stop |

### Tempo
| Key | Action |
|-----|--------|
| `-` | Decrease BPM by 5 |
| `+` | Increase BPM by 5 |

### Pages
| Key | Action |
|-----|--------|
| `←` / `→` | Navigate to previous / next page |
| `K` | Add a page (max 8) |
| `L` | Remove the last page |

---

## How It Works

### Audio engine

Playback is driven by a **polling loop**. The sequencer iterates staff by staff, column by column. For each column it calls `play_column()`, which pushes a fixed number of samples directly into the WM8731 audio FIFO via memory-mapped I/O. The number of samples is determined by:

```
quarter_samples = (8000 Hz × 60) / BPM
note_samples    = duration_64 × (quarter_samples / 16)
```

where `duration_64` is 64 for a whole note, 32 for a half, 16 for a quarter, and so on down to 4 for a single 16th. The CPU busy-waits on the FIFO empty-slot registers (`wsrc`/`wslc`) before writing each sample, so timing is locked to the hardware sample rate of 8 kHz with no jitter.

### Pitch and instruments

- **Beep** — a fixed-point phase accumulator generates a square wave at the target frequency in real time. Changing pitch changes the phase increment; no samples are involved.
- **Piano / Xylophone** — each pitch slot maps to a pre-recorded 16-bit PCM array stored in flash. Piano samples sustain by looping the final 256 samples of the recording. Xylophone samples play once and then output silence (natural percussive decay). Sharps and flats select entirely separate pre-recorded arrays rather than doing any pitch shifting at runtime.

### Note struct

Every placed note is stored in a flat `Note` array (max 512 entries). Each `Note` records its page, staff, column, pitch slot (0–10 mapping G5 down to D4), duration, accidental, and the pixel coordinates of every note head. Multi-head notes (beamed groups) store up to 4 head positions within a single struct entry. Erasing a note restores the background pixels under its bounding box, then swaps the entry with the last element in the array (`notes[i] = notes[num_notes - 1]; num_notes--`) for O(1) deletion.

### Pages

All notes across all pages share one array, tagged by their `page` field. Switching pages redraws the background and replays only the notes matching the new `cur_page` value. During playback, `main()` pre-scans the array to find the last occupied page, staff, and column so playback stops immediately after the final note rather than running empty columns to the end.

-
