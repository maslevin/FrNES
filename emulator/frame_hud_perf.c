/*
 * The frame display's cache loss and PPU / expansion sound times (frame_hud.h), while it's on. Built without LTO, and
 * with nothing of KOS's or the C library's but one interrupt handler registration: LTO compiles the player as one
 * program, partitioned by size, and the frame display's code there (frame_hud.c's growing, or new calls of KOS's timer
 * functions) moved the release's ordinary .text, KOS's code with it (Alien Syndrome +0.45 % with the display off). Its
 * code is last in .text (.text.~frnes_cold_hud), its variables last in .bss (.bss.~frnes_cold_hud) and its font last in
 * .rodata (.frnes_cold_rodata, tools/shlelf_hot.xc): nothing else moves for it.
 *
 * (The interrupt handler is registered by frame_hud.c's frame_hud_emu_begin, compiled with the rest: a call of KOS's
 * arch_irq_set_handler from here made it visible outside LTO, and it was compiled 4 bytes longer, moving the .text
 * after it.)
 *
 * Cache loss: PRFC1 counts the instruction cache's freeze cycles (event 0x24) or the operand cache's (0x25), switching
 * every 3 frames, over the emulation window only (restarted at its start, which clears it), as a share of the window's
 * PRFC0 cycles; each is the average of its last 8 frames.
 *
 * The PPU's time: TMU1 interrupts at SAMPLE_HZ while the display is on, and each interrupt in an emulation window takes
 * the interrupted PC: in the catch-up (cpu_sync_devices, ppu_run inlined there; the APU's catch-up too, a small part of
 * it) or the PPU's hot slots (tools/hot_slots.py: the $2002 sync, the fast renderer, rows, sprites, the MMC3's clock),
 * or in the PPU's slow path (the frame slot's ppu_ functions: replays and rendering dot by dot). The window's time
 * times their share of its samples (~14 a frame at 7 ms) is the PPU's. Nothing of it runs in the emulation: every
 * counter or test there cost ~1 % with the display off (on cpu_sync_devices' entry, and in ppu.c's replay, $2002 sync
 * and fast line paths). Record and playback builds have TMU1 as their watchdog (main.c): no samples there.
 *
 * The expansion sound's time: the samples whose PC is in its code (the timer slot from __frnes_slot_timer_expansion:
 * expansion_run, the VRC7's span fill and envelope) between a frame's emulation and the next's, the frame's synthesis,
 * each 1 / SAMPLE_HZ s. apu_synth.c's own estimate (one sample in 64 timed) misses most of the VRC7's: its FM is
 * rendered in spans of up to 1024 samples, a few calls a frame, which that sample rarely is.
 *
 * Shown: frame_hud_record draws a frame's bar with no PPU segment and the synthesis split by apu_synth.c's estimate,
 * and, every TEXT_UPDATE_FRAMES frames, its text with the cache line as it was; the next frame's start (hud_perf_begin,
 * right after it) paints the PPU's segment over the start of that row, splits its synthesis again by the samples (the
 * synthesis' extent read back from the row), and writes text lines 2 to 4 (the texture's words written whole: no reads
 * of the PVR's memory, which are slow)
 */
#include <stdint.h>
#include <stddef.h>
#include <kos/irq.h>
#include "frame_hud.h"

// (frame_hud.c's layout of its textures and bars)
#define HUD_CODEBOOK 2048
#define TEXT_W 128
#define GLYPH 8
#define TEXT_ROW_BYTES (TEXT_W / 4)
#define BARS_ROW_BYTES 64
#define HUD_FRAMES 64
#define BAR_US_PER_PIXEL 500
#define BAR_SYNTH 4
#define BAR_EXPANSION 5
#define BAR_PPU 10

#define PERF_TEXT __attribute__((noinline, section(".text.~frnes_cold_hud")))
#define PERF_BSS __attribute__((section(".bss.~frnes_cold_hud")))
#define PERF_RODATA __attribute__((section(".frnes_cold_rodata")))

