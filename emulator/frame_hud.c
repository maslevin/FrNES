#include "frame_hud.h"
#include <kos.h>
#include <dc/perfctr.h>
#include "memory.h"

/**
 * The player's frame display (FRNES_FRAME_HUD): in the top left corner the frames per second over the last second,
 * the last frame's milliseconds of work and of waiting for sound, the share of the frame the SH4 spent frozen on cache
 * misses (PRFC1: operand cache misses, or instruction cache misses; KOS keeps PRFC0 for its timer), and a bar per
 * frame for the last 64 frames: its work stacked as emulation (CPU and devices; green when the whole frame's work is
 * under 14 ms, yellow up to the 16.7 ms budget, red over), the APU's sound (cyan), expansion audio (purple) and the rest
 * (presenting, saves: grey), with a white mark under frames the PVR couldn't take (not presented) and a blue one when
 * sound ran low.
 *
 * Recording a frame is a few stores into a ring (frame_hud_record, between frames); drawing is translucent polygons
 * added at the frame submit (frame_hud_draw, inside video_frame_end's translucent list), from an 8x8 glyph texture
 * made once at start-up. Nothing is drawn into the NES texture.
 */
#define HUD_FRAMES 64
#define GLYPH 8
#define GLYPHS_PER_ROW 16
#define FONT_W (GLYPH * GLYPHS_PER_ROW)
#define FONT_H (GLYPH * 2)

typedef struct {
    uint16_t work_us, wait_us;
    uint8_t flags;
} hud_frame_t;

static hud_frame_t hud_ring[HUD_FRAMES]; // (not "ring": apu_synth.c's is pinned by name, tools/bss_pins.ld)
static uint32_t ring_head = 0;
static pvr_ptr_t font_texture = NULL;
// the bar graph as a 64x64 ARGB4444 texture: row n is frame slot n of the ring, its pixels the bar from the left
// (500 us a pixel), written with one row of 32-bit stores per frame; drawn as one quad turned on its side, starting
// at the oldest row (the texture wraps)
#define BAR_US_PER_PIXEL 500
// the text: 16 characters on each of 3 lines, 12 pixels apart, rendered from the glyphs with 32-bit stores
#define TEXT_W 128
#define TEXT_H 64
#define TEXT_UPDATE_FRAMES 15
static pvr_ptr_t text_texture = NULL;
static uint32_t text_frames = 0;
static pvr_ptr_t bars_texture = NULL;
// 0 off, 1 operand cache misses, 2 instruction cache misses
static uint32_t hud_mode = 1;
static uint32_t miss_permille = 0;
static uint64_t second_start_us = 0;
static uint32_t second_frames = 0, fps_tenths = 0;

