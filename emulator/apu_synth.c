// (branch perf-fpu-synth: the N163 through the FPU synth prototype unless FRNES_NO_FPU_N163)
// (in the player builds and on the host: in the test builds its frame would be reached through apu_synth_tap, which
// they otherwise fold to NULL, so their frame stays b28ab3cd's to the instruction, also .hotdata, where the tap lives)
#if !defined(FRNES_NO_FPU_N163) && (defined(FRNES_SYNTH_OUTLINE) || defined(FRNES_HOST))
#define FRNES_FPU_N163
#endif
// (the output filters and the averaging are options: apu_synth_set_sound_filter / apu_synth_set_sound_quality)
#ifdef _arch_dreamcast
#include <sh4zam/shz_xmtrx.h>
#endif
#include "apu_synth.h"
#include <stdlib.h>
#ifndef FRNES_HOST
#include <malloc.h>
#endif
#include <math.h>
#include "memory.h"

#include <dc/sound/stream.h>
#ifndef FRNES_HOST
#include <arch/timer.h>
#endif
#include <dc/sound/sound.h>
#include <dc/spu.h>

#define CPU_CLOCK_NTSC 1789773
// CPU cycles per output sample in 16.16 fixed point
#define CYCLES_PER_SAMPLE_FP ((uint32_t)(((uint64_t)CPU_CLOCK_NTSC << 16) / APU_SYNTH_SAMPLE_RATE))

/**
 * Register write log, filled by the CPU and replayed by the synthesizer
 */
#define LOG_SIZE 2048
typedef struct {
    uint32_t cycle;
    uint16_t address;
    uint8_t value;
} apu_write_t;
static apu_write_t write_log[LOG_SIZE];
static uint32_t log_head = 0;   // next entry to replay
static uint32_t log_tail = 0;   // next free entry

void apu_synth_log_write(uint16_t address, uint8_t value, uint32_t cycle) {
    uint32_t next = (log_tail + 1) & (LOG_SIZE - 1);
    if (next == log_head) {
        return;
    }
    write_log[log_tail].cycle = cycle;
    write_log[log_tail].address = address;
    write_log[log_tail].value = value;
    log_tail = next;
}

/**
 * Tables
 */
static const uint8_t length_table[32] = {
    10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};
static const uint8_t duty_table[4] = { 0x02, 0x06, 0x1e, 0xf9 };   // bit n = output of sequencer step n
// (in .hotdata: read every output sample, its operand cache sets shouldn't move with the size of unrelated code)
static const uint8_t triangle_table[32] FRNES_HOT_RODATA = {
    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};
static const uint16_t noise_period_table[16] = { 4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068 };
static const uint16_t dmc_rate_table[16] = { 428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54 };

// non-linear mixer, as signed 16 bit centered around 0 after the DC blocker
static int16_t pulse_mix[31];
static int16_t tnd_mix[203];

/**
 * Channel state
 */
typedef struct {
    uint8_t start;
    uint8_t divider;
    uint8_t decay;
    uint8_t loop;
    uint8_t constant;
    uint8_t volume;
} envelope_t;

typedef struct {
    uint8_t enabled;
    uint8_t duty;
    uint8_t step;
    uint16_t period;
    int32_t timer;
    uint8_t length;
    envelope_t envelope;
    uint8_t sweep_enabled;
    uint8_t sweep_period;
    uint8_t sweep_negate;
    uint8_t sweep_shift;
    uint8_t sweep_reload;
    uint8_t sweep_divider;
    uint8_t ones_complement;
} pulse_t;

typedef struct {
    uint8_t enabled;
    uint16_t period;
    int32_t timer;
    uint8_t step;
    uint8_t length;
    uint8_t control;
    uint8_t linear_reload_value;
    uint8_t linear_counter;
    uint8_t linear_reload;
} triangle_t;

typedef struct {
    uint8_t enabled;
    uint8_t mode;
    uint16_t period;
    int32_t timer;
    uint16_t shift;
    uint8_t length;
    envelope_t envelope;
} noise_t;

typedef struct {
    uint8_t enabled;
    uint8_t loop;
    uint16_t rate;
    int32_t timer;
    uint8_t output;
    uint16_t sample_address;
    uint16_t sample_length;
    uint16_t current_address;
    uint16_t bytes_remaining;
    uint8_t shift;
    uint8_t bits_remaining;
    uint8_t silence;
} dmc_t;

static pulse_t pulse[2];
static triangle_t triangle;
static noise_t noise;
static dmc_t dmc;

// frame sequencer
static uint8_t sequencer_five_step;
static uint8_t sequencer_step;
static int32_t sequencer_counter;   // CPU cycles until the next sequencer step

// synthesis time
static uint32_t synth_cycle;
static uint32_t sample_fraction;
// CPU cycles per output sample now (16.16): CYCLES_PER_SAMPLE_FP at the NES's rate, less under vblank pacing
// (apu_synth_pace)
static uint32_t cycles_per_sample_fp = CYCLES_PER_SAMPLE_FP;
static int32_t dc_previous_in;
static int32_t dc_previous_out;
/*
 * The output options (Matt, 2026-11-09: off by default, like the picture's bilinear filter).
 *
 * Sound filter: APU_SYNTH_FILTER_OFF is the DC block (a high pass at ~37 Hz). APU_SYNTH_FILTER_NES / _FAMICOM are the
 * consoles' own output filters:
 *  - NES: a 90 Hz and a 440 Hz first order high pass, then a 14 kHz first order low pass. nesdev's "APU Mixer" page
 *    lists the 90 Hz high pass and the 14 kHz low pass (and a second order low pass far above hearing, left out). The
 *    440 Hz one is from the analysis that page cites (lidnariq, forums.nesdev.org/viewtopic.php?p=44255, 2009-03-15:
 *    corners of 90 Hz, 442 Hz and 14 kHz; the 442 Hz part not yet identified there).
 *  - Famicom: "APU Mixer": the Famicom "ONLY specifies a first-order high-pass filter at 37 Hz" (the RF modulator's
 *    properties unknown and varying, so nothing more).
 * Either is one linear step: with the states y1, y2, y3 (each stage's last output) and d = x - the last input,
 *   y1' = a1 (y1 + d), y2' = a2 (y2 + y1' - y1), y3' = y3 + b (y2' - y3)
 * is the matrix below times (y1, y2, y3, d), a stage left out passing its input on (y2' = y1', b = 1): on the SH4 one
 * FTRV a sample, the matrix in XMTRX for the frame.
 *
 * Sound quality: APU_SYNTH_QUALITY_HIGH averages the 2A03 pulses and triangle, the VRC6, the 5B and the MMC5 pulses
 * over each sample's CPU cycles instead of sampling them (far less aliasing; ~0.5 ms a frame for the 2A03, ~2 ms with
 * the VRC6 or 5B on the console).
 *
 * The setters apply the choice at once (they're called between frames, from the main thread: a frame boundary), with
 * the filter's state handed over; apu_synth_init starts the chosen filter from rest.
 */
static struct {
    float matrix[4][4] __attribute__((aligned(8)));
    float y1, y2, y3, x;
    uint8_t filter, average;              // in use
    uint8_t wanted_filter, wanted_average; // asked for (applied at the next frame)
    void (*user_tap)(const int16_t* samples, uint32_t count); // apu_synth_set_tap's
    uint8_t sound_off;                    // apu_synth_set_sound_enabled(0)
    // sound off: the samples the frames would have made and the time they'd have taken to play (apu_synth_poll's
    // count, so the frame loop's wait for the sound paces by the clock): the samples not yet "played", the time it was
    // last counted at, and the part of a sample left over
    int32_t off_level;
    uint32_t off_rest;
    uint64_t off_last_us;
    // (apu_synth_frame_silent's calls, set by apu_synth_set_sound_enabled: direct calls from the silent frame gave
    // sequencer_clock a second caller, and GCC split it and inlined a part into the sample loop, also in the test
    // builds, where the silent frame is never reached)
    void (*off_apply)(uint16_t address, uint8_t value);
    void (*off_clock)(void);
    void (*off_levels)(void);
} output_options;

// (the NES's stages written out as the prototype had them, so its output is unchanged to the bit)
static void output_filter_matrix(uint32_t filter) {
    const float dt = 1.0f / APU_SYNTH_SAMPLE_RATE, pi2 = 6.2831853f;
    memset(output_options.matrix, 0, sizeof(output_options.matrix));
    if (filter == APU_SYNTH_FILTER_NES) {
        const float rc1 = 1.0f / (pi2 * 90.0f), rc2 = 1.0f / (pi2 * 440.0f), rc3 = 1.0f / (pi2 * 14000.0f);
        const float a1 = rc1 / (rc1 + dt), a2 = rc2 / (rc2 + dt), b = dt / (rc3 + dt);
        const float m[4][4] = {
            { a1, 0, 0, a1 },
            { a2 * (a1 - 1), a2, 0, a2 * a1 },
            { b * a2 * (a1 - 1), b * a2, 1 - b, b * a2 * a1 },
            { 0, 0, 0, 0 },
        };
        memcpy(output_options.matrix, m, sizeof(m));
    } else if (filter == APU_SYNTH_FILTER_FAMICOM) {
        // (the one high pass; the other two stages pass it on)
        const float rc1 = 1.0f / (pi2 * 37.0f), a1 = rc1 / (rc1 + dt);
        for (uint32_t i = 0; i < 3; i++) {
            output_options.matrix[i][0] = a1;
            output_options.matrix[i][3] = a1;
        }
    }
}

// output ring buffer of 16 bit mono samples
#define RING_SIZE 8192
__attribute__((aligned(32))) static int16_t ring[RING_SIZE];
static volatile uint32_t ring_read = 0;
static volatile uint32_t ring_write = 0;
static snd_stream_hnd_t stream = SND_STREAM_INVALID;

/**
 * Units clocked by the frame sequencer
 */
static void envelope_clock(envelope_t* envelope) {
    if (envelope -> start) {
        envelope -> start = 0;
        envelope -> decay = 15;
        envelope -> divider = envelope -> volume;
    } else if (envelope -> divider) {
        envelope -> divider--;
    } else {
        envelope -> divider = envelope -> volume;
        if (envelope -> decay) {
            envelope -> decay--;
        } else if (envelope -> loop) {
            envelope -> decay = 15;
        }
    }
}

static inline uint8_t envelope_output(envelope_t* envelope) {
    return envelope -> constant ? envelope -> volume : envelope -> decay;
}

static inline uint32_t pulse_sweep_target(pulse_t* p) {
    uint32_t change = p -> period >> p -> sweep_shift;
    if (p -> sweep_negate) {
        return p -> period - change - p -> ones_complement;
    }
    return p -> period + change;
}

static inline uint32_t pulse_muted(pulse_t* p) {
    return (p -> period < 8) || (!p -> sweep_negate && (pulse_sweep_target(p) > 0x7ff));
}

static void pulse_sweep_clock(pulse_t* p) {
    if ((p -> sweep_divider == 0) && p -> sweep_enabled && p -> sweep_shift && !pulse_muted(p)) {
        p -> period = pulse_sweep_target(p) & 0x7ff;
    }
    if ((p -> sweep_divider == 0) || p -> sweep_reload) {
        p -> sweep_divider = p -> sweep_period;
        p -> sweep_reload = 0;
    } else {
        p -> sweep_divider--;
    }
}

static void quarter_frame() {
    envelope_clock(&pulse[0].envelope);
    envelope_clock(&pulse[1].envelope);
    envelope_clock(&noise.envelope);
    if (triangle.linear_reload) {
        triangle.linear_counter = triangle.linear_reload_value;
    } else if (triangle.linear_counter) {
        triangle.linear_counter--;
    }
    if (!triangle.control) {
        triangle.linear_reload = 0;
    }
}

static void half_frame() {
    for (uint32_t i = 0; i < 2; i++) {
        if (pulse[i].length && !pulse[i].envelope.loop) {
            pulse[i].length--;
        }
        pulse_sweep_clock(&pulse[i]);
    }
    if (triangle.length && !triangle.control) {
        triangle.length--;
    }
    if (noise.length && !noise.envelope.loop) {
        noise.length--;
    }
}

// sequencer step lengths in CPU cycles (4 step: 7457, 7456, 7458, 7458; 5 step adds a 7452 step before the last)
static const int32_t sequence_4[4] = { 7457, 7456, 7458, 7459 };
static const int32_t sequence_5[5] = { 7457, 7456, 7458, 7458, 7453 };

// (out of line, as GCC placed it before the options: with their frame copies as callers GCC split it and inlined a part
// into the standard frame's loop)
static __attribute__((noinline)) void sequencer_clock() {
    if (sequencer_five_step) {
        // steps 0-4: quarter on 0, 1, 2, 4; half on 1, 4
        if (sequencer_step != 3) {
            quarter_frame();
        }
        if ((sequencer_step == 1) || (sequencer_step == 4)) {
            half_frame();
        }
        sequencer_step = (sequencer_step + 1) % 5;
        sequencer_counter += sequence_5[sequencer_step];
    } else {
        quarter_frame();
        if ((sequencer_step == 1) || (sequencer_step == 3)) {
            half_frame();
        }
        sequencer_step = (sequencer_step + 1) & 3;
        sequencer_counter += sequence_4[sequencer_step];
    }
}

/**
 * DMC
 */
static void dmc_restart() {
    dmc.current_address = dmc.sample_address;
    dmc.bytes_remaining = dmc.sample_length;
}

static void dmc_clock() {
    if (!dmc.silence) {
        if (dmc.shift & 1) {
            if (dmc.output <= 125) {
                dmc.output += 2;
            }
        } else if (dmc.output >= 2) {
            dmc.output -= 2;
        }
    }
    dmc.shift >>= 1;
    if (dmc.bits_remaining) {
        dmc.bits_remaining--;
    }
    if (dmc.bits_remaining == 0) {
        dmc.bits_remaining = 8;
        if (dmc.bytes_remaining) {
            // the sample is read from CPU memory at synthesis time (normally ROM, so it hasn't changed)
            dmc.shift = read_byte(dmc.current_address);
            dmc.current_address = (dmc.current_address == 0xffff) ? 0x8000 : (dmc.current_address + 1);
            dmc.silence = 0;
            if (--dmc.bytes_remaining == 0 && dmc.loop) {
                dmc_restart();
            }
        } else {
            dmc.silence = 1;
        }
    }
}

/**
 * Expansion sound (cartridge chips), written from the nesdev wiki descriptions. The mapper logs its sound register
 * writes into the same cycle stamped log (apu_synth_log_write), they are applied in order with the APU's.
 */
static uint8_t expansion = APU_EXPANSION_NONE;

void apu_synth_set_expansion(uint32_t chip) {
    expansion = chip;
}

// (the chip set: the FPU N163's buffers allocated (or freed) for it; apu_synth_init calls it, and whoever sets the chip
// after apu_synth_init (the NSF player, the host). Not apu_synth_set_expansion itself: it's in the frame slot)
#ifdef FRNES_FPU_N163
static void fpu_spans_update(uint32_t chip);
#endif
FRNES_COLD_TEXT void apu_synth_expansion_ready(void) {
#ifdef FRNES_FPU_N163
    fpu_spans_update(expansion);
#endif
}

/*
 * VRC6: two pulses ($9000-$9002, $A000-$A002: mode / duty / volume, period low, enable / period high) with 16 step
 * duty cycles, and a sawtooth ($B000-$B002: accumulator rate, period low, enable / period high) whose accumulator adds
 * the rate every other step of 14 and is reset after the 14th; $9003 bit 0 halts both. Output 0-15 per pulse, the
 * saw's accumulator >> 3 (0-31).
 */
typedef struct {
    uint8_t mode, duty, volume, enabled, step;
    uint16_t period;
    int32_t timer;
} vrc6_pulse_t;
static struct {
    vrc6_pulse_t pulse[2];
    uint8_t saw_rate, saw_enabled, saw_step, saw_accumulator, halt;
    uint16_t saw_period;
    int32_t saw_timer;
} vrc6;

static void vrc6_write(uint16_t address, uint8_t value) {
    if (address == 0x9003) {
        vrc6.halt = value & 1;
        return;
    }
    uint32_t reg = address & 3;
    if (address < 0xb000) {
        vrc6_pulse_t* p = &vrc6.pulse[(address >> 12) - 9];
        switch (reg) {
            case 0: p -> mode = value >> 7; p -> duty = (value >> 4) & 7; p -> volume = value & 15; break;
            case 1: p -> period = (p -> period & 0x0f00) | value; break;
            case 2:
                p -> period = (p -> period & 0x00ff) | ((value & 15) << 8);
                p -> enabled = value >> 7;
                if (!p -> enabled) {
                    p -> step = 0;
                }
                break;
        }
    } else {
        switch (reg) {
            case 0: vrc6.saw_rate = value & 0x3f; break;
            case 1: vrc6.saw_period = (vrc6.saw_period & 0x0f00) | value; break;
            case 2:
                vrc6.saw_period = (vrc6.saw_period & 0x00ff) | ((value & 15) << 8);
                vrc6.saw_enabled = value >> 7;
                if (!vrc6.saw_enabled) {
                    vrc6.saw_step = 0;
                    vrc6.saw_accumulator = 0;
                }
                break;
        }
    }
}

