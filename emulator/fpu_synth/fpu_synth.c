/*
 * fpu_synth.h's channel bank (prototype). The per sample work, split as the benchmark times it:
 *   step:   phase += step (+ the modulator's bend), wrapped to the waveform's length
 *   gather: sample = wave[phase >> 16] (float)
 *   mix:    out = gains x samples: on the SH4 one FTRV per 4 channels against XMTRX (the gains, loaded once per change),
 *           elsewhere the same products in C
 */
#include <stdio.h>
#include <string.h>
#include "fpu_synth.h"

#if defined(__SH4__) || defined(_arch_dreamcast)
#define FS_SH4 1
#include <sh4zam/shz_xmtrx.h>
#include <arch/timer.h>
#include <dc/perfctr.h>
#endif

void fsyn_init(fsyn_bank_t* bank, uint32_t count, uint32_t outputs) {
    memset(bank, 0, sizeof(*bank));
    bank -> count = (count > FS_MAX_CHANNELS) ? FS_MAX_CHANNELS : count;
    bank -> outputs = (outputs == 2) ? 2 : 1;
    for (uint32_t i = 0; i < FS_MAX_CHANNELS; i++) {
        bank -> ch[i].mod_source = -1;
        bank -> ch[i].length = 1;
    }
    bank -> matrix_dirty = 1;
}

void fsyn_set_gain(fsyn_bank_t* bank, uint32_t channel, float left, float right) {
    uint32_t group = channel >> 2, column = channel & 3;
    bank -> matrix[group * 2][column] = left;
    bank -> matrix[group * 2 + 1][column] = (bank -> outputs == 2) ? right : 0.0f;
    bank -> matrix_dirty = 1;
}

// the step and gather of every channel for one sample, into s[8]
static inline void fsyn_step_gather(fsyn_bank_t* bank, float* s) {
    const uint32_t count = bank -> count;
    for (uint32_t i = 0; i < count; i++) {
        fsyn_channel_t* c = &bank -> ch[i];
        uint32_t step = c -> step;
        if (c -> mod_source >= 0) {
            step += (int32_t)(c -> mod_depth * bank -> ch[c -> mod_source].out);
        }
        uint32_t limit = c -> length << 16;
        uint32_t phase = c -> phase + step;
        while (phase >= limit) {
            phase -= limit;
        }
        c -> phase = phase;
        c -> out = s[i] = c -> wave[phase >> 16];
    }
    for (uint32_t i = count; i < FS_MAX_CHANNELS; i++) {
        s[i] = 0.0f;
    }
}

void fsyn_render_reference(fsyn_bank_t* bank, float* out, uint32_t n) {
    float s[FS_MAX_CHANNELS];
    for (uint32_t k = 0; k < n; k++) {
        fsyn_step_gather(bank, s);
        float l = 0, r = 0;
        for (uint32_t i = 0; i < FS_MAX_CHANNELS; i++) {
            l += bank -> matrix[(i >> 2) * 2][i & 3] * s[i];
            r += bank -> matrix[(i >> 2) * 2 + 1][i & 3] * s[i];
        }
        if (bank -> outputs == 2) {
            out[2 * k] += l;
            out[2 * k + 1] += r;
        } else {
            out[k] += l;
        }
    }
}

#ifdef FS_SH4
// XMTRX = the gain matrix; FTRV of channels 0-3 gives (L0-3, R0-3, x, x), of 4-7 (x, x, L4-7, R4-7)
static inline void fsyn_load_gains(fsyn_bank_t* bank) {
    // (by rows: FTRV's result element i is row i . the vector)
    shz_xmtrx_load_rows_4x4((const shz_vec4_t*)bank -> matrix[0], (const shz_vec4_t*)bank -> matrix[1],
        (const shz_vec4_t*)bank -> matrix[2], (const shz_vec4_t*)bank -> matrix[3]);
    bank -> matrix_dirty = 0;
}

void fsyn_render(fsyn_bank_t* bank, float* out, uint32_t n) {
    float s[FS_MAX_CHANNELS] __attribute__((aligned(8)));
    // (XMTRX holds the gains for the whole call: nothing else here uses it)
    fsyn_load_gains(bank);
    const uint32_t eight = bank -> count > 4;
    for (uint32_t k = 0; k < n; k++) {
        fsyn_step_gather(bank, s);
        shz_vec4_t a = shz_xmtrx_transform_vec4(*(shz_vec4_t*)&s[0]);
        float l = a.x, r = a.y;
        if (eight) {
            shz_vec4_t b = shz_xmtrx_transform_vec4(*(shz_vec4_t*)&s[4]);
            l += b.z;
            r += b.w;
        }
        if (bank -> outputs == 2) {
            out[2 * k] += l;
            out[2 * k + 1] += r;
        } else {
            out[k] += l;
        }
    }
}
#else
// (the matrix product in C: exactly fsyn_render_reference)
void fsyn_render(fsyn_bank_t* bank, float* out, uint32_t n) {
    fsyn_render_reference(bank, out, n);
}
#endif


