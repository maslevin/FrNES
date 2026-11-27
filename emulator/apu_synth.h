#ifndef __APU_SYNTH_H
#define __APU_SYNTH_H

#include <kos.h>

/**
 * APU sound synthesis
 *
 * The CPU side only logs APU register writes ($4000-$4013, $4015, $4017) with the CPU cycle they happened at
 * (apu_synth_log_write, cheap, called from write_byte). Once per frame apu_synth_frame() replays the log through
 * a software APU (2 pulse, triangle, noise and DMC channels, frame sequencer, non-linear mixer) and produces the
 * frame's 44.1 kHz samples into a ring buffer, which a KOS sound stream plays through the AICA. Everything that
 * touches the AICA (stream polling uses the store queues) happens in apu_synth_poll(), which must only be called
 * outside frame generation.
 */
#define APU_SYNTH_SAMPLE_RATE 44100

int apu_synth_init();

// while set, the stream is still fed from the synthesized sound (same work and timing) but plays silence; the
// unthrottled test harness sets it, since it produces sound faster than the AICA plays it
extern int apu_synth_muted;
void apu_synth_shutdown();
// stops the AICA and zeroes its sound RAM (outside the frame loop)
void apu_synth_silence();

/**
 * Cartridge sound chips, mixed with the APU. The mapper picks one at init (apu_synth_set_expansion) and logs its
 * sound register writes with apu_synth_log_write (with addresses outside $4000-$4017)
 */
#define APU_EXPANSION_NONE  0
#define APU_EXPANSION_VRC6  1
#define APU_EXPANSION_5B    2
#define APU_EXPANSION_MMC5  3
#define APU_EXPANSION_N163  4
#define APU_EXPANSION_VRC7  5
#define APU_EXPANSION_FDS   6
void apu_synth_set_expansion(uint32_t chip);
// after setting the chip once the synth is running (apu_synth_init does it for a chip set before it)
void apu_synth_expansion_ready(void);

/**
 * Output options (the Options menu, beside the picture's bilinear filter). Cold: call them when a game starts and
 * whenever the option changes, before or after apu_synth_init, from the main thread between frames (never from the
 * sound callback); the choice applies from the next frame on, and is kept from game to game. Both default to off / standard.
 *  - Sound filter: OFF the DC block only (as before); NES the front loader's 90 Hz + 440 Hz high pass and 14 kHz low
 *    pass; FAMICOM its 37 Hz high pass (nesdev "APU Mixer"). Costs nothing measurable.
 *  - Sound quality: HIGH averages the 2A03 / VRC6 / 5B / MMC5 tone channels over each sample (far less aliasing) for
 *    ~0.5 ms a frame (2A03 only) to ~2 ms (VRC6 / 5B) on the console.
 */
#define APU_SYNTH_FILTER_OFF       0
#define APU_SYNTH_FILTER_NES       1
#define APU_SYNTH_FILTER_FAMICOM   2
void apu_synth_set_sound_filter(uint32_t filter);
// Sound: 1 on (the default), 0 off (no synthesis, the AICA silent, the frames paced by a timer at the NES's rate)
void apu_synth_set_sound_enabled(uint32_t enabled);
uint32_t apu_synth_sound_enabled(void);
#define APU_SYNTH_QUALITY_STANDARD 0
#define APU_SYNTH_QUALITY_HIGH     1
void apu_synth_set_sound_quality(uint32_t quality);

// test builds: the expansion chip's state as text
int apu_synth_expansion_state(char* buffer, int size);

/**
 * Logs an APU register write at CPU cycle 'cycle'
 */
void apu_synth_log_write(uint16_t address, uint8_t value, uint32_t cycle);

/**
 * Synthesizes the samples for CPU cycles up to 'end_cycle' (the end of the frame), replaying the logged writes
 */
void apu_synth_frame(uint32_t end_cycle);
// called with each frame's synthesized samples (the NSF player's visualizer; test builds: to write them to a file),
// NULL for none
void apu_synth_set_tap(void (*tap)(const int16_t* samples, uint32_t count));

/**
 * Feeds the AICA stream. Call outside frame generation (store queues). Returns the number of samples buffered.
 */
uint32_t apu_synth_poll();
// the sound's rate for the frame about to run: frames_per_second the game runs at under vblank pacing (the display's
// rate; the AICA buffer level adds a small correction), 0 for the NES's own rate (sound pacing)
void apu_synth_pace(float frames_per_second);

/**
 * Samples waiting in the ring buffer
 */
uint32_t apu_synth_buffered();
// player builds with the frame display (APU_SYNTH_TIME_EXPANSION): microseconds the expansion audio took since the
// last call (sampled), else 0
uint32_t apu_synth_expansion_us();

#endif