// 65536 / n, for the averages (a sample is ~41 CPU cycles: no division in the sample loop)
// (in .rodata: a table in .bss moved the heap's start, and every buffer allocated after it)
#define AR(i) ((i) ? 65536u / ((i) ? (i) : 1u) : 0u)
#define AR8(b) AR(b), AR(b + 1), AR(b + 2), AR(b + 3), AR(b + 4), AR(b + 5), AR(b + 6), AR(b + 7)
static const uint32_t average_reciprocal[128] = { AR8(0), AR8(8), AR8(16), AR8(24), AR8(32), AR8(40), AR8(48), AR8(56),
    AR8(64), AR8(72), AR8(80), AR8(88), AR8(96), AR8(104), AR8(112), AR8(120) };
#undef AR8
#undef AR
static inline uint32_t average_scale(uint32_t sum, uint32_t cycles) {
    // (16.16 of sum / cycles)
    return (cycles < 128) ? sum * average_reciprocal[cycles] : (uint32_t)(((uint64_t)sum << 16) / cycles);
}
// the expansion's output x CPU cycles since the last sample, and the cycles (VRC6 and 5B: their output averaged)
static uint32_t expansion_average_sum, expansion_average_cycles;
static inline int32_t vrc6_level(void);
static inline int32_t s5b_level(void);

static inline void vrc6_run(int32_t cycles) {
    if (vrc6.halt) {
        return;
    }
    for (uint32_t i = 0; i < 2; i++) {
        vrc6_pulse_t* p = &vrc6.pulse[i];
        if (p -> enabled) {
            p -> timer -= cycles;
            while (p -> timer <= 0) {
                p -> timer += p -> period + 1;
                p -> step = (p -> step + 1) & 15;
            }
        }
    }
    if (vrc6.saw_enabled) {
        vrc6.saw_timer -= cycles;
        while (vrc6.saw_timer <= 0) {
            vrc6.saw_timer += vrc6.saw_period + 1;
            if (++vrc6.saw_step == 14) {
                vrc6.saw_step = 0;
                vrc6.saw_accumulator = 0;
            } else if (!(vrc6.saw_step & 1)) {
                vrc6.saw_accumulator += vrc6.saw_rate;
            }
        }
    }
}

static inline __attribute__((always_inline)) void vrc6_run_opt(int32_t cycles, const uint32_t average) {
    if (average) {
        if (vrc6.halt) {
            expansion_average_sum += vrc6_level() * cycles;
            expansion_average_cycles += cycles;
            return;
        }
        // (each channel on its own: its output x the cycles it held it, to each of its steps in turn)
        uint32_t sum = 0;
        for (uint32_t i = 0; i < 2; i++) {
            vrc6_pulse_t* p = &vrc6.pulse[i];
            if (!p -> enabled) {
                continue;
            }
            if (p -> mode) {
                sum += p -> volume * cycles;
            }
            int32_t timer = p -> timer, remaining = cycles;
            uint32_t step = p -> step, high = 0;
            const int32_t reload = p -> period + 1;
            while (timer <= remaining) {
                high += (step <= p -> duty) ? timer : 0;
                remaining -= timer;
                step = (step + 1) & 15;
                timer = reload;
            }
            high += (step <= p -> duty) ? remaining : 0;
            p -> timer = timer - remaining;
            p -> step = step;
            if (!p -> mode) {
                sum += p -> volume * high;
            }
        }
        if (vrc6.saw_enabled) {
            int32_t timer = vrc6.saw_timer, remaining = cycles;
            uint32_t step = vrc6.saw_step, total = 0;
            uint8_t accumulator = vrc6.saw_accumulator;
            const int32_t reload = vrc6.saw_period + 1;
            while (timer <= remaining) {
                total += (accumulator >> 3) * timer;
                remaining -= timer;
                timer = reload;
                if (++step == 14) {
                    step = 0;
                    accumulator = 0;
                } else if (!(step & 1)) {
                    accumulator += vrc6.saw_rate;
                }
            }
            total += (accumulator >> 3) * remaining;
            vrc6.saw_timer = timer - remaining;
            vrc6.saw_step = step;
            vrc6.saw_accumulator = accumulator;
            sum += total;
        }
        expansion_average_sum += sum;
        expansion_average_cycles += cycles;
        return;
    }
    if (vrc6.halt) {
        return;
    }
    for (uint32_t i = 0; i < 2; i++) {
        vrc6_pulse_t* p = &vrc6.pulse[i];
        if (p -> enabled) {
            p -> timer -= cycles;
            while (p -> timer <= 0) {
                p -> timer += p -> period + 1;
                p -> step = (p -> step + 1) & 15;
            }
        }
    }
    if (vrc6.saw_enabled) {
        vrc6.saw_timer -= cycles;
        while (vrc6.saw_timer <= 0) {
            vrc6.saw_timer += vrc6.saw_period + 1;
            if (++vrc6.saw_step == 14) {
                vrc6.saw_step = 0;
                vrc6.saw_accumulator = 0;
            } else if (!(vrc6.saw_step & 1)) {
                vrc6.saw_accumulator += vrc6.saw_rate;
            }
        }
    }
}

static inline int32_t vrc6_level(void) {
    int32_t out = 0;
    for (uint32_t i = 0; i < 2; i++) {
        vrc6_pulse_t* p = &vrc6.pulse[i];
        if (p -> enabled && (p -> mode || (p -> step <= p -> duty))) {
            out += p -> volume;
        }
    }
    if (vrc6.saw_enabled) {
        out += vrc6.saw_accumulator >> 3;
    }
    return out;
}

// a sample's output (averaged over its cycles with the averaging on, in 1/16 steps)
static inline int32_t vrc6_output() {
    int32_t out = 0;
    for (uint32_t i = 0; i < 2; i++) {
        vrc6_pulse_t* p = &vrc6.pulse[i];
        if (p -> enabled && (p -> mode || (p -> step <= p -> duty))) {
            out += p -> volume;
        }
    }
    if (vrc6.saw_enabled) {
        out += vrc6.saw_accumulator >> 3;
    }
    return out;
}

static inline __attribute__((always_inline)) int32_t vrc6_output_opt(const uint32_t average) {
    if (average) {
        int32_t out = expansion_average_cycles ? (int32_t)(average_scale(expansion_average_sum, expansion_average_cycles) >> 12) : vrc6_level() * 16;
        expansion_average_sum = expansion_average_cycles = 0;
        return out;
    } else {
        return vrc6_level();
    }
}

// expansion output in mixer units: a VRC6 pulse at full volume about as loud as an APU pulse at full volume
// (pulse_mix[15]; nesdev's "roughly equivalent", and Matt prefers it to Mesen 2's, 3.8 dB louder). A 5B tone at full
// volume 7.9 dB louder than an APU pulse, as Mesen 2 mixes it (Matt, 2026-11-09: it was as loud as an APU pulse;
// nesdev calls the 5B "very loud compared to other audio expansion carts"; perf_runs/exp_vs_mesen measured both)
#define VRC6_SCALE 290
#define S5B_SCALE 89
// MMC5, FDS and VRC7 as Mesen 2 mixes them (NesSoundMixer: MMC5 x43, FDS x20, VRC7 x1 against its APU pulses), at
// our scale: an APU pulse at full volume is pulse_mix[15] = 4464 here and 747 there, 5.98 to 1. MMC5's pulses and
// PCM are linear, 43 x 5.98 per step: a pulse at full volume is 0.86 of an APU pulse, the PCM at 255 is 14.7 of one
#define MMC5_SCALE 257
// a VRC7 channel at full volume (+-4095 here) as loud as emu2413's (+-2045, x1 in Mesen's mixer): 2.74 APU pulses
#define VRC7_SCALE 3
// the Namco 163's averaged output (+-120 for one channel at full volume) against the APU pulses as Mesen 2 mixes it
#define N163_SCALE 120

/*
 * Sunsoft 5B (FME-7 boards, a YM2149F): $C000 selects a register, $E000 writes it. Registers 0-5 the three tone
 * periods (12 bit), 7 the mixer (bits 0-2 clear: tone A / B / C on), 8-10 the volumes (4 bit, 3 dB per step).
 * The chip runs at half the CPU clock and each tone's square wave toggles every 'period' of its ticks: its 16 steps
 * output on the first 8. Noise and the envelope are not emulated (nor by Mesen 2's 5B).
 */
static struct {
    uint8_t selected;
    uint8_t registers[16];
    int32_t timer[3];
    uint8_t step[3];
    uint8_t half;
} s5b;
static uint8_t s5b_volume[16];

static void s5b_write(uint16_t address, uint8_t value) {
    if (address == 0xc000) {
        s5b.selected = value & 0x0f;
    } else if (address == 0xe000) {
        s5b.registers[s5b.selected] = value;
    }
}

static inline uint32_t s5b_period(uint32_t channel) {
    uint32_t period = s5b.registers[channel * 2] | ((s5b.registers[channel * 2 + 1] & 0x0f) << 8);
    return period ? period : 1;
}

static inline void s5b_run(int32_t cycles) {
    // ticks at half the CPU clock
    int32_t ticks = (cycles + s5b.half) >> 1;
    s5b.half = (cycles + s5b.half) & 1;
    if (!ticks) {
        return;
    }
    for (uint32_t i = 0; i < 3; i++) {
        s5b.timer[i] -= ticks;
        if (s5b.timer[i] <= 0) {
            int32_t period = s5b_period(i);
            uint32_t step = s5b.step[i];
            do {
                s5b.timer[i] += period;
                step++;
            } while (s5b.timer[i] <= 0);
            s5b.step[i] = step & 15;
        }
    }
}

static inline __attribute__((always_inline)) void s5b_run_opt(int32_t cycles, const uint32_t average) {
    // ticks at half the CPU clock
    int32_t ticks = (cycles + s5b.half) >> 1;
    s5b.half = (cycles + s5b.half) & 1;
    if (average) {
        // (to each tone's next step in turn, the output's level x the cycles it held; the cycles a half tick leaves over at
        // the level after)
        expansion_average_cycles += cycles;
        int32_t left = cycles - 2 * ticks;
        // (each tone on its own: a tone the mixer lets through at a volume above 0 adds its volume x the cycles it's high,
        // in its 16 steps' first 8; the others step as before)
        uint32_t sum = 0;
        for (uint32_t i = 0; i < 3; i++) {
            const uint32_t heard = !(s5b.registers[7] & (1 << i)) && (s5b.registers[8 + i] & 0x0f);
            if (ticks <= 0) {
                sum += (heard && (s5b.step[i] < 8)) ? s5b_volume[s5b.registers[8 + i] & 0x0f] * cycles : 0;
                continue;
            }
            int32_t timer = s5b.timer[i], remaining = ticks;
            uint32_t step = s5b.step[i], high = 0;
            const int32_t period = s5b_period(i);
            if (heard) {
                // (the level of the first tick: a half tick carried over from the last run was counted at it then)
                const uint32_t first_high = step < 8;
                while (timer <= remaining) {
                    high += (step < 8) ? timer : 0;
                    remaining -= timer;
                    step = (step + 1) & 15;
                    timer = period;
                }
                high += (step < 8) ? remaining : 0;
                timer -= remaining;
                // (in CPU cycles: 2 a tick; a half tick left over (left 1) at the step it ends on, a half tick the last run
                // already counted (left -1) off the first tick, at its level. Taking it off the last step's level made
                // the sum negative when a tone went high on a run's only tick: an unsigned wrap, a full scale spike)
                const uint32_t edge_high = (left > 0) ? (step < 8) : first_high;
                sum += s5b_volume[s5b.registers[8 + i] & 0x0f] * (2 * high + (edge_high ? left : 0));
            } else {
                timer -= ticks;
                while (timer <= 0) {
                    timer += period;
                    step++;
                }
                step &= 15;
            }
            s5b.timer[i] = timer;
            s5b.step[i] = step;
        }
        expansion_average_sum += sum;
        return;
    }
    if (!ticks) {
        return;
    }
    for (uint32_t i = 0; i < 3; i++) {
        s5b.timer[i] -= ticks;
        if (s5b.timer[i] <= 0) {
            int32_t period = s5b_period(i);
            uint32_t step = s5b.step[i];
            do {
                s5b.timer[i] += period;
                step++;
            } while (s5b.timer[i] <= 0);
            s5b.step[i] = step & 15;
        }
    }
}

static inline int32_t s5b_level(void) {
    int32_t out = 0;
    for (uint32_t i = 0; i < 3; i++) {
        if (!(s5b.registers[7] & (1 << i)) && (s5b.step[i] < 8)) {
            out += s5b_volume[s5b.registers[8 + i] & 0x0f];
        }
    }
    return out;
}

static inline int32_t s5b_output() {
    int32_t out = 0;
    for (uint32_t i = 0; i < 3; i++) {
        if (!(s5b.registers[7] & (1 << i)) && (s5b.step[i] < 8)) {
            out += s5b_volume[s5b.registers[8 + i] & 0x0f];
        }
    }
    return out;
}

static inline __attribute__((always_inline)) int32_t s5b_output_opt(const uint32_t average) {
    if (average) {
        int32_t out = expansion_average_cycles ? (int32_t)(average_scale(expansion_average_sum, expansion_average_cycles) >> 12) : s5b_level() * 16;
        expansion_average_sum = expansion_average_cycles = 0;
        return out;
    } else {
        return s5b_level();
    }
}

/*
 * MMC5: two pulse channels like the APU's without the sweep ($5000-$5003, $5004-$5007), $5015 enables them; their
 * envelopes and length counters are clocked at a fixed 240 Hz. $5011 writes the 8 bit PCM output (in write mode,
 * $5010 bit 0 clear).
 */
static struct {
    pulse_t pulse[2];
    uint8_t pcm;
    uint8_t pcm_read_mode;
    int32_t sequencer;
} mmc5;
#define MMC5_QUARTER_FRAME 7457

static void mmc5_write(uint16_t address, uint8_t value) {
    if (address <= 0x5007) {
        pulse_t* p = &mmc5.pulse[(address >> 2) & 1];
        switch (address & 3) {
            case 0:
                p -> duty = value >> 6;
                p -> envelope.loop = (value >> 5) & 1;
                p -> envelope.constant = (value >> 4) & 1;
                p -> envelope.volume = value & 15;
                break;
            case 2:
                p -> period = (p -> period & 0x700) | value;
                break;
            case 3:
                p -> period = (p -> period & 0xff) | ((value & 7) << 8);
                if (p -> enabled) {
                    p -> length = length_table[value >> 3];
                }
                p -> step = 0;
                p -> envelope.start = 1;
                break;
        }
    } else if (address == 0x5010) {
        mmc5.pcm_read_mode = value & 1;
    } else if (address == 0x5011) {
        if (!mmc5.pcm_read_mode && value) {
            mmc5.pcm = value;
        }
    } else if (address == 0x5015) {
        for (uint32_t i = 0; i < 2; i++) {
            mmc5.pulse[i].enabled = (value >> i) & 1;
            if (!mmc5.pulse[i].enabled) {
                mmc5.pulse[i].length = 0;
            }
        }
    }
}

// the MMC5 pulses' level x CPU cycles since the last sample, and the cycles
static uint32_t mmc5_average_sum, mmc5_average_cycles;

static inline void mmc5_run(int32_t cycles) {
    for (uint32_t i = 0; i < 2; i++) {
        pulse_t* p = &mmc5.pulse[i];
        // (periods under 8 are silent, as on the APU, and aren't worth running)
        if (p -> period >= 8) {
            p -> timer -= cycles;
            while (p -> timer <= 0) {
                p -> timer += (p -> period + 1) << 1;
                p -> step = (p -> step - 1) & 7;
            }
        }
    }
    mmc5.sequencer -= cycles;
    while (mmc5.sequencer <= 0) {
        mmc5.sequencer += MMC5_QUARTER_FRAME;
        for (uint32_t i = 0; i < 2; i++) {
            pulse_t* p = &mmc5.pulse[i];
            envelope_clock(&p -> envelope);
            if (p -> length && !p -> envelope.loop) {
                p -> length--;
            }
        }
    }
}

