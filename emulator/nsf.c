/*
 * NSF music (nsf.h): an .nsf file becomes a cartridge of the NSF board, whose 32 kB PRG window is filled from the
 * tune's 4 kB banks (copied in: the CPU window is banked in 8 kB pages), with 8 kB of PRG RAM at $6000 and 8 kB of CHR
 * RAM the picture never uses. A driver at $5000-$50FF (read through $4020-$5FFF, so the CPU cores fetch it through
 * read_byte: slow, so it runs only at a song's start and once a frame) has the board clear the RAM ($5F04), clears the
 * APU, calls INIT with the song in A, turns the vblank NMI on and idles in a JMP to itself at $FFF7, in the window (fetched directly, and the
 * cores skip such loops). Its NMI handler calls PLAY as many times as the tune's period fits in the frames so far
 * ($5F02, after $5F03 counted the frame in). The window's last 9 bytes are the driver's: the idle loop and the vectors
 * (NMI the handler, reset $5000, IRQ an RTI), as hardware NSF players overlay them. $5F00 starts a song (its banks set
 * again), $5F03 answers whether another one was asked for (left / right on the first pad, read from its latch).
 * The expansion chips' writes go to the synthesizer as the boards' do (one chip at a time: VRC6, VRC7, MMC5, N163,
 * Sunsoft 5B; not the FDS, whose tunes run from RAM). Built without LTO, as the boards are.
 */
#include <kos.h>
#include <math.h>
#ifndef FRNES_HOST
#include <malloc.h>
#include <sh4zam/shz_complex.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nsf.h"
#include "mapper.h"
#include "memory.h"
#include "cartridge.h"
#include "cpu.h"
#include "apu_synth.h"
#ifdef FRNES_HOST
// (host builds: the song on the console)
#define frame_hud_message(text, frames) printf("NSF SONG %s\n", (text))
#else
#include "frame_hud.h"
#endif

#define NSF_HEADER 0x80
#define NSF_BANK 0x1000
#define NSF_DRIVER 0x5000
// the NES pad bits (main.c's pad_state)
#define NSF_PAD_LEFT 0x40
#define NSF_PAD_RIGHT 0x80
// the NSF header's extra sound chip bits
#define NSF_CHIP_VRC6 0x01
#define NSF_CHIP_VRC7 0x02
#define NSF_CHIP_FDS 0x04
#define NSF_CHIP_MMC5 0x08
#define NSF_CHIP_N163 0x10
#define NSF_CHIP_5B 0x20
// frames the sound stays muted while a song starts (the driver clearing RAM and calling INIT, and a few frames of its
// sound buffered before it's heard): the switch from one song to the next is silence, not the old song cut short
#define NSF_QUIET_FRAMES 8
// an NTSC frame in CPU cycles, doubled (29780.5)
#define NSF_FRAME2 59561

// the driver (tools: the hand assembler's output, see the comment above); INIT and PLAY's addresses go at the offsets
static const uint8_t nsf_driver_code[] = {
    0x78, 0xd8, 0xa2, 0xff, 0x9a, 0xa9, 0x00, 0x8d, 0x00, 0x20, 0x8d, 0x01, 0x20, 0xad, 0x02, 0x20, 0xa9, 0x3f,
    0x8d, 0x06, 0x20, 0xa9, 0x00, 0x8d, 0x06, 0x20, 0xa9, 0x0f, 0x8d, 0x07, 0x20, 0xa9, 0x00, 0x8d, 0x06, 0x20,
    0x8d, 0x06, 0x20, 0xa9, 0x00, 0xad, 0x04, 0x5f, 0xa2, 0x13, 0x9d, 0x00, 0x40, 0xca, 0x10, 0xfa, 0xa9, 0x0f,
    0x8d, 0x15, 0x40, 0xa9, 0x40, 0x8d, 0x17, 0x40, 0xad, 0x00, 0x5f, 0xae, 0x01, 0x5f, 0x20, 0x00, 0x00, 0xad,
    0x02, 0x20, 0xa9, 0x80, 0x8d, 0x00, 0x20, 0x4c, 0xf7, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x48, 0x8a, 0x48, 0x98, 0x48, 0xad, 0x03, 0x5f, 0xf0, 0x03, 0x4c, 0x00, 0x50, 0xad, 0x02, 0x5f,
    0xf0, 0x06, 0x20, 0x00, 0x00, 0x4c, 0x8d, 0x50, 0x68, 0xa8, 0x68, 0xaa, 0x68, 0x40
};
#define NSF_DRIVER_INIT 69
#define NSF_DRIVER_PLAY 147
// its NMI handler, and an RTI for any IRQ
#define NSF_DRIVER_NMI 0x5080
#define NSF_DRIVER_RTI (NSF_DRIVER + 157)