// PRFC0 (KOS's free running cycle counter, never reprogrammed), PRFC1's control and count (KOS's perfctr.c)
#define PRFC0_LOW (*(volatile uint32_t*)0xff100008)
#define PMCR1 (*(volatile uint16_t*)0xff000088)
#define PMCTR1_HIGH (*(volatile uint32_t*)0xff10000c)
#define PMCTR1_LOW (*(volatile uint32_t*)0xff100010)
#define PMCR_RUN 0xc000
#define PMCR_CLR 0x2000
#define EVENT_ICACHE_FREEZE 0x24
#define EVENT_OCACHE_FREEZE 0x25

#define CACHE_AVERAGE 8
// (main.c's: the frame loop gave up waiting for the sound once. Defined first, it is placed last of this file's
// variables, the last in .bss: it moves none)
uint8_t pace_sound_wait_gave_up PERF_BSS;
static uint32_t emu_start_cycles PERF_BSS, cache_event PERF_BSS, mode PERF_BSS;
static uint16_t cache_permille[2][CACHE_AVERAGE] PERF_BSS;
static uint32_t cache_next[2] PERF_BSS;
// the last frame's PPU time and emulation window (microseconds); the sums the text averages
static uint32_t last_ppu_us PERF_BSS, last_window_us PERF_BSS;
static uint32_t ppu_us_sum PERF_BSS, slow_us_sum PERF_BSS, expansion_us_sum PERF_BSS, frames_summed PERF_BSS;

#if !defined(FRNES_RECORD) && !defined(FRNES_PLAYBACK_PATH)
#define HUD_SAMPLES 1
// (2 kHz: an interrupt costs ~3.3 us, mostly KOS's entry and exit saving the whole context, FPU included: at 20 kHz
// +0.80 ms a frame in Lagrange Point, +0.74 in Castlevania III with the display on; at 2 kHz ~0.7 %, ~33 samples a
// frame, the text's averages over TEXT_UPDATE_FRAMES frames ~500)
#define SAMPLE_HZ 2000
// TMU1 and its interrupt priority (IPRA bits 8-11) as KOS's timer.c / irq.c set them; Pφ/4 (KOS's TIMER_TPSC)
#define TMU_TSTR (*(volatile uint8_t*)0xffd80004)
#define TMU_TCOR1 (*(volatile uint32_t*)0xffd80014)
#define TMU_TCNT1 (*(volatile uint32_t*)0xffd80018)
#define TMU_TCR1 (*(volatile uint16_t*)0xffd8001c)
#define INTC_IPRA (*(volatile uint16_t*)0xffd00004)
#define SAMPLE_COUNT ((199499520 / 4 / 4) / SAMPLE_HZ)
// (weak: a placed build (the Makefile's PLACE, tools/hot_place.ld) has no slots; its hot code's sections are listed in
// __frnes_hud_ranges instead: start, end and class triples, written by tools/simplay/hud_ranges.py, at the end of .rodata.
// A slotted build's list is empty)
#define HUD_WEAK __attribute__((weak))
extern char _frnes_slot_sync[] HUD_WEAK, _frnes_slot_sync_end[] HUD_WEAK, _frnes_slot_sync_ppu[] HUD_WEAK,
    _frnes_slot_sync_ppu_end[] HUD_WEAK, _frnes_slot_render[] HUD_WEAK, _frnes_slot_render_end[] HUD_WEAK,
    _frnes_slot_row[] HUD_WEAK, _frnes_slot_row_end[] HUD_WEAK, _frnes_slot_evaluate[] HUD_WEAK,
    _frnes_slot_evaluate_end[] HUD_WEAK, _frnes_slot_fetch[] HUD_WEAK, _frnes_slot_fetch_end[] HUD_WEAK,
    _frnes_slot_mmc3[] HUD_WEAK, _frnes_slot_mmc3_end[] HUD_WEAK, _frnes_slot_frame[] HUD_WEAK,
    _frnes_slot_frame_ppu_end[] HUD_WEAK, _frnes_slot_timer_expansion[] HUD_WEAK, _frnes_slot_timer_end[] HUD_WEAK;