// the glyphs, 5x7 in an 8x8 cell, one string per row ('#' set): 0-9 . % F P S O C I m s : A B D E K U Y space, and
// the rest of the capitals for messages (frame_hud_message)
static const char glyph_chars[] = "0123456789.%FPSOCIms:ABDEKUY GHJLMNQRTVWXZ+/";
static const char* const glyph_rows[][7] = {
    { " ### ", "#   #", "#  ##", "# # #", "##  #", "#   #", " ### " },
    { "  #  ", " ##  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### " },
    { " ### ", "#   #", "    #", "   # ", "  #  ", " #   ", "#####" },
    { "#####", "   # ", "  #  ", "   # ", "    #", "#   #", " ### " },
    { "   # ", "  ## ", " # # ", "#  # ", "#####", "   # ", "   # " },
    { "#####", "#    ", "#### ", "    #", "    #", "#   #", " ### " },
    { "  ## ", " #   ", "#    ", "#### ", "#   #", "#   #", " ### " },
    { "#####", "    #", "   # ", "  #  ", " #   ", " #   ", " #   " },
    { " ### ", "#   #", "#   #", " ### ", "#   #", "#   #", " ### " },
    { " ### ", "#   #", "#   #", " ####", "    #", "   # ", " ##  " },
    { "     ", "     ", "     ", "     ", "     ", " ##  ", " ##  " },
    { "##   ", "##  #", "   # ", "  #  ", " #   ", "#  ##", "   ##" },
    { "#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#    " },
    { "#### ", "#   #", "#   #", "#### ", "#    ", "#    ", "#    " },
    { " ####", "#    ", "#    ", " ### ", "    #", "    #", "#### " },
    { " ### ", "#   #", "#   #", "#   #", "#   #", "#   #", " ### " },
    { " ### ", "#   #", "#    ", "#    ", "#    ", "#   #", " ### " },
    { " ### ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### " },
    { "     ", "     ", "## # ", "# # #", "# # #", "#   #", "#   #" },
    { "     ", "     ", " ####", "#    ", " ### ", "    #", "#### " },
    { "     ", " ##  ", " ##  ", "     ", " ##  ", " ##  ", "     " },
    { " ### ", "#   #", "#   #", "#####", "#   #", "#   #", "#   #" },
    { "#### ", "#   #", "#   #", "#### ", "#   #", "#   #", "#### " },
    { "#### ", "#   #", "#   #", "#   #", "#   #", "#   #", "#### " },
    { "#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#####" },
    { "#   #", "#  # ", "# #  ", "##   ", "# #  ", "#  # ", "#   #" },
    { "#   #", "#   #", "#   #", "#   #", "#   #", "#   #", " ### " },
    { "#   #", "#   #", " # # ", "  #  ", "  #  ", "  #  ", "  #  " },
    { "     ", "     ", "     ", "     ", "     ", "     ", "     " },
    { " ### ", "#   #", "#    ", "# ###", "#   #", "#   #", " ####" },
    { "#   #", "#   #", "#   #", "#####", "#   #", "#   #", "#   #" },
    { "  ###", "   # ", "   # ", "   # ", "   # ", "#  # ", " ##  " },
    { "#    ", "#    ", "#    ", "#    ", "#    ", "#    ", "#####" },
    { "#   #", "## ##", "# # #", "# # #", "#   #", "#   #", "#   #" },
    { "#   #", "#   #", "##  #", "# # #", "#  ##", "#   #", "#   #" },
    { " ### ", "#   #", "#   #", "#   #", "# # #", "#  # ", " ## #" },
    { "#### ", "#   #", "#   #", "#### ", "# #  ", "#  # ", "#   #" },
    { "#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  " },
    { "#   #", "#   #", "#   #", "#   #", "#   #", " # # ", "  #  " },
    { "#   #", "#   #", "#   #", "# # #", "# # #", "# # #", " # # " },
    { "#   #", "#   #", " # # ", "  #  ", " # # ", "#   #", "#   #" },
    { "#####", "    #", "   # ", "  #  ", " #   ", "#    ", "#####" },
    { "     ", "  #  ", "  #  ", "#####", "  #  ", "  #  ", "     " },
    { "    #", "    #", "   # ", "  #  ", " #   ", "#    ", "#    " },
};

// writes 'str' at character column 'col' of text line 'line' in 'color' (ARGB4444), clearing the cells it covers
static void glyphs_put(pvr_ptr_t texture, uint32_t width, uint32_t line, uint32_t col, const char* str, uint16_t color) {
    for (; *str && (col < width / GLYPH); str++, col++) {
        const char* p = strchr(glyph_chars, *str);
        // (anything else: the space)
        uint32_t g = p ? (uint32_t)(p - glyph_chars) : (uint32_t)(strchr(glyph_chars, ' ') - glyph_chars);
        for (uint32_t y = 0; y < GLYPH; y++) {
            volatile uint32_t* row = (volatile uint32_t*)((uintptr_t)texture + ((line * 12 + y) * width + col * GLYPH) * 2);
            for (uint32_t x = 0; x < GLYPH; x += 2) {
                uint32_t p0 = ((y < 7) && (x >= 1) && (x <= 5) && (glyph_rows[g][y][x - 1] == '#')) ? color : 0;
                uint32_t p1 = ((y < 7) && (x + 1 <= 5) && (glyph_rows[g][y][x] == '#')) ? color : 0;
                row[x / 2] = p0 | (p1 << 16);
            }
        }
    }
}