// the loaded tune (at load time and on rare events only: last in .bss, see memory.h)
static struct {
    uint8_t* banks;             // the tune's 4 kB banks
    uint32_t bank_count;
    uint8_t bank_init[8];       // the banks at $8000-$FFFF when a song starts
    uint8_t bank[8];            // the banks there now
    uint16_t init, play;
    uint8_t songs, song;        // song: 0-based
    uint8_t chip;               // NSF_CHIP_* the synthesizer plays (one)
    uint8_t restart, pads, shown;
    uint8_t quiet;              // frames left with the sound muted (a song starting: its first frames buffered first)
    uint32_t period2;           // CPU cycles between PLAY calls, doubled
    uint32_t credit2;           // CPU cycles of the frames so far not yet played, doubled
    uint8_t n163_address;
    uint8_t n163_ram[128];
    uint8_t driver[sizeof(nsf_driver_code)];
    struct nsf_visual* visual;  // the spectrum analyzer's state (on the heap while a tune plays)
    char title[33];
} nsf FRNES_COLD_BSS;

static uint16_t nsf_word(const uint8_t* p) {
    return p[0] | (p[1] << 8);
}

/*
 * The spectrum analyzer over the black screen: the synthesizer's output (its tap) into a ring, and each frame the last
 * 1024 samples (23 ms) windowed, through SH4ZAM's FFT (shz_fft: XMTRX butterflies), into 40 bands spaced evenly in pitch from 50 Hz to 14 kHz, drawn as
 * bars (level in dB, falling back slowly) with a peak mark above each
 */
#define NSF_FFT 1024
#define NSF_BANDS 40
#define NSF_SAMPLE_RATE 44100.0f
struct nsf_visual {
#ifndef FRNES_HOST
    shz_complex_t data[NSF_FFT];
#endif
    int16_t ring[NSF_FFT];
    uint32_t at;
    float window[NSF_FFT];
    // shz_fft's output: bin k at index order[k] (it may leave them in bit reversed order), scaled by 'scale' to an
    // unnormalized transform's (both found on a test tone when the analyzer starts)
    uint16_t order[NSF_FFT / 2];
    float scale;
    uint16_t band_low[NSF_BANDS], band_high[NSF_BANDS];
    float level[NSF_BANDS], peak[NSF_BANDS];
};

static void nsf_visual_tap(const int16_t* samples, uint32_t count) {
    struct nsf_visual* v = nsf.visual;
    for (uint32_t i = 0; i < count; i++) {
        v -> ring[v -> at] = samples[i];
        v -> at = (v -> at + 1) & (NSF_FFT - 1);
    }
}

#ifndef FRNES_HOST
// a bar: two vertices at the bottom in one color, two at the top in another
static void nsf_bar(float x, float y, float w, float h, uint32_t bottom, uint32_t top) {
    pvr_vertex_t vertex;
    vertex.flags = PVR_CMD_VERTEX;
    vertex.z = 2.0f;
    vertex.u = vertex.v = 0.0f;
    vertex.oargb = 0;
    vertex.argb = bottom; vertex.x = x;     vertex.y = y + h; pvr_prim(&vertex, sizeof(vertex));
    vertex.argb = top;    vertex.x = x;     vertex.y = y;     pvr_prim(&vertex, sizeof(vertex));
    vertex.argb = bottom; vertex.x = x + w; vertex.y = y + h; pvr_prim(&vertex, sizeof(vertex));
    vertex.flags = PVR_CMD_VERTEX_EOL;
    vertex.argb = top;    vertex.x = x + w; vertex.y = y;     pvr_prim(&vertex, sizeof(vertex));
}