// (the classes: 1 the PPU's slow path, 2 the PPU, 4 the expansion sound)
#define HUD_CLASS_SLOW 1
#define HUD_CLASS_PPU 2
#define HUD_CLASS_EXPANSION 4
extern const uint32_t _frnes_hud_ranges[], _frnes_hud_ranges_end[];
static volatile uint32_t sample_window PERF_BSS, sample_timer PERF_BSS;
static volatile uint32_t samples[3] PERF_BSS;  // in the window: all, the PPU's (the slow path's too), the slow path's
static volatile uint32_t expansion_samples PERF_BSS;  // at any time (hud_perf_end clears them: the synthesis follows)

PERF_TEXT void hud_perf_sample(irq_t code, irq_context_t* ctx, void* data) {
    (void)code;
    (void)data;
    TMU_TCR1 &= ~0x100;  // (UNF)
    const uintptr_t pc = ctx -> pc;
    #define IN(slot, end) ((pc >= (uintptr_t)(slot)) && (pc < (uintptr_t)(end)))
    if (_frnes_hud_ranges != _frnes_hud_ranges_end) {
        uint32_t class = 0;
        for (const uint32_t* r = _frnes_hud_ranges; r < _frnes_hud_ranges_end; r += 3) {
            if ((pc >= r[0]) && (pc < r[1])) {
                class = r[2];
                break;
            }
        }
        if (class & HUD_CLASS_EXPANSION) {
            expansion_samples++;
        }
        if (sample_window) {
            samples[0]++;
            if (class & (HUD_CLASS_PPU | HUD_CLASS_SLOW)) {
                samples[1]++;
            }
            if (class & HUD_CLASS_SLOW) {
                samples[2]++;
            }
        }
        return;
    }
    if (IN(_frnes_slot_timer_expansion, _frnes_slot_timer_end)) {
        expansion_samples++;
    }
    if (!sample_window) {
        return;
    }
    samples[0]++;
    if (IN(_frnes_slot_frame, _frnes_slot_frame_ppu_end)) {
        samples[1]++;
        samples[2]++;
    } else if (IN(_frnes_slot_sync, _frnes_slot_sync_end) || IN(_frnes_slot_sync_ppu, _frnes_slot_sync_ppu_end) ||
        IN(_frnes_slot_render, _frnes_slot_render_end) || IN(_frnes_slot_row, _frnes_slot_row_end) ||
        IN(_frnes_slot_evaluate, _frnes_slot_evaluate_end) || IN(_frnes_slot_fetch, _frnes_slot_fetch_end) ||
        IN(_frnes_slot_mmc3, _frnes_slot_mmc3_end)) {
        samples[1]++;
    }
    #undef IN
}
#else
// (no samples: frame_hud.c's reference to the handler resolves, never set)
PERF_TEXT void hud_perf_sample(irq_t code, irq_context_t* ctx, void* data) {
    (void)code;
    (void)ctx;
    (void)data;
}
#endif
#ifdef HUD_SAMPLES

// TMU1's interrupts on while the display is (on: after frame_hud_emu_begin has set hud_perf_sample as their handler)
PERF_TEXT static void sample_timer_set(uint32_t on) {
    if (on && !sample_timer) {
        TMU_TSTR &= ~2;
        TMU_TCNT1 = SAMPLE_COUNT;
        TMU_TCOR1 = SAMPLE_COUNT;
        TMU_TCR1 = 0x20;  // (Pφ/4, UNIE)
        irq_mask_t mask = irq_disable();
        INTC_IPRA = (INTC_IPRA & ~0x0f00) | 0x0f00;  // (priority 15, KOS's TIMER_PRIO)
        irq_restore(mask);
        TMU_TSTR |= 2;
        sample_timer = 1;
    } else if (!on && sample_timer) {
        TMU_TSTR &= ~2;
        irq_mask_t mask = irq_disable();
        INTC_IPRA &= ~0x0f00;
        irq_restore(mask);
        TMU_TCR1 = 0;
        sample_timer = 0;
    }
}
#endif