/*
 * The bars and the text are VQ textures whose codebook entries are 4 texels in a row (the NES picture's layout,
 * video.c): an index byte is 4 pixels. The bars' entries are one color each (the texture drawn 4 to 1 across, so a
 * byte is a bar pixel); the text's are (ink << 4) | pattern, a pattern's bits in an ink, the rest clear, so a glyph's
 * 8 pixel row is 2 bytes. The indices are written to the texture in 32-bit words
 */
#define HUD_CODEBOOK 2048
#define TEXT_ROW_BYTES (TEXT_W / 4)
#define BARS_ROW_BYTES 64
static const uint16_t text_inks[3] = { 0xffff, 0xf8cf, 0xffc4 };
static const uint16_t bar_colors[11] = { 0x0000, 0xe4e4, 0xeee4, 0xef44, 0xe4dd, 0xeb4f, 0xe888, 0x8fff, 0xffff, 0xf48f, 0xe1a1 };
enum { BAR_CLEAR, BAR_GREEN, BAR_YELLOW, BAR_RED, BAR_SYNTH, BAR_EXPANSION, BAR_OTHER, BAR_TARGET, BAR_SKIPPED, BAR_SOUND_LOW, BAR_PPU };

static void text_put(uint32_t line, uint32_t col, const char* str, uint16_t color) {
    uint32_t ink = (color == text_inks[1]) ? 1 : ((color == text_inks[2]) ? 2 : 0);
    for (; *str && (col < TEXT_W / GLYPH); str++, col++) {
        const char* p = strchr(glyph_chars, *str);
        uint32_t g = p ? (uint32_t)(p - glyph_chars) : (uint32_t)(strchr(glyph_chars, ' ') - glyph_chars);
        for (uint32_t y = 0; y < GLYPH; y++) {
            uint8_t bits = 0;
            for (uint32_t x = 0; (x < 5) && (y < 7); x++) {
                bits |= (glyph_rows[g][y][x] == '#') ? (0x40 >> x) : 0;
            }
            // (the cell's 2 bytes in their 32-bit word, read and written back whole: the PVR's memory takes no byte writes)
            uint32_t at = (line * 12 + y) * TEXT_ROW_BYTES + col * 2;
            volatile uint32_t* word = (volatile uint32_t*)((uintptr_t)text_texture + HUD_CODEBOOK + (at & ~3u));
            uint32_t pair = ((ink << 4) | (bits >> 4)) | (((ink << 4) | (bits & 15)) << 8);
            uint32_t shift = (at & 3) * 8;
            *word = (*word & ~(0xffffu << shift)) | (pair << shift);
        }
    }
}

/*
 * A message at the bottom of the screen for a while (the FDS's disk side), drawn whatever the frame display's mode and
 * in builds without it: one 32 character line, twice the size of the frame display's text
 */
// (up to three lines, 12 texels apart: each '\n' in a message starts the next)
#define MESSAGE_W 256
#define MESSAGE_H 32
#define MESSAGE_LINE_H 12
#define MESSAGE_LINES 3
static pvr_ptr_t message_texture = NULL;
static uint32_t message_frames = 0, message_length = 0, message_lines = 1;

// (each game's video_init starts the PVR again, its texture memory with it: the last game's message texture is gone,
// and drawing into it wrote over what the new one put there, as the game menu's texture)
FRNES_COLD_TEXT void frame_hud_message_reset(void) {
    message_texture = NULL;
    message_frames = 0;
}