// green through yellow to red, by height (0-1)
static uint32_t nsf_bar_color(float height) {
    uint32_t r = (height < 0.5f) ? (uint32_t)(height * 2.0f * 255.0f) : 255;
    uint32_t g = (height < 0.5f) ? 255 : (uint32_t)((1.0f - height) * 2.0f * 255.0f);
    return 0xff000000 | (r << 16) | (g << 8) | 0x30;
}

static void nsf_visual_draw(void) {
    struct nsf_visual* v = nsf.visual;
    if (v == NULL) {
        return;
    }
    // (the samples' mean taken out first: the synthesizer's output sits on a DC level, and the window spreads it into
    // the lowest bins, the first bands, which then never fell below about half way)
    float mean = 0.0f;
    for (uint32_t i = 0; i < NSF_FFT; i++) {
        mean += v -> ring[i];
    }
    mean /= NSF_FFT;
    for (uint32_t i = 0; i < NSF_FFT; i++) {
        v -> data[i].real = (v -> ring[(v -> at + i) & (NSF_FFT - 1)] - mean) * v -> window[i];
        v -> data[i].imag = 0.0f;
    }
    shz_fft(v -> data, NSF_FFT);
    pvr_poly_cxt_t context;
    pvr_poly_hdr_t header;
    pvr_poly_cxt_col(&context, PVR_LIST_TR_POLY);
    pvr_poly_compile(&header, &context);
    pvr_prim(&header, sizeof(header));
    const float left = 64.0f, width = 512.0f / NSF_BANDS, base = 392.0f, tall = 300.0f;
    for (uint32_t b = 0; b < NSF_BANDS; b++) {
        float magnitude = 0.0f;
        for (uint32_t k = v -> band_low[b]; k <= v -> band_high[b]; k++) {
            const shz_complex_t* c = &v -> data[v -> order[k]];
            float m = c -> real * c -> real + c -> imag * c -> imag;
            magnitude = (m > magnitude) ? m : magnitude;
        }
        magnitude *= v -> scale * v -> scale;
        // (dB of full scale: -72 dB the floor, 0 the top)
        float db = 10.0f * log10f(magnitude + 1.0f) - 20.0f * log10f(32768.0f * NSF_FFT / 4.0f);
        float level = (db + 72.0f) / 72.0f;
        level = (level < 0.0f) ? 0.0f : ((level > 1.0f) ? 1.0f : level);
        v -> level[b] = (level > v -> level[b]) ? level : (v -> level[b] * 0.88f);
        v -> peak[b] = (v -> level[b] > v -> peak[b]) ? v -> level[b] : (v -> peak[b] - 0.006f);
        float h = v -> level[b] * tall;
        float x = left + b * width;
        if (h >= 1.0f) {
            nsf_bar(x, base - h, width - 3.0f, h, 0xff00ff30, nsf_bar_color(v -> level[b]));
        }
        if (v -> peak[b] > 0.01f) {
            uint32_t color = nsf_bar_color(v -> peak[b]);
            nsf_bar(x, base - v -> peak[b] * tall - 3.0f, width - 3.0f, 2.0f, color, color);
        }
    }
}
#endif

// bit k reversed in log2(NSF_FFT) bits
static uint32_t nsf_bit_reverse(uint32_t k) {
    uint32_t r = 0;
    for (uint32_t bit = 1; bit < NSF_FFT; bit <<= 1) {
        r = (r << 1) | ((k & bit) ? 1 : 0);
    }
    return r;
}