static inline __attribute__((always_inline)) void mmc5_run_opt(int32_t cycles, const uint32_t average) {
    if (average) {
        mmc5_average_cycles += cycles;
        for (uint32_t i = 0; i < 2; i++) {
            pulse_t* p = &mmc5.pulse[i];
            const uint32_t duty = duty_table[p -> duty];
            const uint32_t level = (p -> length && (p -> period >= 8)) ? envelope_output(&p -> envelope) : 0;
            uint32_t step = p -> step, high = 0;
            if (p -> period >= 8) {
                int32_t timer = p -> timer, remaining = cycles;
                const int32_t reload = (p -> period + 1) << 1;
                while (timer <= remaining) {
                    high += ((duty >> step) & 1) ? timer : 0;
                    remaining -= timer;
                    step = (step - 1) & 7;
                    timer = reload;
                }
                high += ((duty >> step) & 1) ? remaining : 0;
                p -> timer = timer - remaining;
                p -> step = step;
            }
            mmc5_average_sum += high * level;
        }
    } else
    for (uint32_t i = 0; i < 2; i++) {
        pulse_t* p = &mmc5.pulse[i];
        // (periods under 8 are silent, as on the APU, and aren't worth running)
        if (p -> period >= 8) {
            p -> timer -= cycles;
            while (p -> timer <= 0) {
                p -> timer += (p -> period + 1) << 1;
                p -> step = (p -> step - 1) & 7;
            }
        }
    }
    mmc5.sequencer -= cycles;
    while (mmc5.sequencer <= 0) {
        mmc5.sequencer += MMC5_QUARTER_FRAME;
        for (uint32_t i = 0; i < 2; i++) {
            pulse_t* p = &mmc5.pulse[i];
            envelope_clock(&p -> envelope);
            if (p -> length && !p -> envelope.loop) {
                p -> length--;
            }
        }
    }
}

static inline int32_t mmc5_output() {
    uint32_t p0 = 0, p1 = 0;
    for (uint32_t i = 0; i < 2; i++) {
        pulse_t* p = &mmc5.pulse[i];
        if (p -> length && (p -> period >= 8) && ((duty_table[p -> duty] >> p -> step) & 1)) {
            if (i) {
                p1 = envelope_output(&p -> envelope);
            } else {
                p0 = envelope_output(&p -> envelope);
            }
        }
    }
    return (int32_t)(p0 + p1 + mmc5.pcm) * MMC5_SCALE;
}

static inline __attribute__((always_inline)) int32_t mmc5_output_opt(const uint32_t average) {
    uint32_t p0 = 0, p1 = 0;
    for (uint32_t i = 0; i < 2; i++) {
        pulse_t* p = &mmc5.pulse[i];
        if (p -> length && (p -> period >= 8) && ((duty_table[p -> duty] >> p -> step) & 1)) {
            if (i) {
                p1 = envelope_output(&p -> envelope);
            } else {
                p0 = envelope_output(&p -> envelope);
            }
        }
    }
    if (average && mmc5_average_cycles) {
        int32_t out = (int32_t)(((uint64_t)average_scale(mmc5_average_sum, mmc5_average_cycles) * MMC5_SCALE) >> 16) + mmc5.pcm * MMC5_SCALE;
        mmc5_average_sum = mmc5_average_cycles = 0;
        return out;
    }
    return (int32_t)(p0 + p1 + mmc5.pcm) * MMC5_SCALE;
}

/*
 * VRC7: a YM2413 (OPLL) derivative with six 2 operator FM channels and 15 fixed instruments plus a custom one.
 * $9010 selects a register, $9030 writes it: $00-$07 the custom instrument, $10-$15 F-number low bits, $20-$25
 * sustain / key on / block / F-number bit 8, $30-$35 instrument / volume. Written from the YM2413 / nesdev VRC7
 * descriptions with simplified envelope timing (exponential attack, linear decay / release in dB, rates doubling
 * every 4 steps): the pitches, instruments and envelope shapes follow the registers, the exact envelope timing and
 * the LFOs' depths are approximate. Run at the chip's own 49.7 kHz rate is not needed for this: the operators are
 * evaluated once per output sample (44.1 kHz) with the phase increments scaled to match.
 */
static const uint8_t vrc7_patches[16][8] = {
    { 0 },
    { 0x03, 0x21, 0x05, 0x06, 0xe8, 0x81, 0x42, 0x27 },
    { 0x13, 0x41, 0x14, 0x0d, 0xd8, 0xf6, 0x23, 0x12 },
    { 0x11, 0x11, 0x08, 0x08, 0xfa, 0xb2, 0x20, 0x12 },
    { 0x31, 0x61, 0x0c, 0x07, 0xa8, 0x64, 0x61, 0x27 },
    { 0x32, 0x21, 0x1e, 0x06, 0xe1, 0x76, 0x01, 0x28 },
    { 0x02, 0x01, 0x06, 0x00, 0xa3, 0xe2, 0xf4, 0xf4 },
    { 0x21, 0x61, 0x1d, 0x07, 0x82, 0x81, 0x11, 0x07 },
    { 0x23, 0x21, 0x22, 0x17, 0xa2, 0x72, 0x01, 0x17 },
    { 0x35, 0x11, 0x25, 0x00, 0x40, 0x73, 0x72, 0x01 },
    { 0xb5, 0x01, 0x0f, 0x0f, 0xa8, 0xa5, 0x51, 0x02 },
    { 0x17, 0xc1, 0x24, 0x07, 0xf8, 0xf8, 0x22, 0x12 },
    { 0x71, 0x23, 0x11, 0x06, 0x65, 0x74, 0x18, 0x16 },
    { 0x01, 0x02, 0xd3, 0x05, 0xc9, 0x95, 0x03, 0x02 },
    { 0x61, 0x63, 0x0c, 0x00, 0x94, 0xc0, 0x33, 0xf6 },
    { 0x21, 0x72, 0x0d, 0x00, 0xc1, 0xd5, 0x56, 0x06 },
};

#define OPLL_EG_ATTACK  0
#define OPLL_EG_DECAY   1
#define OPLL_EG_SUSTAIN 2
#define OPLL_EG_RELEASE 3
#define OPLL_EG_OFF     4
// envelope level in 1/65536 of 0.375 dB steps, 0 loudest, 128 (48 dB) and over silent
#define OPLL_EG_MAX     (128 << 16)

typedef struct {
    uint32_t phase;          // 10.22 bits of the sine table index
    uint32_t eg_level;
    uint8_t eg_state;
    int32_t output[2];       // last two outputs (modulator feedback)
} opll_operator_t;

// what each channel's registers and patch give, worked out when they're written rather than every sample
typedef struct {
    uint8_t valid;
    const uint8_t* patch;
    uint32_t ksr_offset;
    uint32_t increment[2];   // phase increment per chip sample (before vibrato)
    uint32_t step[2];        // the same per output sample, in 10.22 bits
    uint32_t car_base, mod_base;   // total level + key scale level, in 0.375 dB
} vrc7_channel_t;

static struct {
    uint8_t selected;
    uint8_t registers[0x40];
    opll_operator_t op[6][2];
    uint8_t key[6];
    uint32_t lfo_phase;
    vrc7_channel_t channel[6];
} vrc7;
// the VRC7 span's output and the two tables it reads every sample, in one block: the operand cache is direct mapped, and
// apart they could share lines' places and evict each other (7000 misses a frame in Lagrange Point)
#define VRC7_SPAN_MAX 1024
static struct {
    int32_t span[VRC7_SPAN_MAX];
    int16_t sine[1024];         // full wave, +-4095
    uint16_t attenuation[512];  // 2^(-n * 0.375 dB) as 1/4096ths
} opll_block __attribute__((aligned(32)));
#define opll_sine opll_block.sine
#define opll_attenuation opll_block.attenuation
#define vrc7_span opll_block.span
static const uint8_t opll_multiple[16] = { 1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30 };   // x2
// key scale level attenuation by the F-number's top 4 bits (in 0.75 dB), before the block term
static const uint8_t opll_ksl[16] = { 0, 24, 32, 37, 40, 43, 45, 47, 48, 50, 51, 52, 53, 54, 55, 56 };

static void opll_init_tables() {
    for (uint32_t i = 0; i < 1024; i++) {
        opll_sine[i] = (int16_t)(sinf((float)i * 6.2831853f / 1024.0f) * 4095.0f);
    }
    for (uint32_t i = 0; i < 512; i++) {
        opll_attenuation[i] = (uint16_t)(4096.0f * powf(10.0f, -(float)i * 0.375f / 20.0f));
    }
}

static inline const uint8_t* vrc7_patch(uint32_t channel) {
    uint32_t instrument = vrc7.registers[0x30 + channel] >> 4;
    return instrument ? vrc7_patches[instrument] : vrc7.registers;
}

static FRNES_COLD void vrc7_write(uint16_t address, uint8_t value) {
    if (address == 0x9010) {
        vrc7.selected = value & 0x3f;
        return;
    }
    uint32_t reg = vrc7.selected;
    vrc7.registers[reg] = value;
    if (reg < 8) {
        // the custom instrument: any channel can use it
        for (uint32_t c = 0; c < 6; c++) {
            vrc7.channel[c].valid = 0;
        }
    } else if ((reg >= 0x10) && ((reg & 15) < 6)) {
        vrc7.channel[reg & 15].valid = 0;
    }
    if ((reg >= 0x20) && (reg <= 0x25)) {
        uint32_t channel = reg - 0x20;
        uint8_t key = (value >> 4) & 1;
        if (key && !vrc7.key[channel]) {
            for (uint32_t o = 0; o < 2; o++) {
                vrc7.op[channel][o].eg_state = OPLL_EG_ATTACK;
                vrc7.op[channel][o].phase = 0;
            }
        } else if (!key && vrc7.key[channel]) {
            for (uint32_t o = 0; o < 2; o++) {
                if (vrc7.op[channel][o].eg_state != OPLL_EG_OFF) {
                    vrc7.op[channel][o].eg_state = OPLL_EG_RELEASE;
                }
            }
        }
        vrc7.key[channel] = key;
    }
}

// (48 dB of decay or release at rate 4, each 4 steps of rate twice as fast: fitted to Mesen 2's VRC7, measured with
// the per instrument test ROM (perf_runs/vrc7_vs_mesen): its slopes are ~7 dB/s at rate 10. The attack keeps the
// time it had (OPLL_ATTACK_SCALE), which matched)
#define OPLL_DECAY_SECONDS 20.0f
#define OPLL_ATTACK_SCALE(step) (((step) * 50) / 9)
// envelope increment per output sample for a 0-63 rate (the level falls 48 dB in OPLL_DECAY_SECONDS at rate 4, each 4
// steps of rate twice as fast)
// (envelopes are updated every OPLL_EG_EVERY samples, by that many samples' worth)
#define OPLL_EG_EVERY 16
static inline uint32_t opll_rate_step(uint32_t rate) {
    if (rate == 0) {
        return 0;
    }
    if (rate > 63) {
        rate = 63;
    }
    uint32_t base = OPLL_EG_EVERY * (128u << 16) / (uint32_t)(OPLL_DECAY_SECONDS * 44100.0f);   // at rate 4
    return (base * (4 + (rate & 3)) << (rate >> 2)) >> 3;
}

static FRNES_HOT(timer_vrc7_envelope) void opll_envelope(opll_operator_t* op, const uint8_t* patch, uint32_t which, uint32_t channel, uint32_t ksr_offset) {
    uint32_t ar = patch[4 + which] >> 4, dr = patch[4 + which] & 15;
    uint32_t sl = patch[6 + which] >> 4, rr = patch[6 + which] & 15;
    uint32_t sustained = (patch[which] >> 5) & 1;
    uint32_t ksr = (patch[which] & 0x10) ? ksr_offset : (ksr_offset >> 2);
    switch (op -> eg_state) {
        case OPLL_EG_ATTACK:
            if (ar == 15) {
                op -> eg_level = 0;
            } else if (ar) {
                // exponential: the step shrinks as the level approaches 0
                uint32_t step = OPLL_ATTACK_SCALE(opll_rate_step(ar * 4 + ksr)) << 3;
                uint64_t fall = ((uint64_t)op -> eg_level * step) >> 22;
                op -> eg_level = (fall + 1 >= op -> eg_level) ? 0 : op -> eg_level - (uint32_t)fall - 1;
            }
            if (op -> eg_level == 0) {
                op -> eg_state = OPLL_EG_DECAY;
            }
            break;
        case OPLL_EG_DECAY:
            op -> eg_level += opll_rate_step(dr * 4 + ksr);
            if (op -> eg_level >= ((sl * 8) << 16)) {
                op -> eg_level = (sl * 8) << 16;
                op -> eg_state = OPLL_EG_SUSTAIN;
            }
            break;
        case OPLL_EG_SUSTAIN:
            // percussive instruments keep falling at the release rate, sustained ones hold
            if (!sustained) {
                op -> eg_level += opll_rate_step(rr * 4 + ksr);
            }
            break;
        case OPLL_EG_RELEASE: {
            // the channel's sustain bit makes the release slow
            uint32_t rate = (vrc7.registers[0x20 + channel] & 0x20) ? 5 : (sustained ? rr : 7);
            op -> eg_level += opll_rate_step(rate * 4 + ksr);
        } break;
        default:
            break;
    }
    if (op -> eg_level >= OPLL_EG_MAX) {
        op -> eg_level = OPLL_EG_MAX;
        if (op -> eg_state >= OPLL_EG_SUSTAIN) {
            op -> eg_state = OPLL_EG_OFF;
        }
    }
}

// whether opll_envelope would leave the operator as it is (until a write changes its state): off, or in sustain on a
// sustained instrument below the silent level
static inline uint32_t vrc7_eg_steady(const opll_operator_t* op, const uint8_t* patch, uint32_t which) {
    return (op -> eg_state == OPLL_EG_OFF) ||
        ((op -> eg_state == OPLL_EG_SUSTAIN) && ((patch[which] >> 5) & 1) && (op -> eg_level < OPLL_EG_MAX));
}

// a channel's cached values (vrc7_channel_t) from its registers and patch
static __attribute__((noinline)) void vrc7_channel_update(uint32_t c) {
    vrc7_channel_t* ch = &vrc7.channel[c];
    const uint8_t* patch = vrc7_patch(c);
    uint32_t fnum = vrc7.registers[0x10 + c] | ((vrc7.registers[0x20 + c] & 1) << 8);
    uint32_t block = (vrc7.registers[0x20 + c] >> 1) & 7;
    ch -> patch = patch;
    ch -> ksr_offset = (block << 1) | (fnum >> 8);
    // key scale level (0.75 dB units) from the F-number's top bits and the block
    int32_t ksl_base = opll_ksl[fnum >> 5] - 8 * (7 - block);
    if (ksl_base < 0) {
        ksl_base = 0;
    }
    for (uint32_t o = 0; o < 2; o++) {
        // phase increment per 49716 Hz chip sample: F-number << block x multiple / 2 in 1/2^19 of a cycle; per
        // 44.1 kHz output sample, and in 10.22 bits: x 49716 / 44100 x 2^13 = x 9235.2, as (x * 591054) >> 6
        uint32_t increment = ((fnum << block) * opll_multiple[patch[o] & 15]) >> 1;
        ch -> increment[o] = increment;
        ch -> step[o] = (uint32_t)(((uint64_t)increment * 591054u) >> 6);
    }
    ch -> car_base = (vrc7.registers[0x30 + c] & 15) * 8 + ((ksl_base * (patch[3] >> 6)) >> 1);
    ch -> mod_base = (patch[2] & 0x3f) * 2 + ((ksl_base * (patch[2] >> 6)) >> 1);
    ch -> valid = 1;
}

// VRC7 output in spans: the samples up to the next logged VRC7 write (or the frame's end) are worked out one channel at
// a time, which keeps a channel's state in registers; the same samples as one at a time (no write lands inside a span)
static uint32_t vrc7_span_pos, vrc7_span_length, vrc7_eg_tick;
// (apu_synth_frame's end_cycle, for the span's length)
static uint32_t synth_frame_end;

// a slow sine for vibrato (6.4 Hz), stepped every sample
#define VRC7_LFO_STEP ((uint32_t)(6.4f * 4294967296.0f / 44100.0f))
// tremolo: the YM2413's AM LFO, 3.7 Hz, a triangle from 0 to 4.8 dB (13 of the envelope's 0.375 dB steps), in 64 steps
// a cycle; its phase is the output sample count's (vrc7_eg_tick) x VRC7_AM_STEP, so it needs no state of its own
#define VRC7_AM_STEP ((uint32_t)(3.7f * 4294967296.0f / 44100.0f))
#define VRC7_AM_DEPTH 13
static inline uint32_t vrc7_am_level(uint32_t index) {
    uint32_t triangle = (index < 32) ? index : 63 - index;
    return (triangle * VRC7_AM_DEPTH + 15) / 31;
}