void frame_hud_message(const char* text, uint32_t frames) {
    if (message_texture == NULL) {
        message_texture = pvr_mem_malloc(MESSAGE_W * MESSAGE_H * 2);
        if (message_texture == NULL) {
            printf("HUD: out of VRAM for the message\n");
            return;
        }
        // (clear: the rows between the lines are never written)
        volatile uint32_t* texels = (volatile uint32_t*)message_texture;
        for (uint32_t i = 0; i < MESSAGE_W * MESSAGE_H / 2; i++) {
            texels[i] = 0;
        }
    }
    // each line centred in the longest one's width
    const char* starts[MESSAGE_LINES];
    uint32_t lengths[MESSAGE_LINES];
    const char* at = text;
    message_lines = 0;
    message_length = 0;
    while (message_lines < MESSAGE_LINES) {
        const char* end = strchr(at, '\n');
        starts[message_lines] = at;
        lengths[message_lines] = end ? (uint32_t)(end - at) : strlen(at);
        message_length = (lengths[message_lines] > message_length) ? lengths[message_lines] : message_length;
        message_lines++;
        if (end == NULL) {
            break;
        }
        at = end + 1;
    }
    if (message_length > MESSAGE_W / GLYPH) {
        message_length = MESSAGE_W / GLYPH;
    }
    for (uint32_t l = 0; l < MESSAGE_LINES; l++) {
        uint32_t length = (l >= message_lines) ? 0 : ((lengths[l] > message_length) ? message_length : lengths[l]);
        uint32_t pad = (message_length - length) / 2;
        char line[MESSAGE_W / GLYPH + 1];
        snprintf(line, sizeof(line), "%*s%.*s%-*s", (int)pad, "", (int)length, (l < message_lines) ? starts[l] : "",
            (int)(MESSAGE_W / GLYPH - pad - length), "");
        glyphs_put(message_texture, MESSAGE_W, l, 0, line, 0xffff);
    }
    message_frames = frames;
    printf("MESSAGE %s\n", text);
}

static void quad(float x, float y, float w, float h, uint32_t argb, float u0, float v0, float u1, float v1);

static void message_draw() {
    message_frames--;
    pvr_poly_cxt_t context;
    pvr_poly_hdr_t header;
    const float texels = (message_lines - 1) * MESSAGE_LINE_H + GLYPH;
    const float scale = 2.0f, w = message_length * GLYPH * scale, h = texels * scale;
    const float x = (640.0f - w) / 2.0f, y = 480.0f - 48.0f - h;
    pvr_poly_cxt_col(&context, PVR_LIST_TR_POLY);
    pvr_poly_compile(&header, &context);
    pvr_prim(&header, sizeof(header));
    quad(x - 8.0f, y - 6.0f, w + 16.0f, h + 12.0f, 0xa0000000, 0, 0, 0, 0);
    pvr_poly_cxt_txr(&context, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED, MESSAGE_W, MESSAGE_H,
        message_texture, PVR_FILTER_NONE);
    pvr_poly_compile(&header, &context);
    pvr_prim(&header, sizeof(header));
    quad(x, y, w, h, 0xffffffff, 0.0f, 0.0f, (float)(message_length * GLYPH) / MESSAGE_W, texels / MESSAGE_H);
}

// (game_menu.c's text) a glyph's row (0-6) as 8 pixels, bit 7 the leftmost: the 5x7 glyph at columns 1-5; anything
// without a glyph is the space
FRNES_COLD_TEXT uint8_t frame_hud_glyph_row(char c, uint32_t row) {
    const char* p = strchr(glyph_chars, c);
    uint32_t g = p ? (uint32_t)(p - glyph_chars) : (uint32_t)(strchr(glyph_chars, ' ') - glyph_chars);
    uint8_t bits = 0;
    for (uint32_t x = 0; (x < 5) && (row < 7); x++) {
        if (glyph_rows[g][row][x] == '#') {
            bits |= 0x40 >> x;
        }
    }
    return bits;
}