/*
 * Channel-major rendering: each channel's block of samples in a loop of its own (its phase, step and waveform stay in
 * registers), into a per channel buffer; a modulated channel after its modulator (it reads the modulator's buffer);
 * then the mix, a sample at a time across the buffers. The same output as fsyn_render.
 */
#define FSYN_BLOCK 256
// (the caller's: FS_MAX_CHANNELS rows of FSYN_BLOCK floats, 32 byte aligned; FrNES's from the heap, apu_synth.c)
static float (*fsyn_buffers)[FSYN_BLOCK];

// (before_out: each channel's output before this block, what a channel bent by a later numbered one reads first)
static void fsyn_channel_block(fsyn_bank_t* bank, uint32_t i, uint32_t n, const float* before_out) {
    fsyn_channel_t* c = &bank -> ch[i];
    float* dst = fsyn_buffers[i];
    const float* wave = c -> wave;
    const uint32_t limit = c -> length << 16;
    uint32_t phase = c -> phase;
    if (c -> mod_source < 0) {
        const uint32_t step = c -> step;
        for (uint32_t k = 0; k < n; k++) {
            phase += step;
            if (phase >= limit) {
                phase -= limit;
                while (phase >= limit) {
                    phase -= limit;
                }
            }
            dst[k] = wave[phase >> 16];
        }
    } else {
        // (fsyn_step_gather's order: a lower numbered modulator has already played this sample, a higher numbered one
        // still holds the last; the modulators' buffers are filled first)
        const uint32_t source = (uint32_t)c -> mod_source;
        const float* src = fsyn_buffers[source];
        const float depth = c -> mod_depth;
        for (uint32_t k = 0; k < n; k++) {
            float m = (source < i) ? src[k] : (k ? src[k - 1] : before_out[source]);
            phase += c -> step + (int32_t)(depth * m);
            while (phase >= limit) {
                phase -= limit;
            }
            dst[k] = wave[phase >> 16];
        }
    }
    c -> phase = phase;
    c -> out = dst[n - 1];
}

void fsyn_render_blocked(fsyn_bank_t* bank, float* out, uint32_t n) {
    const uint32_t count = bank -> count;
#ifdef FS_SH4
    fsyn_load_gains(bank);
#endif
    while (n) {
        uint32_t m = (n > FSYN_BLOCK) ? FSYN_BLOCK : n;
        float before_out[FS_MAX_CHANNELS];
        for (uint32_t i = 0; i < count; i++) {
            before_out[i] = bank -> ch[i].out;
        }
        // (the unmodulated channels first: a modulated one reads its modulator's buffer)
        for (uint32_t pass = 0; pass < 2; pass++) {
            for (uint32_t i = 0; i < count; i++) {
                if ((bank -> ch[i].mod_source >= 0) == pass) {
                    fsyn_channel_block(bank, i, m, before_out);
                }
            }
        }
        for (uint32_t i = count; i < FS_MAX_CHANNELS; i++) {
            memset(fsyn_buffers[i], 0, m * sizeof(float));
        }
        for (uint32_t k = 0; k < m; k++) {
            float s[FS_MAX_CHANNELS] __attribute__((aligned(8)));
            for (uint32_t i = 0; i < FS_MAX_CHANNELS; i++) {
                s[i] = fsyn_buffers[i][k];
            }
            float l, r;
#ifdef FS_SH4
            shz_vec4_t a = shz_xmtrx_transform_vec4(*(shz_vec4_t*)&s[0]);
            l = a.x;
            r = a.y;
            if (count > 4) {
                shz_vec4_t b = shz_xmtrx_transform_vec4(*(shz_vec4_t*)&s[4]);
                l += b.z;
                r += b.w;
            }
#else
            l = r = 0;
            for (uint32_t i = 0; i < FS_MAX_CHANNELS; i++) {
                l += bank -> matrix[(i >> 2) * 2][i & 3] * s[i];
                r += bank -> matrix[(i >> 2) * 2 + 1][i & 3] * s[i];
            }
#endif
            if (bank -> outputs == 2) {
                out[2 * k] += l;
                out[2 * k + 1] += r;
            } else {
                out[k] += l;
            }
        }
        out += m * bank -> outputs;
        n -= m;
    }
}


/*
 * Channel-major with the mix folded in: each channel's loop adds gain x sample straight into the output (an FMAC per
 * output), no buffers and no mix pass (the unmodulated case; a modulated channel needs fsyn_render_blocked's buffers)
 */