/*
 * The text: the glyphs it needs, frame_hud.c's 5 x 7 font (a row a byte, bit 6 the leftmost pixel), in text_put's
 * encoding (a pixel row's 8 pixels in two bytes, each (ink << 4) | 4 pixels)
 */
static const char font_chars[] PERF_RODATA = "0123456789. ICOPUSLEX:";
static const uint8_t font_rows[][7] PERF_RODATA = {
    { 0x38, 0x44, 0x4c, 0x54, 0x64, 0x44, 0x38 }, { 0x10, 0x30, 0x10, 0x10, 0x10, 0x10, 0x38 },
    { 0x38, 0x44, 0x04, 0x08, 0x10, 0x20, 0x7c }, { 0x7c, 0x08, 0x10, 0x08, 0x04, 0x44, 0x38 },
    { 0x08, 0x18, 0x28, 0x48, 0x7c, 0x08, 0x08 }, { 0x7c, 0x40, 0x78, 0x04, 0x04, 0x44, 0x38 },
    { 0x18, 0x20, 0x40, 0x78, 0x44, 0x44, 0x38 }, { 0x7c, 0x04, 0x08, 0x10, 0x20, 0x20, 0x20 },
    { 0x38, 0x44, 0x44, 0x38, 0x44, 0x44, 0x38 }, { 0x38, 0x44, 0x44, 0x3c, 0x04, 0x08, 0x30 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30 }, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x38, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38 }, { 0x38, 0x44, 0x40, 0x40, 0x40, 0x44, 0x38 },
    { 0x38, 0x44, 0x44, 0x44, 0x44, 0x44, 0x38 }, { 0x78, 0x44, 0x44, 0x78, 0x40, 0x40, 0x40 },
    { 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x38 }, { 0x3c, 0x40, 0x40, 0x38, 0x04, 0x04, 0x78 },
    { 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7c }, { 0x7c, 0x40, 0x40, 0x78, 0x40, 0x40, 0x7c },
    { 0x44, 0x44, 0x28, 0x10, 0x28, 0x44, 0x44 }, { 0x00, 0x30, 0x30, 0x00, 0x30, 0x30, 0x00 },
};
#define FONT_SPACE 11

// a whole text line (16 columns, padded with spaces), each pixel row built here and written as whole words
PERF_TEXT static void text_line(void* texture, uint32_t line, const char* str, uint32_t ink) {
    uint8_t glyph[TEXT_W / GLYPH];
    for (uint32_t col = 0; col < TEXT_W / GLYPH; col++) {
        uint32_t g = FONT_SPACE;
        if (*str) {
            for (uint32_t i = 0; font_chars[i]; i++) {
                if (font_chars[i] == *str) {
                    g = i;
                }
            }
            str++;
        }
        glyph[col] = (uint8_t)g;
    }
    for (uint32_t y = 0; y < GLYPH; y++) {
        uint32_t row[TEXT_ROW_BYTES / 4];
        uint8_t* bytes = (uint8_t*)row;
        for (uint32_t col = 0; col < TEXT_W / GLYPH; col++) {
            uint8_t bits = (y < 7) ? font_rows[glyph[col]][y] : 0;
            bytes[col * 2] = (uint8_t)((ink << 4) | (bits >> 4));
            bytes[col * 2 + 1] = (uint8_t)((ink << 4) | (bits & 15));
        }
        volatile uint32_t* out = (volatile uint32_t*)((uintptr_t)texture + HUD_CODEBOOK + (line * 12 + y) * TEXT_ROW_BYTES);
        for (uint32_t w = 0; w < TEXT_ROW_BYTES / 4; w++) {
            out[w] = row[w];
        }
    }
}