// "12.3" from tenths
static void tenths(char* out, uint32_t value) {
    sprintf(out, "%lu.%lu", (unsigned long)(value / 10), (unsigned long)(value % 10));
}

// PRFC1's count at the last frame: the counter runs on and frames take the difference (KOS's perf_cntr_clear() stops
// the counter too, so clearing it every frame left it stopped after the first: the display read 0%)
static uint64_t perf_last;

static void perf_start() {
    perf_last = 0;
    perf_cntr_start(PRFC1, (hud_mode == 2) ? PMCR_PIPELINE_FREEZE_BY_ICACHE_MISS_MODE : PMCR_PIPELINE_FREEZE_BY_DCACHE_MISS_MODE,
        PMCR_COUNT_CPU_CYCLES);
}

int frame_hud_init() {
    static uint16_t pixels[FONT_W * FONT_H] __attribute__((aligned(32)));
    memset(pixels, 0, sizeof(pixels));
    // (the texture holds the first two rows' worth: the text and messages are drawn from glyph_rows)
    for (uint32_t g = 0; (g < sizeof(glyph_chars) - 1) && (g < GLYPHS_PER_ROW * FONT_H / GLYPH); g++) {
        uint32_t x0 = (g % GLYPHS_PER_ROW) * GLYPH, y0 = (g / GLYPHS_PER_ROW) * GLYPH;
        for (uint32_t y = 0; y < 7; y++) {
            for (uint32_t x = 0; x < 5; x++) {
                if (glyph_rows[g][y][x] == '#') {
                    pixels[(y0 + y) * FONT_W + x0 + x + 1] = 0xffff;
                }
            }
        }
    }
    font_texture = pvr_mem_malloc(sizeof(pixels));
    if (font_texture == NULL) {
        return -1;
    }
    pvr_txr_load(pixels, font_texture, sizeof(pixels));
    text_texture = pvr_mem_malloc(HUD_CODEBOOK + TEXT_ROW_BYTES * TEXT_H);
    bars_texture = pvr_mem_malloc(HUD_CODEBOOK + BARS_ROW_BYTES * 64);
    if ((text_texture == NULL) || (bars_texture == NULL)) {
        return -1;
    }
    // (the codebooks, built in the glyph buffer: text, every 4 pixel pattern in each ink; bars, each color whole)
    uint16_t* codebook = pixels;
    for (uint32_t e = 0; e < 256; e++) {
        uint32_t ink = e >> 4, pattern = e & 15;
        for (uint32_t t = 0; t < 4; t++) {
            codebook[e * 4 + t] = ((ink < 3) && (pattern & (8 >> t))) ? text_inks[ink] : 0x0000;
        }
    }
    pvr_txr_load(codebook, text_texture, HUD_CODEBOOK);
    for (uint32_t e = 0; e < 256; e++) {
        for (uint32_t t = 0; t < 4; t++) {
            codebook[e * 4 + t] = (e < sizeof(bar_colors) / sizeof(bar_colors[0])) ? bar_colors[e] : 0x0000;
        }
    }
    pvr_txr_load(codebook, bars_texture, HUD_CODEBOOK);
    memset(pixels, 0, sizeof(pixels));
    pvr_txr_load(pixels, (pvr_ptr_t)((uintptr_t)text_texture + HUD_CODEBOOK), TEXT_ROW_BYTES * TEXT_H);
    pvr_txr_load(pixels, (pvr_ptr_t)((uintptr_t)bars_texture + HUD_CODEBOOK), BARS_ROW_BYTES * 64);
    perf_start();
    second_start_us = timer_us_gettime64();
    return 0;
}

void frame_hud_wait_begin() {
    if (hud_mode) {
        perf_cntr_stop(PRFC1);
    }
}

void frame_hud_wait_end() {
    if (hud_mode) {
        perf_cntr_resume(PRFC1);
    }
}