void fsyn_render_accumulate(fsyn_bank_t* bank, float* out, uint32_t n) {
    const uint32_t count = bank -> count, stereo = bank -> outputs == 2;
    for (uint32_t i = 0; i < count; i++) {
        fsyn_channel_t* c = &bank -> ch[i];
        const float gl = bank -> matrix[(i >> 2) * 2][i & 3], gr = bank -> matrix[(i >> 2) * 2 + 1][i & 3];
        const float* wave = c -> wave;
        const uint32_t limit = c -> length << 16, step = c -> step;
        uint32_t phase = c -> phase;
        float v = c -> out;
        if (stereo) {
            for (uint32_t k = 0; k < n; k++) {
                phase += step;
                while (phase >= limit) {
                    phase -= limit;
                }
                v = wave[phase >> 16];
                out[2 * k] += gl * v;
                out[2 * k + 1] += gr * v;
            }
        } else {
            for (uint32_t k = 0; k < n; k++) {
                phase += step;
                while (phase >= limit) {
                    phase -= limit;
                }
                v = wave[phase >> 16];
                out[k] += gl * v;
            }
        }
        c -> phase = phase;
        c -> out = v;
    }
}

/*
 * The benchmark: 8 channels (and 4, and 2) of a 32 sample waveform at different steps, 1024 samples a pass. The parts
 * apart: step + gather alone, then + the mix in C, then + the mix by FTRV; the mix's share is the difference
 */
#define BENCH_N 1024
static float bench_wave[32];
static float bench_out[BENCH_N * 2];

#ifdef FS_SH4
static uint64_t bench_cycles(void) {
    return perf_cntr_count(PRFC1);
}
#else
#include <time.h>
static uint64_t bench_cycles(void) {
    // (host: nanoseconds, labelled as such)
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ull + t.tv_nsec;
}
#endif

static void bench_setup(fsyn_bank_t* bank, uint32_t count, uint32_t outputs) {
    fsyn_init(bank, count, outputs);
    for (uint32_t i = 0; i < count; i++) {
        bank -> ch[i].wave = bench_wave;
        bank -> ch[i].length = 32;
        bank -> ch[i].step = 0x8000 + i * 0x1234;
        fsyn_set_gain(bank, i, 0.1f * (i + 1), 0.05f * (8 - i));
    }
}

// (the HuC6280's LFO case: channel 1's output bends channel 0's step every sample; the blocked renderer against the
// reference, and its cost)
static void bench_modulated(void (*print)(const char* line), char* line, uint32_t size) {
    static fsyn_bank_t a, b;
    static float out_a[BENCH_N * 2], out_b[BENCH_N * 2];
    for (uint32_t pass = 0; pass < 2; pass++) {
        fsyn_bank_t* bank = pass ? &b : &a;
        bench_setup(bank, 6, 2);
        bank -> ch[0].mod_source = 1;
        bank -> ch[0].mod_depth = 4096.0f;
        bank -> ch[1].step = 0x0400;
    }
    memset(out_a, 0, sizeof(out_a));
    memset(out_b, 0, sizeof(out_b));
    fsyn_render_reference(&a, out_a, BENCH_N);
    uint64_t t0 = bench_cycles();
    fsyn_render_blocked(&b, out_b, BENCH_N);
    uint64_t t1 = bench_cycles();
    float worst = 0;
    for (uint32_t k = 0; k < BENCH_N * 2; k++) {
        float d = out_a[k] - out_b[k];
        if (d < 0) d = -d;
        if (d > worst) worst = d;
    }
    snprintf(line, size, "FPUBENCH modulated (6 stereo channels, channel 1 bends channel 0): blocked %.2f per channel-sample, max diff vs reference %g\n",
        (t1 - t0) / (6.0 * BENCH_N), (double)worst);
    print(line);
}