// the text's words (here, not as string literals: those would be in this file's .rodata, among the others), each name
// with its colon and its value right after it ("IC:12.3 OC:45.6" fits the line's 16 columns)
static const char word_ic[] PERF_RODATA = "IC:", word_oc[] PERF_RODATA = " OC:", word_ppu[] PERF_RODATA = "PPU:",
    word_slo[] PERF_RODATA = " SLO:", word_exp[] PERF_RODATA = "EXP:";

// "12.3" from tenths, after 'out'; the end
PERF_TEXT static char* put_tenths(char* out, uint32_t tenths) {
    char digits[10];
    uint32_t n = 0, whole = tenths / 10;
    do {
        digits[n++] = (char)('0' + whole % 10);
        whole /= 10;
    } while (whole && (n < sizeof(digits)));
    while (n) {
        *out++ = digits[--n];
    }
    *out++ = '.';
    *out++ = (char)('0' + tenths % 10);
    return out;
}

PERF_TEXT static char* put_word(char* out, const char* word) {
    while (*word) {
        *out++ = *word++;
    }
    return out;
}

PERF_TEXT static uint32_t cache_average(uint32_t event) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < CACHE_AVERAGE; i++) {
        sum += cache_permille[event][i];
    }
    return sum / CACHE_AVERAGE;
}

// the last frame's bar row: the PPU's segment over its start, and its synthesis split again by the samples
// (expansion_us, or none); its text lines when frame_hud_record has just written the text
PERF_TEXT static void show_last(const hud_perf_view_t* view, uint32_t expansion_us, uint32_t split) {
    uint32_t ppu_px = last_ppu_us / BAR_US_PER_PIXEL, window_px = last_window_us / BAR_US_PER_PIXEL;
    ppu_px = (ppu_px > window_px) ? window_px : ppu_px;
    ppu_px = (ppu_px > 56) ? 56 : ppu_px;
    volatile uint32_t* row = (volatile uint32_t*)((uintptr_t)view -> bars_texture + HUD_CODEBOOK +
        ((view -> ring_head + HUD_FRAMES - 1) % HUD_FRAMES) * BARS_ROW_BYTES);
    if (split) {
        // (the row's 56 bar pixels read back, 14 words: where frame_hud_record put the synthesis, blue and purple)
        uint32_t words[14];
        uint8_t* px = (uint8_t*)words;
        for (uint32_t w = 0; w < 14; w++) {
            words[w] = row[w];
        }
        uint32_t first = 56, end = 0;
        for (uint32_t x = 0; x < 56; x++) {
            if ((px[x] == BAR_SYNTH) || (px[x] == BAR_EXPANSION)) {
                first = (x < first) ? x : first;
                end = x + 1;
            }
        }
        for (uint32_t x = 0; x < ppu_px; x++) {
            px[x] = BAR_PPU;
        }
        if (end > first) {
            uint32_t exp_px = expansion_us / BAR_US_PER_PIXEL;
            exp_px = (exp_px > end - first) ? (end - first) : exp_px;
            for (uint32_t x = first; x < end; x++) {
                px[x] = (x < end - exp_px) ? BAR_SYNTH : BAR_EXPANSION;
            }
        }
        for (uint32_t w = 0; w < 14; w++) {
            row[w] = words[w];
        }
    } else {
        for (uint32_t x = 0; x < ppu_px; x += 4) {
            uint32_t n = ((ppu_px - x) < 4) ? (ppu_px - x) : 4, mask = (n == 4) ? 0xffffffffu : ((1u << (n * 8)) - 1);
            row[x / 4] = (row[x / 4] & ~mask) | ((BAR_PPU * 0x01010101u) & mask);
        }
    }
    if (view -> text_frames == 0) {
        char line[24], * p = line;
        p = put_word(p, word_ic);
        p = put_tenths(p, cache_average(1));
        p = put_word(p, word_oc);
        p = put_tenths(p, cache_average(0));
        *p = 0;
        text_line(view -> text_texture, 2, line, 2);
        uint32_t frames = frames_summed ? frames_summed : 1;
#ifdef HUD_SAMPLES
        p = line;
        p = put_word(p, word_ppu);
        p = put_tenths(p, ppu_us_sum / frames / 100);
        p = put_word(p, word_slo);
        p = put_tenths(p, slow_us_sum / frames / 100);
        *p = 0;
        text_line(view -> text_texture, 3, line, 0);
        p = line;
        p = put_word(p, word_exp);
        p = put_tenths(p, expansion_us_sum / frames / 100);
        *p = 0;
        text_line(view -> text_texture, 4, line, 0);
#else
        (void)frames;
        text_line(view -> text_texture, 3, word_exp + 4, 0);
#endif
        ppu_us_sum = slow_us_sum = expansion_us_sum = frames_summed = 0;
    }
}