void frame_hud_cycle_mode() {
    hud_mode = (hud_mode + 1) % 3;
    printf("HUD %s\n", (hud_mode == 0) ? "off" : ((hud_mode == 1) ? "on, operand cache misses" : "on, instruction cache misses"));
    if (hud_mode) {
        perf_start();
    }
}

// (the game menu's) the mode now: 0 off, 1 operand cache misses, 2 instruction cache misses
FRNES_COLD_TEXT uint32_t frame_hud_mode(void) {
    return hud_mode;
}

void frame_hud_set_mode(uint32_t mode) {
    hud_mode = mode % 3;
    if (hud_mode) {
        perf_start();
    }
}

static inline uint16_t clamp16(uint32_t us) {
    return (us > 0xffff) ? 0xffff : us;
}

void frame_hud_record(const hud_times_t* t, uint32_t flags, uint64_t frame_us) {
    hud_frame_t* f = &hud_ring[ring_head];
    uint32_t work = t -> emu_us + t -> synth_us + t -> expansion_us + t -> other_us;
    f -> work_us = clamp16(work);
    f -> wait_us = clamp16(t -> wait_us);
    f -> flags = flags;
    if (!hud_mode) {
        ring_head = (ring_head + 1) % HUD_FRAMES;
        return;
    }
    // this frame's row of the bar texture: the stacked bar (each segment ends where the running total does, so the
    // whole is the frame's work), then the not presented / sound low marks at the far end
    // (PPU_STATS builds: the emulation split into the PPU's catching up, dark green, and the rest, the CPU's; the timing
    // around every catch-up costs, so only diagnostic builds have it)
#ifdef PPU_STATS
    extern uint64_t ppu_stat_ppu_us;
    static uint64_t ppu_us_seen;
    uint32_t ppu_us = (uint32_t)(ppu_stat_ppu_us - ppu_us_seen);
    ppu_us_seen = ppu_stat_ppu_us;
    ppu_us = (ppu_us > t -> emu_us) ? t -> emu_us : ppu_us;
#else
    const uint32_t ppu_us = 0;
#endif
    const uint32_t ends[5] = { ppu_us, t -> emu_us, t -> emu_us + t -> synth_us, t -> emu_us + t -> synth_us + t -> expansion_us, work };
    const uint8_t colors[5] = { BAR_PPU, (work < 14000) ? BAR_GREEN : ((work <= 16683) ? BAR_YELLOW : BAR_RED), BAR_SYNTH,
        BAR_EXPANSION, BAR_OTHER };
    uint8_t pixels[64];
    uint32_t x = 0;
    for (uint32_t segment = 0; segment < 5; segment++) {
        uint32_t end = ends[segment] / BAR_US_PER_PIXEL;
        for (; (x < end) && (x < 56); x++) {
            pixels[x] = colors[segment];
        }
    }
    for (; x < 58; x++) {
        pixels[x] = (x == 34) ? BAR_TARGET : BAR_CLEAR;
    }
    for (; x < 64; x++) {
        pixels[x] = (x == 58) ? ((flags & HUD_NOT_PRESENTED) ? BAR_SKIPPED : BAR_CLEAR) :
            ((x == 62) ? ((flags & HUD_SOUND_LOW) ? BAR_SOUND_LOW : BAR_CLEAR) : BAR_CLEAR);
    }
    volatile uint32_t* row = (volatile uint32_t*)((uintptr_t)bars_texture + HUD_CODEBOOK + ring_head * BARS_ROW_BYTES);
    for (x = 0; x < 64; x += 4) {
        row[x / 4] = pixels[x] | ((uint32_t)pixels[x + 1] << 8) | ((uint32_t)pixels[x + 2] << 16) | ((uint32_t)pixels[x + 3] << 24);
    }
    ring_head = (ring_head + 1) % HUD_FRAMES;
    if (++text_frames >= TEXT_UPDATE_FRAMES) {
        text_frames = 0;
        char line[24], number[12];
        tenths(number, fps_tenths);
        snprintf(line, sizeof(line), "%-5s FPS", number);
        text_put(0, 0, line, 0xffff);
        tenths(number, f -> work_us / 100);
        snprintf(line, sizeof(line), "%-5sms", number);
        text_put(1, 0, line, 0xffff);
        tenths(number, f -> wait_us / 100);
        snprintf(line, sizeof(line), "%-5sms", number);
        text_put(1, 9, line, 0xf8cf);
        tenths(number, miss_permille);
        // (line 2 is frame_hud_perf.c's, the operand and instruction cache freezes: this old single counter now formats
        // as nothing, so writes nothing. Written, it showed in a frame rendered before frame_hud_perf.c wrote the line
        // again, a flashing "OC:". The format the same length and the code as it was: removing the call moved the
        // release's ordinary .text)
        snprintf(line, sizeof(line), "%.0s%.00s", (hud_mode == 2) ? "IC:" : "OC:", number);
        text_put(2, 0, line, 0xffc4);
    }
    // freeze cycles over the frame's work (200 MHz: 200 cycles a microsecond; the counter is stopped while the frame
    // waits for sound, whose AICA polling over the G2 bus would count too)
    uint64_t count = perf_cntr_count(PRFC1);
    uint64_t frozen = count - perf_last;
    perf_last = count;
    miss_permille = work ? (uint32_t)((frozen * 1000) / ((uint64_t)work * 200)) : 0;
    (void)frame_us;
    second_frames++;
    uint64_t now = timer_us_gettime64();
    if (now - second_start_us >= 1000000) {
        fps_tenths = (uint32_t)((second_frames * 10000000ull) / (now - second_start_us));
        second_frames = 0;
        second_start_us = now;
    }
}