void fsyn_bench(void (*print)(const char* line)) {
    // (the blocked renderer's rows: FrNES gives its own, apu_synth.c)
    static float bench_blocks[FS_MAX_CHANNELS][FSYN_BLOCK] __attribute__((aligned(32)));
    if (!fsyn_buffers) {
        fsyn_buffers = bench_blocks;
    }
    static fsyn_bank_t bank;
    char line[400];
    for (uint32_t i = 0; i < 32; i++) {
        bench_wave[i] = (float)((int32_t)(i * 2) - 31) / 31.0f;
    }
#ifdef FS_SH4
    perf_cntr_start(PRFC1, PMCR_ELAPSED_TIME_MODE, PMCR_COUNT_CPU_CYCLES);
    const char* unit = "cycles";
#else
    const char* unit = "ns";
#endif
    static const uint32_t counts[3] = { 8, 4, 2 };
    for (uint32_t c = 0; c < 3; c++) {
        for (uint32_t outputs = 1; outputs <= 2; outputs++) {
            uint32_t count = counts[c];
            float s[FS_MAX_CHANNELS] __attribute__((aligned(8)));
            uint64_t best_gather = ~0ull, best_c = ~0ull, best_ftrv = ~0ull, best_blocked = ~0ull, best_accum = ~0ull;
            for (uint32_t pass = 0; pass < 8; pass++) {
                bench_setup(&bank, count, outputs);
                uint64_t t0 = bench_cycles();
                for (uint32_t k = 0; k < BENCH_N; k++) {
                    fsyn_step_gather(&bank, s);
                    bench_out[k] = s[0];
                }
                uint64_t t1 = bench_cycles();
                bench_setup(&bank, count, outputs);
                memset(bench_out, 0, sizeof(bench_out));
                uint64_t t2 = bench_cycles();
                fsyn_render_reference(&bank, bench_out, BENCH_N);
                uint64_t t3 = bench_cycles();
                bench_setup(&bank, count, outputs);
                memset(bench_out, 0, sizeof(bench_out));
                uint64_t t4 = bench_cycles();
                fsyn_render(&bank, bench_out, BENCH_N);
                uint64_t t5 = bench_cycles();
                bench_setup(&bank, count, outputs);
                memset(bench_out, 0, sizeof(bench_out));
                uint64_t t6 = bench_cycles();
                fsyn_render_blocked(&bank, bench_out, BENCH_N);
                uint64_t t7 = bench_cycles();
                if (t7 - t6 < best_blocked) best_blocked = t7 - t6;
                bench_setup(&bank, count, outputs);
                memset(bench_out, 0, sizeof(bench_out));
                uint64_t t8 = bench_cycles();
                fsyn_render_accumulate(&bank, bench_out, BENCH_N);
                uint64_t t9 = bench_cycles();
                if (t9 - t8 < best_accum) best_accum = t9 - t8;
                if (t1 - t0 < best_gather) best_gather = t1 - t0;
                if (t3 - t2 < best_c) best_c = t3 - t2;
                if (t5 - t4 < best_ftrv) best_ftrv = t5 - t4;
            }
            // (the FTRV output against the C reference's, to catch a transposed matrix)
            static float ref_out[BENCH_N * 2];
            bench_setup(&bank, count, outputs);
            memset(ref_out, 0, sizeof(ref_out));
            fsyn_render_reference(&bank, ref_out, BENCH_N);
            bench_setup(&bank, count, outputs);
            memset(bench_out, 0, sizeof(bench_out));
            fsyn_render(&bank, bench_out, BENCH_N);
            float worst = 0;
            for (uint32_t k = 0; k < BENCH_N * outputs; k++) {
                float d = bench_out[k] - ref_out[k];
                if (d < 0) d = -d;
                if (d > worst) worst = d;
            }
            static float blk_out[BENCH_N * 2];
            bench_setup(&bank, count, outputs);
            memset(blk_out, 0, sizeof(blk_out));
            fsyn_render_blocked(&bank, blk_out, BENCH_N);
            float worst_blocked = 0;
            for (uint32_t k = 0; k < BENCH_N * outputs; k++) {
                float d = blk_out[k] - ref_out[k];
                if (d < 0) d = -d;
                if (d > worst_blocked) worst_blocked = d;
            }
            bench_setup(&bank, count, outputs);
            memset(blk_out, 0, sizeof(blk_out));
            fsyn_render_accumulate(&bank, blk_out, BENCH_N);
            float worst_accum = 0;
            for (uint32_t k = 0; k < BENCH_N * outputs; k++) {
                float d = blk_out[k] - ref_out[k];
                if (d < 0) d = -d;
                if (d > worst_accum) worst_accum = d;
            }
            double per = (double)BENCH_N * count;
            snprintf(line, sizeof(line), "FPUBENCH channels=%lu outputs=%lu %s/channel-sample: step+gather %.2f, +mix C %.2f (mix %.2f), +mix FTRV %.2f (mix %.2f); FTRV vs C max diff %g; blocked (channel-major + FTRV mix) %.2f, max diff %g; accumulate (channel-major, FMAC mix) %.2f, max diff %g\n",
                (unsigned long)count, (unsigned long)outputs, unit, best_gather / per, best_c / per, (best_c - (double)best_gather) / per,
                best_ftrv / per, (best_ftrv - (double)best_gather) / per, (double)worst, best_blocked / per, (double)worst_blocked, best_accum / per, (double)worst_accum);
            print(line);
        }
    }
    bench_modulated(print, line, sizeof(line));
}