static void nsf_visual_start(void) {
    // (shz_fft wants its array on 8 bytes)
#ifdef FRNES_HOST
    struct nsf_visual* v = malloc(sizeof(struct nsf_visual));
#else
    struct nsf_visual* v = memalign(32, sizeof(struct nsf_visual));
#endif
    if (v == NULL) {
        return;
    }
    memset(v, 0, sizeof(*v));
    for (uint32_t i = 0; i < NSF_FFT; i++) {
        v -> window[i] = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * i / (NSF_FFT - 1));
    }
    for (uint32_t k = 0; k < NSF_FFT / 2; k++) {
        v -> order[k] = k;
    }
    v -> scale = 1.0f;
#ifndef FRNES_HOST
    // a test tone at bin 5, amplitude 1: an unnormalized FFT gives N / 2 there, in order or bit reversed
    for (uint32_t i = 0; i < NSF_FFT; i++) {
        v -> data[i].real = cosf(2.0f * (float)M_PI * 5 * i / NSF_FFT);
        v -> data[i].imag = 0.0f;
    }
    shz_fft(v -> data, NSF_FFT);
    uint32_t reversed = nsf_bit_reverse(5);
    float natural = hypotf(v -> data[5].real, v -> data[5].imag);
    float flipped = hypotf(v -> data[reversed].real, v -> data[reversed].imag);
    float found = (flipped > natural) ? flipped : natural;
    if (flipped > natural) {
        for (uint32_t k = 0; k < NSF_FFT / 2; k++) {
            v -> order[k] = nsf_bit_reverse(k);
        }
    }
    v -> scale = (found > 0.0f) ? ((NSF_FFT / 2) / found) : 1.0f;
    printf("NSF analyzer: shz_fft %s, scale %.3f\n", (flipped > natural) ? "bit reversed" : "in order", (double)v -> scale);
#endif
    for (uint32_t b = 0; b < NSF_BANDS; b++) {
        float low = 50.0f * powf(14000.0f / 50.0f, (float)b / NSF_BANDS);
        float high = 50.0f * powf(14000.0f / 50.0f, (float)(b + 1) / NSF_BANDS);
        uint32_t k0 = (uint32_t)(low * NSF_FFT / NSF_SAMPLE_RATE), k1 = (uint32_t)(high * NSF_FFT / NSF_SAMPLE_RATE);
        v -> band_low[b] = (k0 < 1) ? 1 : k0;
        v -> band_high[b] = (k1 < v -> band_low[b]) ? v -> band_low[b] : ((k1 >= NSF_FFT / 2) ? NSF_FFT / 2 - 1 : k1);
    }
    nsf.visual = v;
    apu_synth_set_tap(nsf_visual_tap);
#ifndef FRNES_HOST
    frame_hud_overlay = nsf_visual_draw;
#endif
}

static void nsf_visual_stop(void) {
#ifndef FRNES_HOST
    frame_hud_overlay = NULL;
#endif
    apu_synth_set_tap(NULL);
    free(nsf.visual);
    nsf.visual = NULL;
}