static void quad(float x, float y, float w, float h, uint32_t argb, float u0, float v0, float u1, float v1) {
    pvr_vertex_t v;
    v.flags = PVR_CMD_VERTEX;
    v.z = 3.0f;
    v.argb = argb;
    v.oargb = 0;
    v.x = x;     v.y = y + h; v.u = u0; v.v = v1; pvr_prim(&v, sizeof(v));
    v.x = x;     v.y = y;     v.u = u0; v.v = v0; pvr_prim(&v, sizeof(v));
    v.x = x + w; v.y = y + h; v.u = u1; v.v = v1; pvr_prim(&v, sizeof(v));
    v.flags = PVR_CMD_VERTEX_EOL;
    v.x = x + w; v.y = y;     v.u = u1; v.v = v0; pvr_prim(&v, sizeof(v));
}


// the drawing's own cost, reported every 600 frames
static uint32_t draw_us_total, draw_us_max, draw_count;

static void frame_hud_draw_polys();

// drawn over the picture before the message (the NSF player's analyzer), NULL for none
void (*frame_hud_overlay)(void) FRNES_COLD_BSS;

void frame_hud_draw() {
    if (frame_hud_overlay) {
        frame_hud_overlay();
    }
    if (message_frames) {
        message_draw();
    }
    if (!hud_mode || (font_texture == NULL)) {
        return;
    }
    uint64_t start = timer_us_gettime64();
    frame_hud_draw_polys();
    uint32_t us = (uint32_t)(timer_us_gettime64() - start);
    draw_us_total += us;
    if (us > draw_us_max) {
        draw_us_max = us;
    }
    if (++draw_count == 600) {
        printf("HUD drawing %lu us a frame on average, %lu at most; %s %lu.%lu%%\n", (unsigned long)(draw_us_total / draw_count),
            (unsigned long)draw_us_max, (hud_mode == 2) ? "IC" : "OC", (unsigned long)(miss_permille / 10),
            (unsigned long)(miss_permille % 10));
        draw_us_total = draw_us_max = draw_count = 0;
    }
}