static void vrc7_render(uint32_t samples) {
    // (each channel steps its own copies of the LFO and the envelope tick from here; the same values for all of them)
    const uint32_t lfo_start = vrc7.lfo_phase, tick_start = vrc7_eg_tick;
    vrc7.lfo_phase += VRC7_LFO_STEP * samples;
    vrc7_eg_tick += samples;
    memset(vrc7_span, 0, samples * sizeof(vrc7_span[0]));
    for (uint32_t c = 0; c < 6; c++) {
        opll_operator_t* mod = &vrc7.op[c][0];
        opll_operator_t* car = &vrc7.op[c][1];
        if ((mod -> eg_state == OPLL_EG_OFF) && (car -> eg_state == OPLL_EG_OFF)) {
            // (only a key on brings it back, and no write lands inside a span)
            continue;
        }
        vrc7_channel_t* ch = &vrc7.channel[c];
        if (__builtin_expect(!ch -> valid, 0)) {
            vrc7_channel_update(c);
        }
        // (in locals for the span: the patch can be the registers, and a byte pointer could alias every store)
        const uint8_t* patch = ch -> patch;
        const uint32_t vibrato_mod = patch[0] & 0x40, vibrato_car = patch[1] & 0x40;
        // (as masks, so the loop has no branches on them)
        const uint32_t tremolo_mod = (patch[0] & 0x80) ? 0xffffffff : 0, tremolo_car = (patch[1] & 0x80) ? 0xffffffff : 0;
        const uint32_t fb = patch[3] & 7, feedback_mask = fb ? 0xffffffff : 0, feedback_shift = fb ? 9 - fb : 0;
        const int32_t rectify_mod = (patch[3] & 0x08) ? -1 : 0, rectify_car = (patch[3] & 0x10) ? -1 : 0;
        const uint32_t step_mod = ch -> step[0], step_car = ch -> step[1];
        const uint32_t increment_mod = ch -> increment[0], increment_car = ch -> increment[1];
        const uint32_t car_base = ch -> car_base, mod_base = ch -> mod_base, ksr_offset = ch -> ksr_offset;
        uint32_t mod_phase = mod -> phase, car_phase = car -> phase;
        int32_t mod_out0 = mod -> output[0], mod_out1 = mod -> output[1];
        uint32_t lfo_phase = lfo_start, tick = tick_start;
        // what changes only with the LFO's table index (every ~7 samples) or at an envelope tick, kept between them
        uint32_t lfo_index = 0xffffffff, step_mod_now = step_mod, step_car_now = step_car, tremolo = 0, am_index = 0xffffffff;
        uint32_t mod_eg = mod -> eg_level >> 16, car_eg = car -> eg_level >> 16;
        uint32_t off = 0;
        // an operator whose envelope can't move (off, or holding a sustained instrument's sustain level) until a write,
        // which ends the span: opll_envelope would leave it as it is, so it isn't called
        const uint32_t mod_steady = vrc7_eg_steady(mod, patch, 0), car_steady = vrc7_eg_steady(car, patch, 1);
        const uint32_t eg_moving = !(mod_steady && car_steady);
        // (one sample's operators: the phases step, and the carrier, unless it's too far down to hear, phase modulated by
        // the modulator, goes into the span)
#define VRC7_SAMPLE(i) do { \
            mod_phase += step_mod_now; \
            car_phase += step_car_now; \
            /* a carrier 54 dB or more down isn't worth the operator math (games key channels on at volume 15) */ \
            uint32_t car_level = car_eg + car_base; \
            if (car_level < 128) { \
                /* modulator (its level is at most 128 + 126 + 84 + 13: always inside the attenuation table) */ \
                uint32_t mod_level = mod_eg + mod_base + (tremolo & tremolo_mod); \
                int32_t feedback = ((mod_out0 + mod_out1) >> feedback_shift) & feedback_mask; \
                int32_t value = opll_sine[((mod_phase >> 22) + feedback) & 1023]; \
                value &= ~((value >> 31) & rectify_mod); \
                int32_t mod_out = (value * opll_attenuation[mod_level]) >> 12; \
                mod_out1 = mod_out0; \
                mod_out0 = mod_out; \
                /* carrier, phase modulated by the modulator: its +-4095 is +-8 pi (the sine table's 1024 entries a cycle; */ \
                /* +-4 pi before, which left every instrument duller than Mesen 2's: perf_runs/vrc7_vs_mesen); under 144 + 13 */ \
                car_level += tremolo & tremolo_car; \
                value = opll_sine[((car_phase >> 22) + mod_out) & 1023]; \
                value &= ~((value >> 31) & rectify_car); \
                vrc7_span[i] += (value * opll_attenuation[car_level]) >> 12; \
            } \
        } while (0)
        for (uint32_t i = 0; i < samples; ) {
            // a sample with the events that can land on it: the LFO's next table index, then the envelope tick
            lfo_phase += VRC7_LFO_STEP;
            if ((lfo_phase >> 22) != lfo_index) {
                lfo_index = lfo_phase >> 22;
                const int32_t lfo = opll_sine[lfo_index];
                if (vibrato_mod) {
                    // vibrato: +-7 cents or so
                    uint32_t increment = increment_mod + ((int32_t)(increment_mod * lfo) >> 19);
                    step_mod_now = (uint32_t)(((uint64_t)increment * 591054u) >> 6);
                }
                if (vibrato_car) {
                    uint32_t increment = increment_car + ((int32_t)(increment_car * lfo) >> 19);
                    step_car_now = (uint32_t)(((uint64_t)increment * 591054u) >> 6);
                }
            }
            {
                uint32_t index = ((tick_start + i) * VRC7_AM_STEP) >> 26;
                if (index != am_index) {
                    am_index = index;
                    tremolo = vrc7_am_level(index);
                }
            }
            if (off) {
                break;
            }
            if (eg_moving && ((++tick % OPLL_EG_EVERY) == 0)) {
                // (only here can the channel go off: it stops at the next sample)
                if (!mod_steady) {
                    opll_envelope(mod, patch, 0, c, ksr_offset);
                }
                if (!car_steady) {
                    opll_envelope(car, patch, 1, c, ksr_offset);
                }
                mod_eg = mod -> eg_level >> 16;
                car_eg = car -> eg_level >> 16;
                off = (mod -> eg_state == OPLL_EG_OFF) && (car -> eg_state == OPLL_EG_OFF);
            }
            VRC7_SAMPLE(i);
            i++;
            // then the samples up to the next event, none landing on them: the LFO's index stays (its phase stays under
            // the next index's start) and no tick comes (the tick count stays off a multiple of OPLL_EG_EVERY)
            uint32_t run = off ? 0 : samples - i;
            // (the vibrato LFO's next index ends the run only on a channel with vibrato: tremolo has its own LFO)
            if (vibrato_mod | vibrato_car) {
                uint32_t to_lfo = (uint32_t)((((uint64_t)lfo_index + 1) << 22) - 1 - lfo_phase) / VRC7_LFO_STEP;
                if (to_lfo < run) {
                    run = to_lfo;
                }
            }
            {
                // (the samples from i on with the AM LFO's index unchanged)
                uint32_t am_phase = (tick_start + i) * VRC7_AM_STEP;
                uint32_t to_am = ((am_phase >> 26) != am_index) ? 0 :
                    (uint32_t)(((((uint64_t)am_index + 1) << 26) - am_phase + VRC7_AM_STEP - 1) / VRC7_AM_STEP);
                if (to_am < run) {
                    run = to_am;
                }
            }
            if (eg_moving) {
                uint32_t to_tick = (OPLL_EG_EVERY - 1) - (tick % OPLL_EG_EVERY);
                if (to_tick < run) {
                    run = to_tick;
                }
                tick += run;
            }
            lfo_phase += VRC7_LFO_STEP * run;
            for (uint32_t end = i + run; i < end; i++) {
                VRC7_SAMPLE(i);
            }
        }
#undef VRC7_SAMPLE
        mod -> phase = mod_phase;
        car -> phase = car_phase;
        mod -> output[0] = mod_out0;
        mod -> output[1] = mod_out1;
    }
}

// the samples from the current one to the next logged VRC7 write or the frame's end (as apu_synth_frame steps them)
#ifdef VRC7_PERF
// diagnostic (test builds with AC_PERF_EVENT): the performance counter's count inside vrc7_render, and its samples
#include <dc/perfctr.h>
uint64_t vrc7_perf_count;
uint32_t vrc7_perf_samples;
#endif

static FRNES_HOT(timer_vrc7_span_fill) void vrc7_span_fill() {
    uint32_t write_pending = 0, write_cycle = 0;
    for (uint32_t j = log_head; j != log_tail; j = (j + 1) & (LOG_SIZE - 1)) {
        // (only a data write changes what the chip plays: $9010 selects the register $9030 writes, read when that comes)
        if (write_log[j].address == 0x9030) {
            write_pending = 1;
            write_cycle = write_log[j].cycle;
            break;
        }
    }
    uint32_t samples = 1, target = synth_cycle, fraction = sample_fraction;
    while ((samples < VRC7_SPAN_MAX) && ((int32_t)(synth_frame_end - target) > 0)) {
        fraction += cycles_per_sample_fp;
        uint32_t next = target + (fraction >> 16);
        fraction &= 0xffff;
        if (write_pending && ((int32_t)(write_cycle - next) <= 0)) {
            break;
        }
        target = next;
        samples++;
    }
#ifdef VRC7_PERF
    uint64_t perf_start = perf_cntr_count(PRFC1);
    vrc7_render(samples);
    vrc7_perf_count += perf_cntr_count(PRFC1) - perf_start;
    vrc7_perf_samples += samples;
#else
    vrc7_render(samples);
#endif
    vrc7_span_pos = 0;
    vrc7_span_length = samples;
}

static inline int32_t vrc7_output() {
    if (vrc7_span_pos == vrc7_span_length) {
        vrc7_span_fill();
    }
    return vrc7_span[vrc7_span_pos++];
}

/*
 * Namco 163: the chip's 128 bytes of RAM ($4800 writes logged as $4800 + address) hold the waveforms (4 bit samples,
 * the low nibble first) and, from $40, eight channels' registers: frequency (18 bit, +0 / +2 / +4 bits 0-1), phase
 * (24 bit, +1 / +3 / +5), wave length (256 - (+4 & $FC) samples), wave address (+6, in samples) and volume (+7 bits
 * 0-3). $7F bits 4-6 + 1 channels are enabled, the highest numbered ones. Every 15 CPU cycles the chip updates one
 * channel, from 7 down: phase += frequency (modulo length << 16), sample = wave[(phase >> 16) + address], and
 * outputs (sample - 8) * volume until the next channel's update, so the channels are time-multiplexed on the one
 * output. Each audio sample is that output averaged over the CPU cycles it covers. $E000 bit 6 halts the chip.
 */
typedef struct {
    // decoded from the registers on each write; the phase is kept here (the CPU can't read the synth's copy of the RAM)
    uint32_t frequency, length, phase;
    uint8_t address, volume;
} n163_channel_t;
static struct {
    uint8_t ram[128];
    // the RAM as 256 4 bit samples
    uint8_t wave[256];
    n163_channel_t channels[8];
    uint8_t channel, last, halt, timer;
    int32_t output;
    int32_t sum, sum_cycles;
} n163 = { .channel = 7, .last = 7, .timer = 15 };
// 65536 / cycles, for averaging the output over a sample (about 41 CPU cycles); signed, the sum is negative half the time
static int32_t n163_reciprocal[64];


#ifdef FRNES_FPU_N163
#include "fpu_synth/fpu_synth.c"
/*
 * The FPU paths' buffers, from the heap while an N163 or FDS game runs (apu_synth_set_expansion): in .bss they moved
 * .bss's end, so the heap's start and every buffer allocated after it, in every game (abFPU, 2026-11-10: SMB's
 * emulation +1-2 % on 3 of 6 layouts)
 */
typedef struct {
    float n163_wave[512];
    float n163_span[1024];
    fsyn_bank_t n163_bank;
} fpu_spans_t;
static fpu_spans_t* fpu_spans;
static void output_options_slow_update(void);
static void fpu_spans_rebuild(void);
// (the FPU paths' buffers only while their chip plays: apu_synth_init, after the mapper set the chip, or the chip's
// first sample (an NSF's); freed by the next game's apu_synth_init for another chip (not by apu_synth_shutdown, which is
// inlined into the frame loop's function: it would change that function's code). Out of line and cold:
// apu_synth_set_expansion, in the frame slot, stays as it was)
static FRNES_COLD_TEXT __attribute__((noinline)) void fpu_spans_update(uint32_t chip) {
    uint32_t wanted = 0;
#ifdef FRNES_FPU_N163
    wanted |= chip == APU_EXPANSION_N163;
#endif
    if (wanted && !fpu_spans) {
#ifdef FRNES_HOST
        fpu_spans = aligned_alloc(32, (sizeof(fpu_spans_t) + 31) & ~31u);
#else
        fpu_spans = memalign(32, sizeof(fpu_spans_t));
#endif
        if (fpu_spans) {
            memset(fpu_spans, 0, sizeof(fpu_spans_t));
            fpu_spans_rebuild();
            // (the N163's frame from now on: apu_synth_frame's slow path, apu_synth_frame_n163)
            output_options_slow_update();
        } else {
            printf("apu synth: no memory for the FPU synth's buffers (%u bytes): expansion sound off\n", (unsigned)sizeof(fpu_spans_t));
        }
    } else if (!wanted && fpu_spans) {
        free(fpu_spans);
        fpu_spans = NULL;
        output_options_slow_update();
    }
}
#endif
#ifdef FRNES_FPU_N163
/*
 * The N163 through the FPU synth prototype (emulator/fpu_synth, perf_runs/fpu_synth): instead of the multiplexed output
 * (one channel at a time, 15 CPU cycles each), each enabled channel is played at the output rate (its phase stepping
 * frequency x the CPU cycles a sample covers / (15 x the channels enabled), as often as the chip updates it) and the
 * channels averaged: the multiplexed output's average without its buzz at 1.79 MHz / (15 x channels). Spans between the
 * chip's logged writes, a channel at a time (fsyn_render_accumulate)
 */
#define N163_SPAN_MAX 1024
// the RAM's samples (-8 to 7) twice over, so a wave running past $FF reads on
#define n163_wave_f (fpu_spans -> n163_wave)
#define n163_span (fpu_spans -> n163_span)
static uint32_t n163_span_pos, n163_span_length;
#define n163_bank (fpu_spans -> n163_bank)

static void n163_fpu_ram(uint32_t index, uint8_t value) {
    if (!fpu_spans) {
        return;
    }
    float lo = (float)((int32_t)(value & 15) - 8), hi = (float)((int32_t)(value >> 4) - 8);
    n163_wave_f[index * 2] = n163_wave_f[index * 2 + 256] = lo;
    n163_wave_f[index * 2 + 1] = n163_wave_f[index * 2 + 257] = hi;
}

static void n163_span_fill(void) {
    uint32_t write_pending = 0, write_cycle = 0;
    for (uint32_t j = log_head; j != log_tail; j = (j + 1) & (LOG_SIZE - 1)) {
        if (((write_log[j].address & 0xff80) == 0x4800) || (write_log[j].address == 0xe000)) {
            write_pending = 1;
            write_cycle = write_log[j].cycle;
            break;
        }
    }
    uint32_t samples = 1, target = synth_cycle, fraction = sample_fraction;
    while ((samples < N163_SPAN_MAX) && ((int32_t)(synth_frame_end - target) > 0)) {
        fraction += cycles_per_sample_fp;
        uint32_t next = target + (fraction >> 16);
        fraction &= 0xffff;
        if (write_pending && ((int32_t)(write_cycle - next) <= 0)) {
            break;
        }
        target = next;
        samples++;
    }
    memset(n163_span, 0, samples * sizeof(float));
    if (!n163.halt) {
        uint32_t count = 8 - n163.last;
        fsyn_bank_t* bank = &n163_bank;
        bank -> count = count;
        bank -> outputs = 1;
        for (uint32_t i = 0; i < count; i++) {
            n163_channel_t* c = &n163.channels[7 - i];
            fsyn_channel_t* f = &bank -> ch[i];
            f -> wave = &n163_wave_f[c -> address];
            f -> length = c -> length >> 16;
            f -> phase = c -> phase;
            f -> mod_source = -1;
            // (16.16 sample steps: frequency per chip update x the updates a sample covers)
            f -> step = (uint32_t)(((uint64_t)c -> frequency * cycles_per_sample_fp) / (15u * count * 65536u));
            bank -> matrix[(i >> 2) * 2][i & 3] = (float)c -> volume / (float)count;
            bank -> matrix[(i >> 2) * 2 + 1][i & 3] = 0;
        }
        fsyn_render_accumulate(bank, n163_span, samples);
        for (uint32_t i = 0; i < count; i++) {
            n163.channels[7 - i].phase = bank -> ch[i].phase;
        }
    }
    n163_span_pos = 0;
    n163_span_length = samples;
}
#endif

static FRNES_COLD void n163_write(uint16_t address, uint8_t value) {
    if ((address & 0xff80) == 0x4800) {
        uint32_t index = address & 0x7f;
        n163.ram[index] = value;
#ifdef FRNES_FPU_N163
        n163_fpu_ram(index, value);
#endif
        n163.wave[index * 2] = value & 15;
        n163.wave[index * 2 + 1] = value >> 4;
        if (index >= 0x40) {
            n163_channel_t* c = &n163.channels[(index >> 3) & 7];
            const uint8_t* r = &n163.ram[index & 0x78];
            switch (index & 7) {
                case 0: case 2: c -> frequency = r[0] | (r[2] << 8) | ((r[4] & 3) << 16); break;
                case 1: case 3: case 5: c -> phase = r[1] | (r[3] << 8) | (r[5] << 16); break;
                case 4:
                    c -> frequency = r[0] | (r[2] << 8) | ((r[4] & 3) << 16);
                    c -> length = (256 - (r[4] & 0xfc)) << 16;
                    break;
                case 6: c -> address = r[6]; break;
                case 7:
                    c -> volume = r[7] & 15;
                    if (index == 0x7f) {
                        n163.last = 7 - ((value >> 4) & 7);
                    }
                    break;
            }
        }
    } else if (address == 0xe000) {
        n163.halt = (value >> 6) & 1;
    }
}