// 1: the display has just come on and the samples' timer is to start (frame_hud_emu_begin sets its interrupt's
// handler, then calls hud_perf_start)
PERF_TEXT uint32_t hud_perf_begin(const hud_perf_view_t* view) {
    mode = view -> mode;
#ifdef HUD_SAMPLES
    if (!mode) {
        sample_timer_set(0);
    }
#endif
    if (!mode) {
        return 0;
    }
    if (view -> frame && (view -> bars_texture != NULL) && (view -> text_texture != NULL)) {
#ifdef HUD_SAMPLES
        uint32_t expansion_us = (uint32_t)(((uint64_t)expansion_samples * 1000000) / SAMPLE_HZ);
        expansion_us_sum += expansion_us;
        show_last(view, expansion_us, 1);
#else
        show_last(view, 0, 0);
#endif
    }
#ifdef HUD_SAMPLES
    samples[0] = samples[1] = samples[2] = 0;
    sample_window = 1;
#endif
    cache_event = (view -> frame / 3) & 1;
    uint32_t start = 0;
#ifdef HUD_SAMPLES
    start = !sample_timer;
#endif
    // (KOS's perf_cntr_start: stopped, cleared, then run on the event, counting CPU cycles)
    PMCR1 &= ~PMCR_RUN;
    PMCR1 |= PMCR_CLR;
    PMCR1 = PMCR_RUN | (cache_event ? EVENT_ICACHE_FREEZE : EVENT_OCACHE_FREEZE);
    emu_start_cycles = PRFC0_LOW;
    return start;
}

PERF_TEXT void hud_perf_start(void) {
#ifdef HUD_SAMPLES
    sample_timer_set(1);
#endif
}

PERF_TEXT void hud_perf_end(void) {
    if (!mode) {
        last_ppu_us = 0;
        return;
    }
    uint32_t window = PRFC0_LOW - emu_start_cycles;
    uint32_t hi, lo, hi2;
    do {
        hi = PMCTR1_HIGH;
        lo = PMCTR1_LOW;
        hi2 = PMCTR1_HIGH;
    } while (hi != hi2);
    uint64_t frozen = ((uint64_t)hi << 32) | lo;
    cache_permille[cache_event][cache_next[cache_event]] = window ? (uint16_t)((frozen * 1000) / window) : 0;
    cache_next[cache_event] = (cache_next[cache_event] + 1) % CACHE_AVERAGE;
    last_window_us = window / 200;
    frames_summed++;
#ifdef HUD_SAMPLES
    sample_window = 0;
    expansion_samples = 0;
    uint32_t all = samples[0] ? samples[0] : 1;
    last_ppu_us = (uint32_t)(((uint64_t)last_window_us * samples[1]) / all);
    ppu_us_sum += last_ppu_us;
    slow_us_sum += (uint32_t)(((uint64_t)last_window_us * samples[2]) / all);
#else
    last_ppu_us = 0;
#endif
}