static void frame_hud_draw_polys() {
    const float x0 = 16.0f, y0 = 16.0f;
    pvr_poly_cxt_t context;
    pvr_poly_hdr_t header;
    // the background panel and the bars, untextured
    pvr_poly_cxt_col(&context, PVR_LIST_TR_POLY);
    pvr_poly_compile(&header, &context);
    pvr_prim(&header, sizeof(header));
    quad(x0 - 4.0f, y0 - 12.0f, 2.0f * HUD_FRAMES + 8.0f + 152.0f, 76.0f, 0xa0000000, 0, 0, 0, 0);
    // the bars: the texture's rows across, oldest first
    pvr_poly_cxt_txr(&context, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED | PVR_TXRFMT_VQ_ENABLE,
        BARS_ROW_BYTES * 4, 64, bars_texture, PVR_FILTER_NONE);
    pvr_poly_compile(&header, &context);
    pvr_prim(&header, sizeof(header));
    {
        // (oldest on the left: rows ring_head + 1 to the last, then 0 to ring_head - 1, in two quads, as the texture
        // doesn't wrap. Not row ring_head: frame_hud_record writes this frame's bar there before the PVR renders this
        // scene, so it would show the newest frame at the far left)
        const uint32_t first = (ring_head + 1) % HUD_FRAMES;
        const float bx = x0 + 154.0f, by = y0 + 56.0f, h = 64.0f;
        float split = 2.0f * (HUD_FRAMES - first);
        const float spans[2][4] = {
            { bx, split, (float)first / HUD_FRAMES, 1.0f },
            { bx + split, 2.0f * (HUD_FRAMES - 1) - split, 0.0f, (float)ring_head / HUD_FRAMES }
        };
        for (uint32_t i = 0; i < 2; i++) {
            if (spans[i][1] <= 0.0f) {
                continue;
            }
            pvr_vertex_t v;
            v.flags = PVR_CMD_VERTEX;
            v.z = 3.0f;
            v.argb = 0xffffffff;
            v.oargb = 0;
            float x = spans[i][0], w = spans[i][1], v0 = spans[i][2], v1 = spans[i][3];
            v.x = x;     v.y = by;     v.u = 0.0f; v.v = v0; pvr_prim(&v, sizeof(v));
            v.x = x;     v.y = by - h; v.u = 1.0f; v.v = v0; pvr_prim(&v, sizeof(v));
            v.x = x + w; v.y = by;     v.u = 0.0f; v.v = v1; pvr_prim(&v, sizeof(v));
            v.flags = PVR_CMD_VERTEX_EOL;
            v.x = x + w; v.y = by - h; v.u = 1.0f; v.v = v1; pvr_prim(&v, sizeof(v));
        }
    }
    // the text, one quad (the texture is rewritten a few times a second, see text_update)
    pvr_poly_cxt_txr(&context, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED | PVR_TXRFMT_VQ_ENABLE,
        TEXT_W, TEXT_H, text_texture, PVR_FILTER_NONE);
    pvr_poly_compile(&header, &context);
    pvr_prim(&header, sizeof(header));
    quad(x0, y0 - 8.0f, TEXT_W, TEXT_H, 0xffffffff, 0.0f, 0.0f, 1.0f, 1.0f);
}

// (frame_hud_perf.c's TMU1 interrupt handler)
void hud_perf_sample(irq_t code, irq_context_t* ctx, void* data);

// (frame_hud_perf.c's, at the frame's start: the display's state, as values; last in .text, tools/shlelf_hot.xc)
#ifndef FRNES_HOST
__attribute__((noinline, section(".text.~frnes_cold_hud")))
#endif
void frame_hud_emu_begin(uint32_t frame) {
    const hud_perf_view_t view = { frame, hud_mode, ring_head, text_frames, text_texture, bars_texture };
    if (hud_perf_begin(&view)) {
        irq_set_handler(EXC_TMU1_TUNI1, hud_perf_sample, NULL);
        hud_perf_start();
    }
}
