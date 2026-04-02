/**
 * @file sequencer_audio.h
 * @brief Public interface for the sequencer playback engine.
 *
 * Include this header in vga_music_v2.c and call play_sequence() from the
 * main keyboard loop to trigger playback. The audio engine handles all
 * timing, instrument selection, playhead animation, and transport control
 * internally.
 *
 * Usage:
 * @code
 *   #include "sequencer_audio.h"
 *   // inside the PS/2 keyboard loop:
 *   if (b == KEY_Q) play_sequence();
 * @endcode
 *
 * @authors Tannaz Chowdhury, Dareen Nasreldin
 */

#ifndef SEQUENCER_AUDIO_H
#define SEQUENCER_AUDIO_H

/** @brief PS/2 Set-2 scancode for the Q key (triggers playback). */
#define KEY_Q 0x15

/**
 * @brief Plays the full sequence for the current page.
 *
 * Iterates staff 0 → staff 3, column by column. For each column, draws the
 * green playhead, plays all notes on that column at the current BPM and
 * instrument setting, then erases the playhead. Polls the PS/2 keyboard
 * between columns for pause, stop, and restart commands. Returns once the
 * last note column has played or the user stops playback.
 */
void play_sequence(void);

#endif /* SEQUENCER_AUDIO_H */