static inline void n163_run(int32_t cycles) {
    n163.sum_cycles += cycles;
    if (n163.halt) {
        n163.sum += n163.output * cycles;
        return;
    }
    int32_t timer = n163.timer;
    if (cycles < timer) {
        n163.sum += n163.output * cycles;
        n163.timer = timer - cycles;
        return;
    }
    // (in locals while the channels are updated, about 3 per audio sample)
    int32_t sum = n163.sum;
    int32_t output = n163.output;
    uint32_t channel = n163.channel;
    const uint32_t last = n163.last;
    const uint8_t* wave = n163.wave;
    do {
        sum += output * timer;
        cycles -= timer;
        timer = 15;
        n163_channel_t* c = &n163.channels[channel];
        uint32_t phase = c -> phase + c -> frequency;
        uint32_t length = c -> length;
        while (__builtin_expect(phase >= length, 0)) {
            phase -= length;
        }
        c -> phase = phase;
        output = ((int32_t)wave[((phase >> 16) + c -> address) & 0xff] - 8) * c -> volume;
        channel = (channel <= last) ? 7 : channel - 1;
    } while (cycles >= timer);
    n163.sum = sum + output * cycles;
    n163.output = output;
    n163.channel = channel;
    n163.timer = timer - cycles;
}

static inline int32_t n163_output() {
    int32_t out = (n163.sum_cycles < 64) ? (n163.sum * n163_reciprocal[n163.sum_cycles]) >> 16 : n163.sum / n163.sum_cycles;
    n163.sum = 0;
    n163.sum_cycles = 0;
    return out;
}

#ifdef FRNES_FPU_N163
// the N163's sample through the FPU synth (its spans): only the FPU N163 frame calls it (apu_synth_frame_n163), so the
// standard frame's loop keeps b28ab3cd's code
static __attribute__((noinline)) int32_t n163_output_fpu() {
    if (!fpu_spans) {
        return 0;
    }
    if (n163_span_pos == n163_span_length) {
        n163_span_fill();
    }
    return (int32_t)n163_span[n163_span_pos++];
}
#endif


/*
 * Famicom Disk System sound (the 2C33's wavetable channel, from nesdev's "FDS audio"): a 64 step table of 6 bit
 * samples ($4040-$407F, writable while $4089 bit 7 is set, which also holds the output), played by a 24 bit
 * accumulator whose top 6 bits are the position, adding the modulated pitch every 16 CPU cycles. The modulator adds its
 * 12 bit pitch to an 18 bit accumulator every 16 cycles too; each carry out of bit 11 applies the mod table entry at
 * the position in bits 12-17 (32 entries, each twice) to a 7 bit signed counter, which bends the pitch through the
 * counter * mod gain formula. Two envelopes (volume, mod gain) tick every 8 * (speed + 1) * ($408A + 1) cycles. Volume
 * gain changes reach the output when the wave position is 0; the output is sample * min(gain, 32) * master volume
 * (1, 2/3, 2/4, 2/5), through a 1 pole lowpass at about 2 kHz.
 */
static const int8_t fds_mod_adjust[8] = { 0, 1, 2, 4, 0, -4, -2, -1 };
// (Mesen 2's WaveVolumeTable)
static const uint8_t fds_master_volume[4] = { 36, 24, 17, 14 };
static struct {
    uint8_t wave[64], mod_table[32];
    uint16_t pitch, mod_pitch;
    uint8_t wave_halt, env_halt, mod_halt, mod_force;
    uint8_t vol_off, vol_increase, vol_speed, vol_gain, vol_output_gain;
    uint8_t mod_off, mod_increase, mod_speed, mod_gain;
    int8_t mod_counter;
    uint8_t write_enable, master, env_master;
    uint32_t wave_acc, mod_acc;
    int32_t vol_timer, mod_timer, tick;
    int32_t output, sum, sum_cycles, filtered;
} fds = { .env_master = 0xe8, .tick = 16 };


static int32_t fds_env_period(uint8_t speed) {
    return 8 * (speed + 1) * (fds.env_master + 1);
}

static void fds_write(uint16_t address, uint8_t value) {
    if ((address >= 0x4040) && (address < 0x4080)) {
        if (fds.write_enable) {
            fds.wave[address & 63] = value & 63;
        }
        return;
    }
    switch (address) {
        case 0x4080:
            fds.vol_speed = value & 63;
            fds.vol_increase = (value >> 6) & 1;
            fds.vol_off = value >> 7;
            if (fds.vol_off) {
                fds.vol_gain = value & 63;
                if (!fds.vol_gain) {
                    // (muting takes effect right away)
                    fds.vol_output_gain = 0;
                }
            }
            fds.vol_timer = fds_env_period(fds.vol_speed);
            break;
        case 0x4082:
            fds.pitch = (fds.pitch & 0xf00) | value;
            break;
        case 0x4083:
            fds.pitch = (fds.pitch & 0x0ff) | ((value & 15) << 8);
            fds.env_halt = (value >> 6) & 1;
            fds.wave_halt = value >> 7;
            if (fds.wave_halt) {
                fds.wave_acc = 0;
            }
            if (fds.env_halt) {
                fds.vol_timer = fds_env_period(fds.vol_speed);
                fds.mod_timer = fds_env_period(fds.mod_speed);
            }
            break;
        case 0x4084:
            fds.mod_speed = value & 63;
            fds.mod_increase = (value >> 6) & 1;
            fds.mod_off = value >> 7;
            if (fds.mod_off) {
                fds.mod_gain = value & 63;
            }
            fds.mod_timer = fds_env_period(fds.mod_speed);
            break;
        case 0x4085:
            // (7 bit signed)
            fds.mod_counter = (int8_t)((value & 0x7f) << 1) >> 1;
            break;
        case 0x4086:
            fds.mod_pitch = (fds.mod_pitch & 0xf00) | value;
            break;
        case 0x4087:
            fds.mod_pitch = (fds.mod_pitch & 0x0ff) | ((value & 15) << 8);
            fds.mod_force = (value >> 6) & 1;
            fds.mod_halt = value >> 7;
            if (fds.mod_halt) {
                fds.mod_acc &= ~0x1fffu;
            }
            break;
        case 0x4088:
            if (fds.mod_halt) {
                fds.mod_table[(fds.mod_acc >> 13) & 31] = value & 7;
                fds.mod_acc = (fds.mod_acc + 0x2000) & 0x3ffff;
            }
            break;
        case 0x4089:
            fds.write_enable = value >> 7;
            fds.master = value & 3;
            break;
        case 0x408a:
            fds.env_master = value;
            break;
    }
}

static void fds_envelope_tick(uint8_t increase, uint8_t* gain) {
    if (increase) {
        if (*gain < 32) {
            (*gain)++;
        }
    } else if (*gain > 0) {
        (*gain)--;
    }
}

// the wave's pitch for a tick: the pitch bent by the mod counter * mod gain (nesdev's formula)
static inline uint32_t fds_wave_pitch(int32_t counter, uint32_t mod_gain, uint32_t pitch) {
    int32_t temp = (counter * (int32_t)mod_gain) & 0xfff;
    if ((temp & 0x0f) && !(temp & 0x800)) {
        temp += 0x20;
    }
    temp = ((temp + 0x400) >> 4) & 0xff;
    return (pitch * (uint32_t)temp) & 0xfffff;
}

/*
 * The span between two samples (or a sample and a register write): the envelopes, then a tick of the wave and mod
 * units every 16 cycles, with the state in locals (the wave pitch only worked out again when the mod counter moves). Not inlined into
 * expansion_run, where sharing a frame with the other chips' code spilled it to the stack (~100 instructions a tick).
 * The output is the same sample for sample (AC_SYNTH_HASH).
 */
static __attribute__((noinline)) void fds_run(int32_t cycles) {
    if (fds.env_master && !fds.env_halt && !fds.wave_halt) {
        if (!fds.vol_off) {
            fds.vol_timer -= cycles;
            while (fds.vol_timer <= 0) {
                fds.vol_timer += fds_env_period(fds.vol_speed);
                fds_envelope_tick(fds.vol_increase, &fds.vol_gain);
            }
        }
        if (!fds.mod_off) {
            fds.mod_timer -= cycles;
            while (fds.mod_timer <= 0) {
                fds.mod_timer += fds_env_period(fds.mod_speed);
                fds_envelope_tick(fds.mod_increase, &fds.mod_gain);
            }
        }
    }
    fds.sum_cycles += cycles;
    int32_t tick = fds.tick;
    if (cycles < tick) {
        fds.sum += fds.output * cycles;
        fds.tick = tick - cycles;
        return;
    }
    // (constant over the span: register writes and envelope ticks come between calls)
    const uint32_t mod_on = !fds.mod_halt && (fds.mod_pitch || fds.mod_force);
    const uint32_t mod_pitch = fds.mod_pitch, mod_force = fds.mod_force, mod_gain = fds.mod_gain, pitch = fds.pitch;
    const uint32_t wave_on = !fds.wave_halt, write_enable = fds.write_enable, vol_gain = fds.vol_gain;
    const int32_t master = fds_master_volume[fds.master];
    uint32_t mod_acc = fds.mod_acc, wave_acc = fds.wave_acc, vol_output_gain = fds.vol_output_gain;
    int32_t counter = fds.mod_counter, output = fds.output, sum = fds.sum;
    uint32_t wave_pitch = fds_wave_pitch(counter, mod_gain, pitch);
    do {
        sum += output * tick;
        cycles -= tick;
        tick = 16;
        if (mod_on) {
            uint32_t low = (mod_acc & 0xfff) + mod_pitch;
            uint32_t position = (mod_acc >> 12) & 63;
            if ((low > 0xfff) || mod_force) {
                uint8_t entry = fds.mod_table[position >> 1];
                if (entry == 4) {
                    counter = 0;
                } else {
                    counter = (int8_t)((uint8_t)(counter + fds_mod_adjust[entry]) << 1) >> 1;
                }
                position = (position + 1) & 63;
                wave_pitch = fds_wave_pitch(counter, mod_gain, pitch);
            }
            mod_acc = (position << 12) | (low & 0xfff);
        }
        if (wave_on) {
            wave_acc = (wave_acc + wave_pitch) & 0xffffff;
        }
        uint32_t position = wave_acc >> 18;
        if (position == 0) {
            vol_output_gain = vol_gain;
        }
        if (!write_enable) {
            output = fds.wave[position] * ((vol_output_gain < 32) ? vol_output_gain : 32) * master;
        }
    } while (cycles >= 16);
    fds.sum = sum + output * cycles;
    fds.tick = 16 - cycles;
    fds.mod_acc = mod_acc;
    fds.wave_acc = wave_acc;
    fds.vol_output_gain = vol_output_gain;
    fds.mod_counter = counter;
    fds.output = output;
}

// 2^32 / n rounded up, for fds_output's average: exact for the sums it divides (under 2^22, n 2-64)
static uint32_t fds_reciprocal[65];


// (in mixer units: the full output, 63 * 32 * 30, about 2.4 times an APU pulse at full volume)
// (with the FPU path a call from the sample loop, which keeps the loop's code as it was before it: the frame slot's
// layout, tools/hot_frame.txt)
static inline int32_t fds_output() {
    int32_t out = fds.output;
    if (fds.sum_cycles) {
        // (the sum is never negative)
        out = ((uint32_t)(fds.sum_cycles - 2) <= 62) ?
            (int32_t)(((uint64_t)(uint32_t)fds.sum * fds_reciprocal[fds.sum_cycles]) >> 32) : fds.sum / fds.sum_cycles;
    }
    fds.sum = 0;
    fds.sum_cycles = 0;
    // 1 pole lowpass, cutoff about 2 kHz at 44.1 kHz (1 - e^(-2 pi 2000 / 44100) ~ 0.248 ~ 63 / 256)
    fds.filtered += ((out - fds.filtered) * 63) >> 8;
    // as Mesen 2: sample * gain * master / 1152, x20 in its mixer, x5.98 to our scale (0.1038 ~ 53 / 512)
    return (fds.filtered * 53) >> 9;
}

#ifdef FRNES_FPU_N163
// the FPU paths' float waves from the chips' own (a new buffer, a state loaded)
static void fpu_spans_rebuild(void) {
#ifdef FRNES_FPU_N163
    for (uint32_t i = 0; i < 256; i++) {
        n163_wave_f[i] = n163_wave_f[i + 256] = (float)((int32_t)n163.wave[i] - 8);
    }
    n163_span_pos = n163_span_length = 0;
#endif
}
#endif

// test builds: the expansion chip's state, in the same terms as Mesen 2's getState() keys (mapper.audio.*)
int apu_synth_expansion_state(char* buffer, int size) {
    if (expansion == APU_EXPANSION_VRC6) {
        return snprintf(buffer, size, "p1 %u %u %u %u %u p2 %u %u %u %u %u saw %u %u %u %u %u",
            vrc6.pulse[0].enabled, vrc6.pulse[0].volume, vrc6.pulse[0].duty, vrc6.pulse[0].period, vrc6.pulse[0].step,
            vrc6.pulse[1].enabled, vrc6.pulse[1].volume, vrc6.pulse[1].duty, vrc6.pulse[1].period, vrc6.pulse[1].step,
            vrc6.saw_enabled, vrc6.saw_rate, vrc6.saw_period, vrc6.saw_accumulator, vrc6.saw_step);
    }
    if (expansion == APU_EXPANSION_5B) {
        return snprintf(buffer, size, "r %u %u %u %u %u %u m %u v %u %u %u",
            s5b.registers[0], s5b.registers[1], s5b.registers[2], s5b.registers[3], s5b.registers[4], s5b.registers[5],
            s5b.registers[7], s5b.registers[8], s5b.registers[9], s5b.registers[10]);
    }
    if (expansion == APU_EXPANSION_VRC7) {
        int n = snprintf(buffer, size, "r");
        for (uint32_t r = 0x10; r < 0x36; r++) {
            if ((r & 0x0f) < 6) {
                n += snprintf(buffer + n, size - n, " %u", vrc7.registers[r]);
            }
        }
        return n;
    }
    if (expansion == APU_EXPANSION_N163) {
        int n = snprintf(buffer, size, "ram");
        for (uint32_t i = 0; i < 128; i++) {
            n += snprintf(buffer + n, size - n, " %u", n163.ram[i]);
        }
        return n;
    }
    if (expansion == APU_EXPANSION_MMC5) {
        return snprintf(buffer, size, "p1 %u %u %u %u p2 %u %u %u %u pcm %u", mmc5.pulse[0].period, mmc5.pulse[0].duty,
            envelope_output(&mmc5.pulse[0].envelope), mmc5.pulse[0].length, mmc5.pulse[1].period, mmc5.pulse[1].duty,
            envelope_output(&mmc5.pulse[1].envelope), mmc5.pulse[1].length, mmc5.pcm);
    }
    if (expansion == APU_EXPANSION_FDS) {
        return snprintf(buffer, size, "vol %u mod %u pitch %u modpitch %u counter %d we %u master %u halt %u%u%u pos %lu",
            fds.vol_gain, fds.mod_gain, fds.pitch, fds.mod_pitch, fds.mod_counter, fds.write_enable, fds.master,
            fds.wave_halt, fds.env_halt, fds.mod_halt, (unsigned long)(fds.wave_acc >> 18));
    }
    return snprintf(buffer, size, "none");
}


static void expansion_write(uint16_t address, uint8_t value) {
    switch (expansion) {
        case APU_EXPANSION_VRC6: vrc6_write(address, value); break;
        case APU_EXPANSION_5B: s5b_write(address, value); break;
        case APU_EXPANSION_MMC5: mmc5_write(address, value); break;
        case APU_EXPANSION_VRC7: vrc7_write(address, value); break;
        case APU_EXPANSION_N163: n163_write(address, value); break;
        case APU_EXPANSION_FDS: fds_write(address, value); break;
    }
}

#ifdef APU_SYNTH_TIME_EXPANSION
/*
 * The player's frame display (FRNES_FRAME_HUD builds): expansion audio's share of the synth, timed around its run
 * and mix on every 64th sample by TMU2's counter (KOS's clock: 12.5 MHz, counting down, reloaded every second; a
 * sample the reload falls in isn't counted). A read of the counter costs about a microsecond: every 8th sample was
 * 0.4 ms a frame
 */
#define TMU2_TCNT (*(volatile uint32_t*)0xffd80024)
static uint32_t expansion_timed, expansion_samples, expansion_ticks;
static inline uint32_t expansion_time_start() {
    return expansion_timed ? TMU2_TCNT : 0;
}
static inline void expansion_time_end(uint32_t start) {
    if (expansion_timed) {
        uint32_t now = TMU2_TCNT;
        if (now <= start) {
            expansion_ticks += start - now;
        }
    }
}
#else
static inline uint32_t expansion_time_start() {
    return 0;
}
static inline void expansion_time_end(uint32_t start) {
    (void)start;
}
#endif