uint8_t* nsf_cartridge(uint8_t* data, uint32_t* size) {
    // (every load: the last tune's banks go, and nothing of it is left for a cartridge that isn't one, its sound muted
    // while a song started included)
    if (nsf.banks != NULL) {
        apu_synth_muted = 0;
        nsf_visual_stop();
    }
    free(nsf.banks);
    memset(&nsf, 0, sizeof(nsf));
    if ((*size < NSF_HEADER) || (memcmp(data, "NESM\x1a", 5) != 0)) {
        return data;
    }
    const uint8_t* header = data;
    uint16_t load = nsf_word(header + 0x08);
    uint8_t bankswitched = 0;
    for (int i = 0; i < 8; i++) {
        bankswitched |= header[0x70 + i];
    }
    uint32_t length = *size - NSF_HEADER;
    // (NSF2: the data's length, metadata after it)
    uint32_t declared = header[0x7d] | (header[0x7e] << 8) | (header[0x7f] << 16);
    if ((header[0x05] >= 2) && declared && (declared < length)) {
        length = declared;
    }
    if ((header[0x7b] & NSF_CHIP_FDS) || (!bankswitched && (load < 0x8000))) {
        printf("NSF: tunes that run from RAM (the FDS's, loaded at $%04X) aren't supported\n", load);
        free(data);
        return NULL;
    }
    uint32_t start = bankswitched ? (load & (NSF_BANK - 1)) : (uint32_t)(load - 0x8000);
    nsf.bank_count = bankswitched ? ((start + length + NSF_BANK - 1) / NSF_BANK) : 8;
    nsf.banks = calloc(nsf.bank_count, NSF_BANK);
    if (nsf.banks == NULL) {
        free(data);
        return NULL;
    }
    uint32_t copy = length;
    if (start + copy > nsf.bank_count * NSF_BANK) {
        copy = nsf.bank_count * NSF_BANK - start;
    }
    memcpy(nsf.banks + start, header + NSF_HEADER, copy);
    for (int i = 0; i < 8; i++) {
        nsf.bank_init[i] = bankswitched ? header[0x70 + i] : i;
    }
    nsf.init = nsf_word(header + 0x0a);
    nsf.play = nsf_word(header + 0x0c);
    nsf.songs = header[0x06] ? header[0x06] : 1;
    nsf.song = (header[0x07] && (header[0x07] <= nsf.songs)) ? (header[0x07] - 1) : 0;
    uint16_t speed = nsf_word(header + 0x6e);
    nsf.period2 = speed ? (uint32_t)(((uint64_t)speed * 1789773 * 2) / 1000000) : NSF_FRAME2;
    uint8_t chips = header[0x7b];
    nsf.chip = (chips & NSF_CHIP_VRC6) ? NSF_CHIP_VRC6 : (chips & NSF_CHIP_VRC7) ? NSF_CHIP_VRC7 :
        (chips & NSF_CHIP_MMC5) ? NSF_CHIP_MMC5 : (chips & NSF_CHIP_N163) ? NSF_CHIP_N163 : (chips & NSF_CHIP_5B) ? NSF_CHIP_5B : 0;
    memcpy(nsf.title, header + 0x0e, 32);
    nsf.title[32] = 0;
    printf("NSF [%s] by [%.32s]: %u songs (from %u), load $%04X init $%04X play $%04X, %lu cycles a play, %s, chips $%02X\n",
        nsf.title, header + 0x2e, nsf.songs, nsf.song + 1, load, nsf.init, nsf.play, (unsigned long)(nsf.period2 / 2),
        bankswitched ? "banked" : "not banked", chips);
    // the cartridge: a NES 2.0 header for NSF_MAPPER, 32 kB of PRG (the window), 8 kB of PRG RAM and of CHR RAM
    uint32_t image_size = 16 + 0x8000;
    uint8_t* image = calloc(1, image_size);
    if (image == NULL) {
        free(nsf.banks);
        nsf.banks = NULL;
        free(data);
        return NULL;
    }
    memcpy(image, "NES\x1a", 4);
    image[4] = 2;
    image[5] = 0;
    image[6] = (NSF_MAPPER & 0x0f) << 4;
    image[7] = (NSF_MAPPER & 0xf0) | 0x08;
    image[8] = (NSF_MAPPER >> 8) & 0x0f;
    image[10] = 0x07;
    image[11] = 0x07;
    for (int i = 0; i < 8; i++) {
        memcpy(image + 16 + i * NSF_BANK, nsf.banks + (nsf.bank_init[i] % nsf.bank_count) * NSF_BANK, NSF_BANK);
    }
    free(data);
    *size = image_size;
    return image;
}

// the window's last 9 bytes: the driver's idle loop (JMP $FFF7) and the vectors
static void nsf_overlay(uint8_t* page) {
    static const uint8_t overlay[9] = { 0x4c, 0xf7, 0xff, NSF_DRIVER_NMI & 0xff, NSF_DRIVER_NMI >> 8, NSF_DRIVER & 0xff,
        NSF_DRIVER >> 8, NSF_DRIVER_RTI & 0xff, NSF_DRIVER_RTI >> 8 };
    memcpy(page + NSF_BANK - sizeof(overlay), overlay, sizeof(overlay));
}

