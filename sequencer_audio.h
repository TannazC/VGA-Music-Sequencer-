/*
 * sequencer_audio.h
 * ─────────────────────────────────────────────────────────────────────
 * Include this in vga_music_v2.c to wire up playback.
 *
 * In vga_music_v2.c, add at the top:
 *     #include "sequencer_audio.h"
 *
 * In the main keyboard loop, add:
 *     if (b == KEY_Q) play_sequence();
 *
 * That is all. The audio file handles everything else.
 * ─────────────────────────────────────────────────────────────────────
 */

#ifndef SEQUENCER_AUDIO_H
#define SEQUENCER_AUDIO_H

#define KEY_Q  0x15   /* PS/2 Set-2 scancode for Q */

/* Trigger full playback: staff 0 bar → staff 1 → staff 2 → staff 3.
   Green playhead animates column by column on the active staff only.
   Audio plays each column's notes as square waves at the correct pitch.
   Returns when the full sequence has finished. */
void play_sequence(void);

#endif /* SEQUENCER_AUDIO_H */