uint32_t apu_synth_expansion_us() {
#ifdef APU_SYNTH_TIME_EXPANSION
    // (64 samples a timed one, 80 ns a tick)
    uint32_t us = (expansion_ticks * 128) / 25;
    expansion_ticks = 0;
    return us;
#else
    return 0;
#endif
}

static FRNES_HOT(timer_expansion_run) void expansion_run(int32_t cycles) {
    uint32_t start = expansion_time_start();
    switch (expansion) {
        case APU_EXPANSION_VRC6: vrc6_run(cycles); break;
        case APU_EXPANSION_5B: s5b_run(cycles); break;
        case APU_EXPANSION_MMC5: mmc5_run(cycles); break;
        case APU_EXPANSION_N163: n163_run(cycles); break;
        case APU_EXPANSION_FDS: fds_run(cycles); break;
    }
    expansion_time_end(start);
}

// (the options' and the FPU N163's frames: its own function outside the timer slot, so the slot keeps the standard code;
// the N163 through the FPU synth has no per cycle work)
static __attribute__((noinline)) void expansion_run_average(int32_t cycles) {
    uint32_t start = expansion_time_start();
    switch (expansion) {
        case APU_EXPANSION_VRC6: vrc6_run_opt(cycles, 1); break;
        case APU_EXPANSION_5B: s5b_run_opt(cycles, 1); break;
        case APU_EXPANSION_MMC5: mmc5_run_opt(cycles, 1); break;
#ifdef FRNES_FPU_N163
        case APU_EXPANSION_N163: if (!fpu_spans) n163_run(cycles); break;
#else
        case APU_EXPANSION_N163: n163_run(cycles); break;
#endif
        case APU_EXPANSION_FDS: fds_run(cycles); break;
    }
    expansion_time_end(start);
}

static inline int32_t expansion_mix_chip() {
    switch (expansion) {
        case APU_EXPANSION_VRC6: return vrc6_output() * VRC6_SCALE;
        case APU_EXPANSION_5B: return s5b_output() * S5B_SCALE;
        case APU_EXPANSION_MMC5: return mmc5_output();
        case APU_EXPANSION_VRC7: return vrc7_output() * VRC7_SCALE;
        case APU_EXPANSION_N163: return n163_output() * N163_SCALE;
        case APU_EXPANSION_FDS: return fds_output();
    }
    return 0;
}

static inline __attribute__((always_inline)) int32_t expansion_mix_chip_opt(const uint32_t average) {
    switch (expansion) {
        case APU_EXPANSION_VRC6: return average ? (vrc6_output_opt(1) * VRC6_SCALE) >> 4 : vrc6_output_opt(0) * VRC6_SCALE;
        case APU_EXPANSION_5B: return average ? (s5b_output_opt(1) * S5B_SCALE) >> 4 : s5b_output_opt(0) * S5B_SCALE;
        case APU_EXPANSION_MMC5: return mmc5_output_opt(average);
        case APU_EXPANSION_VRC7: return vrc7_output() * VRC7_SCALE;
#ifdef FRNES_FPU_N163
        case APU_EXPANSION_N163: return (fpu_spans ? n163_output_fpu() : n163_output()) * N163_SCALE;
#else
        case APU_EXPANSION_N163: return n163_output() * N163_SCALE;
#endif
        case APU_EXPANSION_FDS: return fds_output();
    }
    return 0;
}

static inline int32_t expansion_mix() {
    uint32_t start = expansion_time_start();
    int32_t out = expansion_mix_chip();
    expansion_time_end(start);
    return out;
}

static inline __attribute__((always_inline)) int32_t expansion_mix_opt(const uint32_t average) {
    uint32_t start = expansion_time_start();
    int32_t out = expansion_mix_chip_opt(average);
    expansion_time_end(start);
    return out;
}

/**
 * Register writes
 */
static void apply_write(uint16_t address, uint8_t value) {
    switch (address) {
        case 0x4000: case 0x4004: {
            pulse_t* p = &pulse[(address >> 2) & 1];
            p -> duty = value >> 6;
            p -> envelope.loop = (value >> 5) & 1;
            p -> envelope.constant = (value >> 4) & 1;
            p -> envelope.volume = value & 15;
        } break;
        case 0x4001: case 0x4005: {
            pulse_t* p = &pulse[(address >> 2) & 1];
            p -> sweep_enabled = value >> 7;
            p -> sweep_period = (value >> 4) & 7;
            p -> sweep_negate = (value >> 3) & 1;
            p -> sweep_shift = value & 7;
            p -> sweep_reload = 1;
        } break;
        case 0x4002: case 0x4006: {
            pulse_t* p = &pulse[(address >> 2) & 1];
            p -> period = (p -> period & 0x700) | value;
        } break;
        case 0x4003: case 0x4007: {
            pulse_t* p = &pulse[(address >> 2) & 1];
            p -> period = (p -> period & 0xff) | ((value & 7) << 8);
            if (p -> enabled) {
                p -> length = length_table[value >> 3];
            }
            p -> step = 0;
            p -> envelope.start = 1;
        } break;
        case 0x4008:
            triangle.control = value >> 7;
            triangle.linear_reload_value = value & 0x7f;
            break;
        case 0x400a:
            triangle.period = (triangle.period & 0x700) | value;
            break;
        case 0x400b:
            triangle.period = (triangle.period & 0xff) | ((value & 7) << 8);
            if (triangle.enabled) {
                triangle.length = length_table[value >> 3];
            }
            triangle.linear_reload = 1;
            break;
        case 0x400c:
            noise.envelope.loop = (value >> 5) & 1;
            noise.envelope.constant = (value >> 4) & 1;
            noise.envelope.volume = value & 15;
            break;
        case 0x400e:
            noise.mode = value >> 7;
            noise.period = noise_period_table[value & 15];
            break;
        case 0x400f:
            if (noise.enabled) {
                noise.length = length_table[value >> 3];
            }
            noise.envelope.start = 1;
            break;
        case 0x4010:
            dmc.loop = (value >> 6) & 1;
            dmc.rate = dmc_rate_table[value & 15];
            break;
        case 0x4011:
            dmc.output = value & 0x7f;
            break;
        case 0x4012:
            dmc.sample_address = 0xc000 | (value << 6);
            break;
        case 0x4013:
            dmc.sample_length = (value << 4) | 1;
            break;
        case 0x4015:
            pulse[0].enabled = value & 1;
            pulse[1].enabled = (value >> 1) & 1;
            triangle.enabled = (value >> 2) & 1;
            noise.enabled = (value >> 3) & 1;
            dmc.enabled = (value >> 4) & 1;
            if (!pulse[0].enabled) pulse[0].length = 0;
            if (!pulse[1].enabled) pulse[1].length = 0;
            if (!triangle.enabled) triangle.length = 0;
            if (!noise.enabled) noise.length = 0;
            if (!dmc.enabled) {
                dmc.bytes_remaining = 0;
            } else if (dmc.bytes_remaining == 0) {
                dmc_restart();
            }
            break;
        case 0x4017:
            sequencer_five_step = value >> 7;
            sequencer_step = 0;
            sequencer_counter = sequencer_five_step ? sequence_5[0] : sequence_4[0];
            if (sequencer_five_step) {
                quarter_frame();
                half_frame();
            }
            break;
        default:
            expansion_write(address, value);
            break;
    }
}

/**
 * Runs the channel timers for 'cycles' CPU cycles
 */
// (what each pulse outputs while its step lets it through: update_levels)
static uint8_t pulse_level[2];
// level x CPU cycles of each pulse and of the triangle since the last sample, and the cycles (apu_synth_frame_impl)
static uint32_t average_pulse[2], average_triangle, average_cycles;

// a pulse's step and timer over 'cycles', its level x the cycles it was high added to *sum
static inline __attribute__((always_inline)) void average_pulse_run(int32_t* timer_io, uint32_t* step_io, uint32_t period,
    uint32_t duty, uint32_t level, int32_t cycles, uint32_t* sum) {
    uint32_t step = *step_io, high = 0;
    if (period >= 8) {
        int32_t timer = *timer_io, remaining = cycles;
        const int32_t reload = (period + 1) << 1;
        while (timer <= remaining) {
            high += ((duty >> step) & 1) ? timer : 0;
            remaining -= timer;
            step = (step - 1) & 7;
            timer = reload;
        }
        high += ((duty >> step) & 1) ? remaining : 0;
        *timer_io = timer - remaining;
        *step_io = step;
    } else {
        high = ((duty >> step) & 1) ? cycles : 0;
    }
    *sum += high * level;
}

static inline __attribute__((always_inline)) void average_triangle_run(int32_t* timer_io, uint32_t* step_io, uint32_t period,
    uint32_t running, int32_t cycles, uint32_t* sum) {
    uint32_t step = *step_io, total = 0;
    if (running) {
        int32_t timer = *timer_io, remaining = cycles;
        const int32_t reload = period + 1;
        while (timer <= remaining) {
            total += triangle_table[step] * timer;
            remaining -= timer;
            step = (step + 1) & 31;
            timer = reload;
        }
        total += triangle_table[step] * remaining;
        *timer_io = timer - remaining;
        *step_io = step;
    } else {
        total = triangle_table[step] * cycles;
    }
    *sum += total;
}

static inline void run_channels(int32_t cycles) {
    for (uint32_t i = 0; i < 2; i++) {
        pulse_t* p = &pulse[i];
        if (p -> period >= 8) {
            p -> timer -= cycles;
            while (p -> timer <= 0) {
                p -> timer += (p -> period + 1) << 1;
                p -> step = (p -> step - 1) & 7;
            }
        }
    }
    // the triangle only advances while both counters are running; ultrasonic periods are left alone
    if (triangle.length && triangle.linear_counter && (triangle.period >= 2)) {
        triangle.timer -= cycles;
        while (triangle.timer <= 0) {
            triangle.timer += triangle.period + 1;
            triangle.step = (triangle.step + 1) & 31;
        }
    }
    noise.timer -= cycles;
    while (noise.timer <= 0) {
        noise.timer += noise.period;
        uint16_t feedback = (noise.shift ^ (noise.shift >> (noise.mode ? 6 : 1))) & 1;
        noise.shift = (noise.shift >> 1) | (feedback << 14);
    }
    if (dmc.enabled || !dmc.silence) {
        dmc.timer -= cycles;
        while (dmc.timer <= 0) {
            dmc.timer += dmc.rate;
            dmc_clock();
        }
    }
    if (expansion) {
        expansion_run(cycles);
    }
}

static inline __attribute__((always_inline)) void run_channels_opt(int32_t cycles, const uint32_t average) {
    if (average) {
        for (uint32_t i = 0; i < 2; i++) {
            pulse_t* p = &pulse[i];
            int32_t timer = p -> timer;
            uint32_t step = p -> step;
            average_pulse_run(&timer, &step, p -> period, duty_table[p -> duty], pulse_level[i], cycles, &average_pulse[i]);
            p -> timer = timer;
            p -> step = step;
        }
        {
            int32_t timer = triangle.timer;
            uint32_t step = triangle.step;
            average_triangle_run(&timer, &step, triangle.period, triangle.length && triangle.linear_counter && (triangle.period >= 2),
                cycles, &average_triangle);
            triangle.timer = timer;
            triangle.step = step;
        }
        average_cycles += cycles;
    } else {
        for (uint32_t i = 0; i < 2; i++) {
            pulse_t* p = &pulse[i];
            if (p -> period >= 8) {
                p -> timer -= cycles;
                while (p -> timer <= 0) {
                    p -> timer += (p -> period + 1) << 1;
                    p -> step = (p -> step - 1) & 7;
                }
            }
        }
        // the triangle only advances while both counters are running; ultrasonic periods are left alone
        if (triangle.length && triangle.linear_counter && (triangle.period >= 2)) {
            triangle.timer -= cycles;
            while (triangle.timer <= 0) {
                triangle.timer += triangle.period + 1;
                triangle.step = (triangle.step + 1) & 31;
            }
        }
    }
    noise.timer -= cycles;
    while (noise.timer <= 0) {
        noise.timer += noise.period;
        uint16_t feedback = (noise.shift ^ (noise.shift >> (noise.mode ? 6 : 1))) & 1;
        noise.shift = (noise.shift >> 1) | (feedback << 14);
    }
    if (dmc.enabled || !dmc.silence) {
        dmc.timer -= cycles;
        while (dmc.timer <= 0) {
            dmc.timer += dmc.rate;
            dmc_clock();
        }
    }
    if (expansion) {
        if (average) {
            expansion_run_average(cycles);
#ifdef FRNES_FPU_N163
        } else if (fpu_spans) {
            // (the N163 through the FPU synth: no per cycle work)
#endif
        } else {
            expansion_run(cycles);
        }
    }
}

/*
 * What the pulses and the noise output when their sequencer step / shift register lets them through: 0 while the
 * length counter is 0 or a pulse is muted by its sweep, the envelope otherwise. That only changes with a register
 * write or a frame sequencer clock, so it is worked out there instead of for every sample.
 */
static uint8_t noise_level;

static void update_levels() {
    for (uint32_t i = 0; i < 2; i++) {
        pulse_level[i] = (pulse[i].length && !pulse_muted(&pulse[i])) ? envelope_output(&pulse[i].envelope) : 0;
    }
    noise_level = noise.length ? envelope_output(&noise.envelope) : 0;
}

// (set: the frame takes its slow path, apu_synth_frame_slow: a tap set (apu_synth_set_tap) or an output option on.
// apu_synth_frame's one test, as before the options, so its code and the frame slot's layout stay; written only by the
// setters, so a build that never calls them (the test builds) folds it to NULL as before)
void (*apu_synth_tap)(const int16_t* samples, uint32_t count) = NULL;

/*
 * The 2A03 channels' timers, sequencer steps and periods, in locals while the samples are made (about 40 CPU cycles
 * each): written back to the channels before, and read again after, anything that works on them through the globals
 * (a register write, a frame sequencer clock).
 */
typedef struct {
    int32_t pulse_timer[2];
    uint32_t pulse_step[2];
    uint32_t pulse_period[2];
    uint32_t pulse_duty[2];         // duty_table bits of the pulse's duty
    int32_t triangle_timer;
    uint32_t triangle_step;
    uint32_t triangle_period;
    uint32_t triangle_running;      // length and linear counters non zero, period not ultrasonic
    int32_t noise_timer;
    uint32_t noise_period;
    uint32_t noise_shift;
    uint32_t noise_tap;             // the feedback bit's distance: 6 in mode 1, 1 otherwise
} synth_channels_t;

static inline __attribute__((always_inline)) void synth_channels_load(synth_channels_t* c) {
    for (uint32_t i = 0; i < 2; i++) {
        c -> pulse_timer[i] = pulse[i].timer;
        c -> pulse_step[i] = pulse[i].step;
        c -> pulse_period[i] = pulse[i].period;
        c -> pulse_duty[i] = duty_table[pulse[i].duty];
    }
    c -> triangle_timer = triangle.timer;
    c -> triangle_step = triangle.step;
    c -> triangle_period = triangle.period;
    c -> triangle_running = triangle.length && triangle.linear_counter && (triangle.period >= 2);
    c -> noise_timer = noise.timer;
    c -> noise_period = noise.period;
    c -> noise_shift = noise.shift;
    c -> noise_tap = noise.mode ? 6 : 1;
}

static inline __attribute__((always_inline)) void synth_channels_store(const synth_channels_t* c) {
    for (uint32_t i = 0; i < 2; i++) {
        pulse[i].timer = c -> pulse_timer[i];
        pulse[i].step = c -> pulse_step[i];
    }
    triangle.timer = c -> triangle_timer;
    triangle.step = c -> triangle_step;
    noise.timer = c -> noise_timer;
    noise.shift = c -> noise_shift;
}

// run_channels with the 2A03 channels in locals
static inline __attribute__((always_inline)) void synth_channels_run(synth_channels_t* c, int32_t cycles) {
    for (uint32_t i = 0; i < 2; i++) {
        if (c -> pulse_period[i] >= 8) {
            int32_t timer = c -> pulse_timer[i] - cycles;
            if (timer <= 0) {
                uint32_t step = c -> pulse_step[i];
                int32_t reload = (c -> pulse_period[i] + 1) << 1;
                do {
                    timer += reload;
                    step = (step - 1) & 7;
                } while (timer <= 0);
                c -> pulse_step[i] = step;
            }
            c -> pulse_timer[i] = timer;
        }
    }
    if (c -> triangle_running) {
        int32_t timer = c -> triangle_timer - cycles;
        if (timer <= 0) {
            uint32_t step = c -> triangle_step;
            int32_t reload = c -> triangle_period + 1;
            do {
                timer += reload;
                step = (step + 1) & 31;
            } while (timer <= 0);
            c -> triangle_step = step;
        }
        c -> triangle_timer = timer;
    }
    {
        int32_t timer = c -> noise_timer - cycles;
        if (timer <= 0) {
            uint32_t shift = c -> noise_shift;
            const uint32_t tap = c -> noise_tap;
            const int32_t period = c -> noise_period;
            do {
                timer += period;
                uint32_t feedback = (shift ^ (shift >> tap)) & 1;
                shift = (shift >> 1) | (feedback << 14);
            } while (timer <= 0);
            c -> noise_shift = shift;
        }
        c -> noise_timer = timer;
    }
    if (dmc.enabled || !dmc.silence) {
        dmc.timer -= cycles;
        while (dmc.timer <= 0) {
            dmc.timer += dmc.rate;
            dmc_clock();
        }
    }
    if (expansion) {
        expansion_run(cycles);
    }
}