// a 4 kB bank into the window (the CPU reads the window's pages through their MMU aliases: both purged)
static void nsf_set_bank(uint32_t slot, uint8_t bank) {
    nsf.bank[slot] = bank;
    uint8_t* page = ROM + slot * NSF_BANK;
    memcpy(page, nsf.banks + (bank % nsf.bank_count) * NSF_BANK, NSF_BANK);
    if (slot == 7) {
        nsf_overlay(page);
    }
    dcache_purge_range((uintptr_t)page, NSF_BANK);
    dcache_purge_range(ROMBANK0_ADDR + slot * NSF_BANK, NSF_BANK);
#ifdef FRNES_HOST
    // (host builds: the window is a copy of its pages, host_memory.c)
    memcpy((void*)(ROMBANK0_ADDR + slot * NSF_BANK), page, NSF_BANK);
#endif
}

// the song on the screen, in the frame display's capitals
static void nsf_show_song() {
    char line[33];
    int n = snprintf(line, sizeof(line), "%u/%u ", nsf.song + 1, nsf.songs);
    for (int i = 0; nsf.title[i] && (n < 32); i++) {
        char c = nsf.title[i];
        line[n++] = ((c >= 'a') && (c <= 'z')) ? (c - 32) : c;
    }
    line[n] = 0;
    frame_hud_message(line, 0x7fffffff);
}

void nsf_init() {
    if (nsf.banks == NULL) {
        return;
    }
    // (the window as the song starts, its last bytes the driver's)
    for (uint32_t i = 0; i < 8; i++) {
        nsf_set_bank(i, nsf.bank_init[i]);
    }
    memcpy(nsf.driver, nsf_driver_code, sizeof(nsf.driver));
    nsf.driver[NSF_DRIVER_INIT] = nsf.init & 0xff;
    nsf.driver[NSF_DRIVER_INIT + 1] = nsf.init >> 8;
    nsf.driver[NSF_DRIVER_PLAY] = nsf.play & 0xff;
    nsf.driver[NSF_DRIVER_PLAY + 1] = nsf.play >> 8;
    switch (nsf.chip) {
        case NSF_CHIP_VRC6: apu_synth_set_expansion(APU_EXPANSION_VRC6); break;
        case NSF_CHIP_VRC7: apu_synth_set_expansion(APU_EXPANSION_VRC7); break;
        case NSF_CHIP_MMC5: apu_synth_set_expansion(APU_EXPANSION_MMC5); break;
        case NSF_CHIP_N163: apu_synth_set_expansion(APU_EXPANSION_N163); break;
        case NSF_CHIP_5B: apu_synth_set_expansion(APU_EXPANSION_5B); break;
        default: apu_synth_set_expansion(APU_EXPANSION_NONE); break;
    }
    apu_synth_expansion_ready();
    // (the song's shown from the first frame: the frame display's texture needs the PVR, set up after the board)
    nsf.shown = 0;
    nsf.quiet = NSF_QUIET_FRAMES;
    apu_synth_muted = 1;
    nsf_visual_start();
}

