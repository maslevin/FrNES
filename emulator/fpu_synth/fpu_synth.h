#ifndef FPU_SYNTH_H
#define FPU_SYNTH_H

/*
 * A wavetable channel bank on the SH4's FPU (prototype, perf_runs/fpu_synth/README.md): up to 8 channels, each
 * playing a waveform of any length from a 16.16 phase, optionally with its step bent every sample by another channel's
 * output (the HuC6280's LFO, the FDS's modulator), mixed to 1 or 2 outputs through per channel gains. On the SH4 the
 * gains live in XMTRX (one load per gain change) and a sample's mix is 1 FTRV per 4 channels; elsewhere (the host)
 * the same arithmetic in C. No dependency on the rest of FrNES.
 */
#include <stdint.h>

#define FS_MAX_CHANNELS 8

typedef struct {
    const float* wave;       // the waveform, 'length' samples
    uint32_t length;         // in samples (1-256)
    uint32_t phase;          // position in samples, 16.16
    uint32_t step;           // per output sample, 16.16
    int32_t mod_source;      // -1, or the channel whose output bends this one's step
    float mod_depth;         // 16.16 step change per unit of the source's output
    float out;               // the last sample (what a modulated channel reads)
} fsyn_channel_t;

typedef struct {
    uint32_t count;          // channels in use (1-8)
    uint32_t outputs;        // 1 (mono) or 2 (left, right)
    fsyn_channel_t ch[FS_MAX_CHANNELS];
    // the gains as XMTRX holds them: rows L0-3, R0-3, L4-7, R4-7 (mono: L only)
    float matrix[4][4] __attribute__((aligned(8)));
    uint32_t matrix_dirty;
} fsyn_bank_t;

void fsyn_init(fsyn_bank_t* bank, uint32_t count, uint32_t outputs);
void fsyn_set_gain(fsyn_bank_t* bank, uint32_t channel, float left, float right);
// n samples added into out (interleaved L, R when stereo)
void fsyn_render(fsyn_bank_t* bank, float* out, uint32_t n);
// the same, every step in plain C (the reference the FPU path is checked against)
// the same, rendered a channel at a time over blocks (the faster path; same output)
void fsyn_render_blocked(fsyn_bank_t* bank, float* out, uint32_t n);
// channel-major with the gains applied in each channel's loop (no modulation)
void fsyn_render_accumulate(fsyn_bank_t* bank, float* out, uint32_t n);
void fsyn_render_reference(fsyn_bank_t* bank, float* out, uint32_t n);

// SH4 cycle counts (perf_runs/fpu_synth): print receives one line per measurement
void fsyn_bench(void (*print)(const char* line));

#endif