static inline __attribute__((always_inline)) void synth_channels_run_opt(synth_channels_t* c, int32_t cycles, const uint32_t average) {
    if (average) {
        for (uint32_t i = 0; i < 2; i++) {
            average_pulse_run(&c -> pulse_timer[i], &c -> pulse_step[i], c -> pulse_period[i], c -> pulse_duty[i], pulse_level[i],
                cycles, &average_pulse[i]);
        }
        average_triangle_run(&c -> triangle_timer, &c -> triangle_step, c -> triangle_period, c -> triangle_running, cycles,
            &average_triangle);
        average_cycles += cycles;
    } else {
        for (uint32_t i = 0; i < 2; i++) {
            if (c -> pulse_period[i] >= 8) {
                int32_t timer = c -> pulse_timer[i] - cycles;
                if (timer <= 0) {
                    uint32_t step = c -> pulse_step[i];
                    int32_t reload = (c -> pulse_period[i] + 1) << 1;
                    do {
                        timer += reload;
                        step = (step - 1) & 7;
                    } while (timer <= 0);
                    c -> pulse_step[i] = step;
                }
                c -> pulse_timer[i] = timer;
            }
        }
        if (c -> triangle_running) {
            int32_t timer = c -> triangle_timer - cycles;
            if (timer <= 0) {
                uint32_t step = c -> triangle_step;
                int32_t reload = c -> triangle_period + 1;
                do {
                    timer += reload;
                    step = (step + 1) & 31;
                } while (timer <= 0);
                c -> triangle_step = step;
            }
            c -> triangle_timer = timer;
        }
    }
    {
        int32_t timer = c -> noise_timer - cycles;
        if (timer <= 0) {
            uint32_t shift = c -> noise_shift;
            const uint32_t tap = c -> noise_tap;
            const int32_t period = c -> noise_period;
            do {
                timer += period;
                uint32_t feedback = (shift ^ (shift >> tap)) & 1;
                shift = (shift >> 1) | (feedback << 14);
            } while (timer <= 0);
            c -> noise_shift = shift;
        }
        c -> noise_timer = timer;
    }
    if (dmc.enabled || !dmc.silence) {
        dmc.timer -= cycles;
        while (dmc.timer <= 0) {
            dmc.timer += dmc.rate;
            dmc_clock();
        }
    }
    if (expansion) {
        if (average) {
            expansion_run_average(cycles);
#ifdef FRNES_FPU_N163
        } else if (fpu_spans) {
            // (the N163 through the FPU synth: no per cycle work)
#endif
        } else {
            expansion_run(cycles);
        }
    }
}

static inline __attribute__((always_inline)) int32_t synth_channels_mix(const synth_channels_t* c) {
    uint32_t p0 = ((c -> pulse_duty[0] >> c -> pulse_step[0]) & 1) ? pulse_level[0] : 0;
    uint32_t p1 = ((c -> pulse_duty[1] >> c -> pulse_step[1]) & 1) ? pulse_level[1] : 0;
    uint32_t t = triangle_table[c -> triangle_step];
    uint32_t n = (c -> noise_shift & 1) ? 0 : noise_level;
    int32_t out = pulse_mix[p0 + p1] + tnd_mix[3 * t + 2 * n + dmc.output];
    if (expansion) {
        out += expansion_mix();
    }
    return out;
}

static inline __attribute__((always_inline)) int32_t synth_channels_mix_opt(const synth_channels_t* c, const uint32_t average) {
    uint32_t p0 = ((c -> pulse_duty[0] >> c -> pulse_step[0]) & 1) ? pulse_level[0] : 0;
    uint32_t p1 = ((c -> pulse_duty[1] >> c -> pulse_step[1]) & 1) ? pulse_level[1] : 0;
    uint32_t t = triangle_table[c -> triangle_step];
    uint32_t n = (c -> noise_shift & 1) ? 0 : noise_level;
    // (the averages, 16.16, the mixer's tables read between their entries)
    if (average) {
        uint32_t pulses, tnd;
        if (average_cycles) {
            pulses = average_scale(average_pulse[0] + average_pulse[1], average_cycles);
            tnd = average_scale(3 * average_triangle, average_cycles) + ((2 * n + dmc.output) << 16);
        } else {
            pulses = (p0 + p1) << 16;
            tnd = (3 * t + 2 * n + dmc.output) << 16;
        }
        average_pulse[0] = average_pulse[1] = average_triangle = average_cycles = 0;
        uint32_t pi = pulses >> 16, ti = tnd >> 16;
        if (pi > 29) pi = 29;
        if (ti > 201) ti = 201;
        int32_t out = pulse_mix[pi] + (((pulse_mix[pi + 1] - pulse_mix[pi]) * (int32_t)(pulses & 0xffff)) >> 16) +
            tnd_mix[ti] + (((tnd_mix[ti + 1] - tnd_mix[ti]) * (int32_t)(tnd & 0xffff)) >> 16);
        if (expansion) {
            out += expansion_mix_opt(average);
        }
        return out;
    }
    int32_t out = pulse_mix[p0 + p1] + tnd_mix[3 * t + 2 * n + dmc.output];
    if (expansion) {
        out += expansion_mix_opt(average);
    }
    return out;
}

// (apu_synth_tap's value when only the options need the slow path: never called)
static void output_options_marker(const int16_t* samples, uint32_t count) {
    (void)samples;
    (void)count;
}
static void output_options_slow_update(void);

// the options asked for put in use, between samples (apu_synth_frame's start, apu_synth_init)
static FRNES_COLD_TEXT void output_options_apply(void) {
    const uint32_t filter = output_options.wanted_filter;
    if (filter != output_options.filter) {
        // (the new filter takes over the old one's state where they agree: the last input, and the output's AC level as
        // its stages' starting outputs, so the switch doesn't thump)
        if (filter == APU_SYNTH_FILTER_OFF) {
            dc_previous_in = (int32_t)output_options.x;
            dc_previous_out = (int32_t)output_options.y3;
        } else if (output_options.filter == APU_SYNTH_FILTER_OFF) {
            output_options.x = (float)dc_previous_in;
            output_options.y1 = output_options.y2 = output_options.y3 = (float)dc_previous_out;
        }
        output_filter_matrix(filter);
        output_options.filter = filter;
    }
    // (the sums are 0 between samples; cleared anyway)
    average_pulse[0] = average_pulse[1] = average_triangle = average_cycles = 0;
    expansion_average_sum = expansion_average_cycles = 0;
    mmc5_average_sum = mmc5_average_cycles = 0;
    output_options.average = output_options.wanted_average;
}

// apu_synth_tap: set while the frame needs its slow path
static FRNES_COLD_TEXT void output_options_slow_update(void) {
    uint32_t active = output_options.filter | output_options.average | output_options.sound_off;
#ifdef FRNES_FPU_N163
    active |= fpu_spans != NULL;
#endif
    apu_synth_tap = output_options.user_tap ? output_options.user_tap : (active ? output_options_marker : NULL);
}

FRNES_COLD_TEXT void apu_synth_set_sound_filter(uint32_t filter) {
    output_options.wanted_filter = (filter <= APU_SYNTH_FILTER_FAMICOM) ? filter : APU_SYNTH_FILTER_OFF;
    output_options_apply();
    output_options_slow_update();
}

FRNES_COLD_TEXT void apu_synth_set_sound_quality(uint32_t quality) {
    output_options.wanted_average = (quality == APU_SYNTH_QUALITY_HIGH);
    output_options_apply();
    output_options_slow_update();
}

FRNES_COLD_TEXT void apu_synth_set_sound_enabled(uint32_t enabled) {
    const uint32_t off = !enabled;
    if (off == output_options.sound_off) {
        return;
    }
    output_options.sound_off = off;
    output_options.off_apply = apply_write;
    output_options.off_clock = sequencer_clock;
    output_options.off_levels = update_levels;
    // (between frames: the AICA keyed off, or started again on fresh samples)
    ring_read = ring_write = 0;
    output_options.off_level = 0;
    output_options.off_rest = 0;
    output_options.off_last_us = timer_us_gettime64();
    if (stream != SND_STREAM_INVALID) {
        if (off) {
            snd_stream_stop(stream);
        } else {
            snd_stream_start(stream, APU_SYNTH_SAMPLE_RATE, 0);
        }
    }
    output_options_slow_update();
}

FRNES_COLD_TEXT uint32_t apu_synth_sound_enabled(void) {
    return !output_options.sound_off;
}

FRNES_COLD_TEXT void apu_synth_set_tap(void (*tap)(const int16_t* samples, uint32_t count)) {
    output_options.user_tap = tap;
    output_options_slow_update();
}

/*
 * The frame's samples. Two copies: apu_synth_frame's without the tap (tap_buffer NULL, so its code folds away and the
 * sample loop is as it was before the NSF player set apu_synth_tap: that the pointer could be set made LTO keep its test
 * and bookkeeping in every sample, ~2 % of the release player's frame), and apu_synth_frame_tapped's with it, cold
 */
static inline __attribute__((always_inline)) void apu_synth_frame_impl(uint32_t end_cycle, int16_t* tap_buffer) {
    // (the tap sees every sample, also the ones dropped when the ring is full)
    uint32_t tapped = 0;
    synth_channels_t channels;
    synth_channels_load(&channels);
    synth_frame_end = end_cycle;
    while ((int32_t)(end_cycle - synth_cycle) > 0) {
#ifdef APU_SYNTH_TIME_EXPANSION
        expansion_timed = expansion && !(++expansion_samples & 63);
#endif
        sample_fraction += cycles_per_sample_fp;
        uint32_t target = synth_cycle + (sample_fraction >> 16);
        sample_fraction &= 0xffff;

        int32_t span = target - synth_cycle;
        if (((log_head == log_tail) || ((int32_t)(write_log[log_head].cycle - target) > 0)) && (sequencer_counter > span)) {
            // nothing happens before or at the sample (the usual case): the channels in locals
            synth_channels_run(&channels, span);
            sequencer_counter -= span;
            synth_cycle = target;
        } else {
            synth_channels_store(&channels);
            // advance to the sample, stopping at logged writes and sequencer steps
            while ((int32_t)(target - synth_cycle) > 0) {
                uint32_t next = target;
                if ((log_head != log_tail) && ((int32_t)(write_log[log_head].cycle - next) < 0)) {
                    next = write_log[log_head].cycle;
                    if ((int32_t)(next - synth_cycle) < 0) {
                        next = synth_cycle;
                    }
                }
                if ((int32_t)(synth_cycle + sequencer_counter - next) < 0) {
                    next = synth_cycle + sequencer_counter;
                }
                int32_t elapsed = next - synth_cycle;
                if (elapsed > 0) {
                    run_channels(elapsed);
                    sequencer_counter -= elapsed;
                    synth_cycle = next;
                }
                int changed = 0;
                if (sequencer_counter <= 0) {
                    sequencer_clock();
                    changed = 1;
                }
                while ((log_head != log_tail) && ((int32_t)(write_log[log_head].cycle - synth_cycle) <= 0)) {
                    apply_write(write_log[log_head].address, write_log[log_head].value);
                    log_head = (log_head + 1) & (LOG_SIZE - 1);
                    changed = 1;
                }
                if (changed) {
                    update_levels();
                }
            }
            synth_channels_load(&channels);
        }

        // DC blocking high pass, then into the ring buffer (dropped when the stream isn't keeping up)
        int32_t in = synth_channels_mix(&channels);
        int32_t out = in - dc_previous_in + ((dc_previous_out * 32604) >> 15);
        dc_previous_in = in;
        dc_previous_out = out;
        if (out > 32767) out = 32767;
        if (out < -32768) out = -32768;
        uint32_t next_write = (ring_write + 1) & (RING_SIZE - 1);
        if (next_write != ring_read) {
            ring[ring_write] = (int16_t)out;
            ring_write = next_write;
        }
        if (tap_buffer) {
            tap_buffer[tapped++] = (int16_t)out;
            if (tapped == 1024) {
                apu_synth_tap(tap_buffer, tapped);
                tapped = 0;
            }
        }
    }
    synth_channels_store(&channels);
    if (tap_buffer && tapped) {
        apu_synth_tap(tap_buffer, tapped);
    }
}

static inline __attribute__((always_inline)) void apu_synth_frame_impl_opt(uint32_t end_cycle, int16_t* tap_buffer, const uint32_t average,
    const uint32_t filter) {
    // (the tap sees every sample, also the ones dropped when the ring is full)
    uint32_t tapped = 0;
    synth_channels_t channels;
    synth_channels_load(&channels);
    synth_frame_end = end_cycle;
#ifdef _arch_dreamcast
    // (XMTRX: the filter's matrix for the frame; nothing else in the synth uses it, the NSF player's FFT reloads its own)
    if (filter) {
        shz_xmtrx_load_rows_4x4((const shz_vec4_t*)output_options.matrix[0], (const shz_vec4_t*)output_options.matrix[1],
            (const shz_vec4_t*)output_options.matrix[2], (const shz_vec4_t*)output_options.matrix[3]);
    }
#endif
    while ((int32_t)(end_cycle - synth_cycle) > 0) {
#ifdef APU_SYNTH_TIME_EXPANSION
        expansion_timed = expansion && !(++expansion_samples & 63);
#endif
        sample_fraction += cycles_per_sample_fp;
        uint32_t target = synth_cycle + (sample_fraction >> 16);
        sample_fraction &= 0xffff;

        int32_t span = target - synth_cycle;
        if (((log_head == log_tail) || ((int32_t)(write_log[log_head].cycle - target) > 0)) && (sequencer_counter > span)) {
            // nothing happens before or at the sample (the usual case): the channels in locals
            synth_channels_run_opt(&channels, span, average);
            sequencer_counter -= span;
            synth_cycle = target;
        } else {
            synth_channels_store(&channels);
            // advance to the sample, stopping at logged writes and sequencer steps
            while ((int32_t)(target - synth_cycle) > 0) {
                uint32_t next = target;
                if ((log_head != log_tail) && ((int32_t)(write_log[log_head].cycle - next) < 0)) {
                    next = write_log[log_head].cycle;
                    if ((int32_t)(next - synth_cycle) < 0) {
                        next = synth_cycle;
                    }
                }
                if ((int32_t)(synth_cycle + sequencer_counter - next) < 0) {
                    next = synth_cycle + sequencer_counter;
                }
                int32_t elapsed = next - synth_cycle;
                if (elapsed > 0) {
                    run_channels_opt(elapsed, average);
                    sequencer_counter -= elapsed;
                    synth_cycle = next;
                }
                int changed = 0;
                if (sequencer_counter <= 0) {
                    sequencer_clock();
                    changed = 1;
                }
                while ((log_head != log_tail) && ((int32_t)(write_log[log_head].cycle - synth_cycle) <= 0)) {
                    apply_write(write_log[log_head].address, write_log[log_head].value);
                    log_head = (log_head + 1) & (LOG_SIZE - 1);
                    changed = 1;
                }
                if (changed) {
                    update_levels();
                }
            }
            synth_channels_load(&channels);
        }

        // DC blocking high pass, then into the ring buffer (dropped when the stream isn't keeping up)
        int32_t in = synth_channels_mix_opt(&channels, average);
        int32_t out;
        if (filter) {
            float x = (float)in, d = x - output_options.x;
            output_options.x = x;
#ifdef _arch_dreamcast
            shz_vec4_t y = shz_xmtrx_transform_vec4((shz_vec4_t){ .x = output_options.y1, .y = output_options.y2, .z = output_options.y3, .w = d });
            output_options.y1 = y.x;
            output_options.y2 = y.y;
            output_options.y3 = y.z;
#else
            const float (*m)[4] = output_options.matrix;
            float y1 = m[0][0] * output_options.y1 + m[0][3] * d;
            float y2 = m[1][0] * output_options.y1 + m[1][1] * output_options.y2 + m[1][3] * d;
            float y3 = m[2][0] * output_options.y1 + m[2][1] * output_options.y2 + m[2][2] * output_options.y3 + m[2][3] * d;
            output_options.y1 = y1;
            output_options.y2 = y2;
            output_options.y3 = y3;
#endif
            out = (int32_t)output_options.y3;
        } else {
            out = in - dc_previous_in + ((dc_previous_out * 32604) >> 15);
            dc_previous_in = in;
            dc_previous_out = out;
        }
        if (out > 32767) out = 32767;
        if (out < -32768) out = -32768;
        uint32_t next_write = (ring_write + 1) & (RING_SIZE - 1);
        if (next_write != ring_read) {
            ring[ring_write] = (int16_t)out;
            ring_write = next_write;
        }
        if (tap_buffer) {
            tap_buffer[tapped++] = (int16_t)out;
            if (tapped == 1024) {
                if (output_options.user_tap) {
                    output_options.user_tap(tap_buffer, tapped);
                }
                tapped = 0;
            }
        }
    }
    synth_channels_store(&channels);
    if (tap_buffer && tapped && output_options.user_tap) {
        output_options.user_tap(tap_buffer, tapped);
    }
}