uint8_t nsf_read_low(uint16_t address, uint8_t open_bus) {
    if ((address >= NSF_DRIVER) && (address < NSF_DRIVER + sizeof(nsf.driver))) {
        return nsf.driver[address - NSF_DRIVER];
    }
    switch (address) {
        case 0x5f00:
            // a song starts: its banks, and its first PLAY a period from now
            for (uint32_t i = 0; i < 8; i++) {
                nsf_set_bank(i, nsf.bank_init[i]);
            }
            nsf.n163_address = 0;
            memset(nsf.n163_ram, 0, sizeof(nsf.n163_ram));
            nsf.credit2 = 0;
            return nsf.song;
        case 0x5f01:
            return 0;
        case 0x5f04:
            // RAM and PRG RAM cleared, through the CPU's window (its RAM mirrors' lines dropped too)
            memset((void*)RAM_ADDR, 0, 0x800);
            dcache_purge_range(RAM_ADDR, 0x2000);
            if (WRAM != NULL) {
                memset((void*)WRAM_ADDR, 0, 0x2000);
                dcache_purge_range(WRAM_ADDR, 0x2000);
            }
            return 0;
        case 0x5f02:
            // a PLAY call due: one more period played out of the frames counted in
            if (nsf.credit2 >= nsf.period2) {
                nsf.credit2 -= nsf.period2;
                return 1;
            }
            return 0;
        case 0x5f03: {
            // a frame (the NMI handler's first read): counted in, and the pads, as the frame loop latches them: left /
            // right change the song
            nsf.credit2 += NSF_FRAME2;
            if (nsf.quiet && !--nsf.quiet) {
                apu_synth_muted = 0;
            }
            if (!nsf.shown) {
                nsf.shown = 1;
                nsf_show_song();
            }
            uint8_t pads = cpu_registers -> PAD1_Latch;
            uint8_t pressed = pads & ~nsf.pads;
            nsf.pads = pads;
            if (pressed & (NSF_PAD_LEFT | NSF_PAD_RIGHT)) {
                nsf.song = (pressed & NSF_PAD_RIGHT) ? ((nsf.song + 1) % nsf.songs) : ((nsf.song + nsf.songs - 1) % nsf.songs);
                nsf.restart = 1;
                nsf.quiet = NSF_QUIET_FRAMES;
                apu_synth_muted = 1;
                nsf_show_song();
            }
            uint8_t restart = nsf.restart;
            nsf.restart = 0;
            return restart;
        }
    }
    if ((nsf.chip == NSF_CHIP_N163) && ((address & 0xf800) == 0x4800)) {
        uint8_t value = nsf.n163_ram[nsf.n163_address & 0x7f];
        if (nsf.n163_address & 0x80) {
            nsf.n163_address = 0x80 | ((nsf.n163_address + 1) & 0x7f);
        }
        return value;
    }
    return open_bus;
}

void nsf_write_low(uint16_t address, uint8_t data) {
    if ((address >= 0x5ff8) && (address <= 0x5fff)) {
        nsf_set_bank(address - 0x5ff8, data);
        return;
    }
    if ((nsf.chip == NSF_CHIP_MMC5) && (address >= 0x5000) && (address <= 0x5015)) {
        apu_synth_log_write(address, data, cpu_cycle_now);
        return;
    }
    if ((nsf.chip == NSF_CHIP_N163) && ((address & 0xf800) == 0x4800)) {
        nsf.n163_ram[nsf.n163_address & 0x7f] = data;
        apu_synth_log_write(0x4800 | (nsf.n163_address & 0x7f), data, cpu_cycle_now);
        if (nsf.n163_address & 0x80) {
            nsf.n163_address = 0x80 | ((nsf.n163_address + 1) & 0x7f);
        }
    }
}

void nsf_write(uint16_t address, uint8_t data) {
    switch (nsf.chip) {
        case NSF_CHIP_VRC6: {
            uint32_t reg = (address & 0xf000) | (address & 3);
            if (((reg >= 0x9000) && (reg <= 0x9003)) || ((reg >= 0xa000) && (reg <= 0xa002)) || ((reg >= 0xb000) && (reg <= 0xb002))) {
                apu_synth_log_write(reg, data, cpu_cycle_now);
            }
        } break;
        case NSF_CHIP_VRC7:
            if ((address & 0xf000) == 0x9000) {
                apu_synth_log_write((address & 0x20) ? 0x9030 : 0x9010, data, cpu_cycle_now);
            }
            break;
        case NSF_CHIP_N163:
            if ((address & 0xf800) == 0xf800) {
                nsf.n163_address = data;
            }
            break;
        case NSF_CHIP_5B:
            if (((address & 0xe000) == 0xc000) || ((address & 0xe000) == 0xe000)) {
                apu_synth_log_write(address & 0xe000, data, cpu_cycle_now);
            }
            break;
    }
}