// (a tap set: the NSF player's visualizer, the test builds' WAV and hash; or the options' frame where they're switched
// off: tap_buffer filled and given to the tap if there is one)
static FRNES_COLD_TEXT void apu_synth_frame_tapped(uint32_t end_cycle) {
    static int16_t tap_buffer[1024];
#ifdef FRNES_HOST
    // (the host's --wav: the same constant copies as apu_synth_frame and apu_synth_frame_options, so the WAV checks
    // cover them)
    const uint32_t filter = output_options.filter != APU_SYNTH_FILTER_OFF;
    uint32_t n163 = 0;
#ifdef FRNES_FPU_N163
    n163 = fpu_spans != NULL;
#endif
    if (!output_options.average && !filter && !n163) {
        // (b28ab3cd's frame, apu_synth_frame's: the WAV checks cover it)
        apu_synth_frame_impl(end_cycle, tap_buffer);
    } else if (!output_options.average && !filter) {
        apu_synth_frame_impl_opt(end_cycle, tap_buffer, 0, 0);
    } else if (!output_options.average) {
        apu_synth_frame_impl_opt(end_cycle, tap_buffer, 0, 1);
    } else if (filter) {
        apu_synth_frame_impl_opt(end_cycle, tap_buffer, 1, 1);
    } else {
        apu_synth_frame_impl_opt(end_cycle, tap_buffer, 1, 0);
    }
#else
    apu_synth_frame_impl_opt(end_cycle, tap_buffer, output_options.average, output_options.filter != APU_SYNTH_FILTER_OFF);
#endif
}

// (the output options on: a copy of the frame for each, outside the hot slots, chosen once a frame; the off / standard
// copy in apu_synth_frame stays as it was)
static __attribute__((noinline)) void apu_synth_frame_options(uint32_t end_cycle) {
    const uint32_t filter = output_options.filter != APU_SYNTH_FILTER_OFF;
    if (!output_options.average && !filter) {
        // (apu_synth_frame_n163: the N163 through the FPU synth with the options off)
        apu_synth_frame_impl_opt(end_cycle, NULL, 0, 0);
    } else if (!output_options.average) {
        apu_synth_frame_impl_opt(end_cycle, NULL, 0, 1);
    } else if (filter) {
        apu_synth_frame_impl_opt(end_cycle, NULL, 1, 1);
    } else {
        apu_synth_frame_impl_opt(end_cycle, NULL, 1, 0);
    }
}

/*
 * Sound off: no samples. The logged writes are applied and the frame sequencer clocked between them (the channels'
 * registers, lengths and envelopes stay current, so the sound picks up as it should when it's switched on again; their
 * timers stand still), and the frame's samples are counted for apu_synth_poll's clock
 */
static FRNES_COLD_TEXT void apu_synth_frame_silent(uint32_t end_cycle) {
    uint32_t samples = 0;
    synth_frame_end = end_cycle;
    while ((int32_t)(end_cycle - synth_cycle) > 0) {
        sample_fraction += cycles_per_sample_fp;
        uint32_t target = synth_cycle + (sample_fraction >> 16);
        sample_fraction &= 0xffff;
        samples++;
        while ((int32_t)(target - synth_cycle) > 0) {
            uint32_t next = target;
            if ((log_head != log_tail) && ((int32_t)(write_log[log_head].cycle - next) < 0)) {
                next = write_log[log_head].cycle;
                if ((int32_t)(next - synth_cycle) < 0) {
                    next = synth_cycle;
                }
            }
            if ((int32_t)(synth_cycle + sequencer_counter - next) < 0) {
                next = synth_cycle + sequencer_counter;
            }
            sequencer_counter -= (int32_t)(next - synth_cycle);
            synth_cycle = next;
            if (sequencer_counter <= 0) {
                output_options.off_clock();
            }
            while ((log_head != log_tail) && ((int32_t)(write_log[log_head].cycle - synth_cycle) <= 0)) {
                output_options.off_apply(write_log[log_head].address, write_log[log_head].value);
                log_head = (log_head + 1) & (LOG_SIZE - 1);
            }
        }
    }
    output_options.off_levels();
    output_options.off_level += (int32_t)samples;
}

// the frame's slow path (apu_synth_tap set): the tap's copy, or the options'
static FRNES_COLD_TEXT void apu_synth_frame_slow(uint32_t end_cycle) {
    if (output_options.sound_off) {
        apu_synth_frame_silent(end_cycle);
        return;
    }
    uint32_t options = output_options.average | output_options.filter;
#ifdef FRNES_FPU_N163
    options |= fpu_spans != NULL;
#endif
    if (output_options.user_tap || !options) {
        apu_synth_frame_tapped(end_cycle);
    } else {
        apu_synth_frame_options(end_cycle);
    }
}

// In the player builds (FRNES_SYNTH_OUTLINE, the Makefile's REL_FLAGS) its own function in the frame loop's slot
// (FRNES_HOT(zz_main)): inlined into main.c's frnes_run, whose values live across the call, the sample loop got too few
// registers (the noise channel's shift register loop 16 instructions with a stack reload instead of 13, Lagrange's
// release player +2.3 %). The test builds' frame loop (test_main.c) keeps it inlined: there it's out of line +0.9 % in
// the replay A/Bs
#ifdef FRNES_SYNTH_OUTLINE
FRNES_HOT(zz_main)
#else
// (the test builds: out of line in the frame slot, as GCC placed it before the output options; their code tipped its
// inlining decisions the other way, apu_synth_frame into test_main.c's frame loop and initialize_frnes out of it)
__attribute__((noinline))
#endif
void apu_synth_frame(uint32_t end_cycle) {
    if (__builtin_expect(apu_synth_tap != NULL, 0)) {
        apu_synth_frame_slow(end_cycle);
        return;
    }
    apu_synth_frame_impl(end_cycle, NULL);
}

uint32_t apu_synth_buffered() {
    return (ring_write - ring_read) & (RING_SIZE - 1);
}

int apu_synth_muted = 0;

static void* stream_callback(snd_stream_hnd_t handle, int bytes_requested, int* bytes_returned) {
    __attribute__((aligned(32))) static int16_t chunk[RING_SIZE];
    (void)handle;
    uint32_t samples = bytes_requested / 2;
    if (samples > RING_SIZE) {
        samples = RING_SIZE;
    }
    for (uint32_t i = 0; i < samples; i++) {
        if (ring_read != ring_write) {
            chunk[i] = ring[ring_read];
            ring_read = (ring_read + 1) & (RING_SIZE - 1);
        } else {
            // underrun, repeat the last sample
            chunk[i] = i ? chunk[i - 1] : 0;
        }
    }
    if (apu_synth_muted) {
        memset(chunk, 0, samples * 2);
    }
    *bytes_returned = samples * 2;
    return chunk;
}

/*
 * Vblank pacing: the game runs at frames_per_second (the display's rate) instead of the NES's 60.0988, so a second of
 * play is that many frames' CPU cycles; spread over APU_SYNTH_SAMPLE_RATE samples, plus a small correction from the
 * buffer level against its target (a few frames' worth) so it neither runs dry nor grows: +-0.5% at most, which is
 * inaudible. frames_per_second 0: the NES's own rate (sound paced), no correction.
 */
#define PACE_TARGET_SAMPLES (APU_SYNTH_SAMPLE_RATE / 30)
#define PACE_MAX_CORRECTION 0.005f
void apu_synth_pace(float frames_per_second) {
    if (frames_per_second <= 0.0f) {
        cycles_per_sample_fp = CYCLES_PER_SAMPLE_FP;
        return;
    }
    float correction = (float)((int32_t)apu_synth_buffered() - PACE_TARGET_SAMPLES) * (PACE_MAX_CORRECTION / PACE_TARGET_SAMPLES);
    if (correction > PACE_MAX_CORRECTION) {
        correction = PACE_MAX_CORRECTION;
    } else if (correction < -PACE_MAX_CORRECTION) {
        correction = -PACE_MAX_CORRECTION;
    }
    // (more samples buffered than the target: fewer samples per cycle, i.e. more cycles per sample)
    float cycles = (float)CPU_CLOCK_NTSC * (frames_per_second / 60.0988f) / (float)APU_SYNTH_SAMPLE_RATE;
    cycles_per_sample_fp = (uint32_t)(cycles * (1.0f + correction) * 65536.0f);
}

// (sound off: the samples the frames made less the ones the time since would have played, at 44.1 kHz)
static FRNES_COLD_TEXT uint32_t sound_off_poll(void) {
    const uint64_t now = timer_us_gettime64();
    uint64_t played = (now - output_options.off_last_us) * APU_SYNTH_SAMPLE_RATE + output_options.off_rest;
    output_options.off_last_us = now;
    output_options.off_rest = (uint32_t)(played % 1000000u);
    played /= 1000000u;
    int64_t level = (int64_t)output_options.off_level - (int64_t)played;
    // (an "underrun": the frames fell behind; the clock doesn't let them catch up later, as the sound wouldn't)
    output_options.off_level = (level > 0) ? (int32_t)level : 0;
    return (uint32_t)output_options.off_level;
}

uint32_t apu_synth_poll() {
#ifdef FRNES_SYNTH_OUTLINE
    // (the player builds only, where the frontend can switch the sound off: inlined into test_main.c's frame loop, the
    // check changed that function's inlining, initialize_frnes out of it)
    if (__builtin_expect((apu_synth_tap != NULL) && output_options.sound_off, 0)) {
        return sound_off_poll();
    }
#endif
    if (stream != SND_STREAM_INVALID) {
        snd_stream_poll(stream);
    }
    return apu_synth_buffered();
}

// (out of line, as before the output options: inlined into test_main.c's initialize_frnes it kept that from inlining into
// the frame loop's function, which moved the loop in its slot)
__attribute__((noinline)) int apu_synth_init() {
    printf("apu synth - initializing\n");
    for (uint32_t i = 1; i < 64; i++) {
        n163_reciprocal[i] = 65536 / i;
    }
    for (uint32_t i = 2; i <= 64; i++) {
        fds_reciprocal[i] = (uint32_t)((0xffffffffull + i) / i);
    }
    // (wave length register 0: 256 samples)
    for (uint32_t i = 0; i < 8; i++) {
        n163.channels[i].length = 256 << 16;
    }
    // non-linear mixer, scaled so all channels at full output stay inside the 16 bit range
    for (uint32_t i = 0; i < 31; i++) {
        pulse_mix[i] = (int16_t)((i ? 95.52f / (8128.0f / i + 100.0f) : 0.0f) * 30000.0f);
    }
    for (uint32_t i = 0; i < 203; i++) {
        tnd_mix[i] = (int16_t)((i ? 163.67f / (24329.0f / i + 100.0f) : 0.0f) * 30000.0f);
    }
    memset(pulse, 0, sizeof(pulse));
    memset(&triangle, 0, sizeof(triangle));
    memset(&noise, 0, sizeof(noise));
    memset(&dmc, 0, sizeof(dmc));
    update_levels();
    memset(&vrc6, 0, sizeof(vrc6));
    memset(&s5b, 0, sizeof(s5b));
    memset(&mmc5, 0, sizeof(mmc5));
    memset(&vrc7, 0, sizeof(vrc7));
    for (uint32_t c = 0; c < 6; c++) {
        vrc7.op[c][0].eg_state = vrc7.op[c][1].eg_state = OPLL_EG_OFF;
        vrc7.op[c][0].eg_level = vrc7.op[c][1].eg_level = OPLL_EG_MAX;
    }
    opll_init_tables();
    mmc5.sequencer = MMC5_QUARTER_FRAME;
    // 5B volume steps are 3 dB apart, 15 is the loudest
    {
        float level = 1.0f;
        s5b_volume[0] = 0;
        for (uint32_t i = 1; i < 16; i++) {
            level *= 1.4125375f;
            s5b_volume[i] = (uint8_t)level;
        }
    }
    pulse[0].ones_complement = 1;
    noise.shift = 1;
    noise.period = noise_period_table[0];
    dmc.rate = dmc_rate_table[0];
    dmc.bits_remaining = 8;
    dmc.silence = 1;
    sequencer_five_step = 0;
    sequencer_step = 0;
    sequencer_counter = sequence_4[0];
    // (from the CPU's time now, not 0: the frontend runs games one after another in one process, cpu_cycle_total going
    // on from the previous game's, and the first apu_synth_frame synthesised every cycle since boot: a silent stall of
    // minutes at a later game's start, BLUE 2026-10-28. The write log is stamped with that time too)
    extern uint32_t cpu_cycle_total;
    synth_cycle = cpu_cycle_total;
    sample_fraction = 0;
    // (the filter starts at the channels' resting level, the triangle's first step: from 0 it began with a thump as it
    // settled, the size of that level)
    dc_previous_in = tnd_mix[3 * triangle_table[triangle.step] + dmc.output];
    dc_previous_out = 0;
#ifdef FRNES_FPU_N163
    fpu_spans_update(expansion);
#endif
    // (the chosen filter from rest, as if switched on from off now)
    output_options.filter = APU_SYNTH_FILTER_OFF;
    output_options_apply();
    log_head = log_tail = 0;
    ring_read = ring_write = 0;

    printf(" ...starting AICA stream\n");
    if (snd_stream_init() < 0) {
        printf(" ...ERROR: snd_stream_init failed\n");
        return -1;
    }
    // a small AICA buffer (2048 samples, refilled every 1024 = 23 ms) keeps the latency and the pacing granularity low
    stream = snd_stream_alloc(stream_callback, 4096);
    if (stream == SND_STREAM_INVALID) {
        printf(" ...ERROR: snd_stream_alloc failed\n");
        return -1;
    }
    if (!output_options.sound_off) {
        snd_stream_start(stream, APU_SYNTH_SAMPLE_RATE, 0);
    }
    output_options.off_level = 0;
    output_options.off_rest = 0;
    output_options.off_last_us = timer_us_gettime64();
    printf(" ...done%s\n", output_options.sound_off ? " (sound off)" : "");
    return 0;
}

void apu_synth_shutdown() {
    printf("apu synth - shutting down\n");
    if (stream != SND_STREAM_INVALID) {
        snd_stream_stop(stream);
        snd_stream_destroy(stream);
        stream = SND_STREAM_INVALID;
    }
    snd_stream_shutdown();
    snd_shutdown();
    apu_synth_silence();
    printf(" ...done\n");
}

/**
 * Stops the AICA (its ARM and every channel) and zeroes its sound RAM, so nothing left there can be played: before
 * a test build's frames start (not during them, the store queues belong to the renderer then) and on exit
 */
void apu_synth_silence() {
    spu_disable();
    spu_cdda_volume(0, 0);
    spu_memset_sq(0, 0, 0x200000);
}

#ifndef FRNES_HOST
// save states (state.c): the 2A03 channels and every expansion chip, as the synthesizer has them at the frame's end (the
// write log replayed: apu_synth_frame has run); not the output buffers or the tables worked out at start up
#include "state.h"
#define SYNTH_STATE(X, A) A(pulse) X(triangle) X(noise) X(dmc) X(sequencer_five_step) X(sequencer_step) \
    X(sequencer_counter) X(synth_cycle) X(sample_fraction) X(dc_previous_in) X(dc_previous_out) A(pulse_level) \
    X(noise_level) X(vrc6) X(s5b) X(mmc5) X(vrc7) X(vrc7_eg_tick) X(n163) X(fds)

FRNES_COLD_TEXT void apu_synth_state_save(state_writer_t* writer) {
    if (log_head != log_tail) {
        printf("STATE: %lu sound register writes not yet played, left out\n", (unsigned long)((log_tail - log_head) & (LOG_SIZE - 1)));
    }
    STATE_SAVE(SYNTH_STATE)
}

FRNES_COLD_TEXT int apu_synth_state_load(state_reader_t* reader, uint16_t version) {
    if ((version != SYNTH_STATE_VERSION) || (reader -> size != STATE_SIZE(SYNTH_STATE))) {
        return -1;
    }
    STATE_LOAD(SYNTH_STATE)
    log_head = log_tail = 0;
    // (the VRC7's channels are worked out again from its registers, and its span from here on)
    for (uint32_t c = 0; c < 6; c++) {
        vrc7.channel[c].valid = 0;
    }
    vrc7_span_pos = vrc7_span_length = 0;
#ifdef FRNES_FPU_N163
    n163_span_pos = n163_span_length = 0;
    if (fpu_spans) {
        fpu_spans_rebuild();
    }
#endif
    return 0;
}
#endif
