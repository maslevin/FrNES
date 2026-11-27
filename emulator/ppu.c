#include "ppu.h"
#include "mapper.h"
#include "cpu.h"

#define __ALIGN32__ __attribute__ ((aligned (32)))

#define PPU_DOTS_PER_SCANLINE   341
#define PPU_SCANLINES_PER_FRAME 262
#define PPU_VISIBLE_SCANLINES   240
#define PPU_VBLANK_SCANLINE     241
#define PPU_PRERENDER_SCANLINE  261

#define PPU_RENDERING_ENABLED() (ppu_registers -> PPUMASK & (PPUMASK_ENABLE_BACKGROUND_RENDERING | PPUMASK_ENABLE_SPRITE_RENDERING))

uint32_t ppu_frame_count = 0;
void (*ppu_bus_address_hook)(uint16_t address) = NULL;
uint32_t ppu_bus_frame_count = 0;

// set when palette RAM or the greyscale bit changes, so the renderer rebuilds its color tables
uint8_t ppu_palette_dirty = PPU_PALETTE_DIRTY_ALL;

__ALIGN32__ uint8_t ppu_framebuffer[PPU_FRAMEBUFFER_WIDTH * PPU_FRAMEBUFFER_HEIGHT];

// Current position, the dot is the next one to be emulated
static uint32_t ppu_scanline = 0;

// for mappers that watch the scanline (MMC5)
uint32_t ppu_current_scanline() {
    return ppu_scanline;
}

static uint32_t ppu_dot = 0;
// dots left over when ppu_run stops at the end of a frame, run first on the next call
static uint32_t ppu_pending_dots = 0;
static uint32_t ppu_frame_ready = 0;

// OAM is in use by sprite evaluation / fetching: on the visible and pre-render lines with rendering enabled. Returns
// the dot a CPU access made now lands on then (the dot after the one the PPU was brought up to, 1-341), 0 otherwise
uint32_t ppu_oam_busy_dot() {
    if (!PPU_RENDERING_ENABLED() || ((ppu_scanline >= PPU_VISIBLE_SCANLINES) && (ppu_scanline != PPU_PRERENDER_SCANLINE))) {
        return 0;
    }
    return ppu_dot + 1;
}
uint32_t ppu_open_bus_frame[8];

// dot of the current rendering scanline at which the mapper is clocked (A12 rising for the MMC3): 260 when sprites
// use the $1000 table, 324 when the background does and sprites don't
static inline uint32_t ppu_mapper_dot() {
    // MMC3 follows A12 (the sprite / background pattern table split), other mappers clock at a fixed dot
    if (mapper_clock_dot != 260) {
        return mapper_clock_dot;
    }
    uint8_t control = ppu_registers -> PPUCTRL;
    if ((control & PPUCTRL_BACKGROUND_TABLE_ADDRESS) && !(control & PPUCTRL_SPRITE_TABLE_ADDRESS) && !(control & PPUCTRL_SPRITE_SIZE)) {
        return 324;
    }
    return 260;
}

// MMC3 with only the background at $1000: A12, low since the last scanline's fetches, also rises at the pre-render
// line's first background pattern fetch (dot 5), a second clock on that line (given at the start of the line, see
// ppu_run)
#define PPU_MAPPER_PRERENDER_DOT 4
static inline uint32_t ppu_mapper_prerender_clock() {
    return (mapper_clock_dot == 260) && (ppu_mapper_dot() == 324);
}

// the dot A12 rises at for the clock mapper_ppu_clock() is being called for (ppu.h)
uint32_t ppu_mapper_clock_a12_dot = 0;

// clocks the mapper at most once per scanline and clock (a replay runs the same dots again)
static uint32_t ppu_mapper_clocked = 0;
static inline void ppu_clock_mapper_at(uint32_t dot, uint32_t clock) {
    if (!(ppu_mapper_clocked & clock)) {
        ppu_mapper_clocked |= clock;
        ppu_mapper_clock_a12_dot = dot + 1;
        mapper_ppu_clock();
    }
}
static inline void ppu_clock_mapper() {
    ppu_clock_mapper_at(ppu_mapper_dot(), 1);
}


// Where finished scanlines go: line y is the 256 bytes at ppu_output + y * 256, written as 32 bit words. When it is a
// store queue address each 32 byte block is flushed to VRAM as soon as it is complete.
static uint8_t* ppu_output = ppu_framebuffer;
static uint32_t ppu_output_sq = 0;

// per-dot rendering writes into this, it is copied to the output when the scanline ends
__ALIGN32__ static uint8_t ppu_line_buffer[PPU_FRAMEBUFFER_WIDTH];
static uint8_t ppu_line_buffer_used = 0;

// the color a visible scanline with rendering off shows: the backdrop, or the palette entry v points at in $3F00-$3FFF
static inline uint8_t ppu_blank_color() {
    uint32_t index = 0;
    if ((ppu_registers -> PPU_V & 0x3f00) == 0x3f00) {
        index = ppu_registers -> PPU_V & 0x1f;
        if ((index & 0x13) == 0x10) {
            index &= ~0x10;
        }
    }
    uint8_t color = ((uint8_t*)PALETTE_RAM_ADDR)[index];
    return (ppu_registers -> PPUMASK & PPUMASK_GREYSCALE) ? (color & 0x30) : color;
}
// the first dot of the scanline the rendering off fill hasn't been told about (ppu_blank_data_write fills ahead)
static uint32_t ppu_blank_filled_dot = 0;

void ppu_blank_address_write() {
    // Mesen 2: v takes a second $2006 write 3 dots after the access, so a scanline with rendering off shows the old
    // v's color until then
    if ((ppu_scanline >= PPU_VISIBLE_SCANLINES) || PPU_RENDERING_ENABLED() || (ppu_dot > PPU_FRAMEBUFFER_WIDTH)) {
        return;
    }
    uint8_t color = ppu_blank_color();
    for (uint32_t dot = ppu_dot; (dot < ppu_dot + 3) && (dot <= PPU_FRAMEBUFFER_WIDTH); dot++) {
        if (dot) {
            ppu_line_buffer[dot - 1] = color;
        }
    }
    ppu_line_buffer_used = 1;
    ppu_blank_filled_dot = ppu_dot + 3;
}

void ppu_blank_data_write(uint8_t data) {
    // Mesen 2: a $2007 write lands 5 dots after the access and v moves a dot later, so a scanline with rendering
    // off keeps showing the old color at the old v for 5 dots, then the written one for a dot (the PPU itself
    // takes the write right away: this is only what the scanline shows)
    if ((ppu_scanline >= PPU_VISIBLE_SCANLINES) || PPU_RENDERING_ENABLED() || (ppu_dot > PPU_FRAMEBUFFER_WIDTH)) {
        return;
    }
    uint8_t color = ppu_blank_color();
    uint8_t written = color;
    if ((ppu_registers -> PPU_V & 0x3f00) == 0x3f00) {
        written = (ppu_registers -> PPUMASK & PPUMASK_GREYSCALE) ? (data & 0x30) : (data & 0x3f);
    }
    for (uint32_t dot = ppu_dot; (dot <= ppu_dot + 5) && (dot <= PPU_FRAMEBUFFER_WIDTH); dot++) {
        if (dot) {
            ppu_line_buffer[dot - 1] = (dot == ppu_dot + 5) ? written : color;
        }
    }
    ppu_line_buffer_used = 1;
    ppu_blank_filled_dot = ppu_dot + 6;
}

// the per dot pass's line buffer, where pixel x of the scanline being drawn is once ppu_sync() has drawn it past x (the
// Zapper's light: mapper_boards.c, zapper_run)
FRNES_COLD_TEXT const uint8_t* ppu_line_buffer_address(void) {
    return ppu_line_buffer;
}

void ppu_set_output(uint8_t* output, int store_queues) {
    ppu_output = output;
    ppu_output_sq = store_queues;
}

// finishes a 32 byte block of output, pushing it to VRAM when writing through the store queues
#define PPU_OUTPUT_BLOCK_DONE(address) if (ppu_output_sq) { sq_flush(address); }

static FRNES_COLD void ppu_output_line(const uint8_t* line) {
    uint32_t* out = (uint32_t*)(ppu_output + (ppu_scanline << 8));
    const uint32_t* in = (const uint32_t*)line;
    for (uint32_t block = 0; block < 8; block++) {
        out[0] = in[0]; out[1] = in[1]; out[2] = in[2]; out[3] = in[3];
        out[4] = in[4]; out[5] = in[5]; out[6] = in[6]; out[7] = in[7];
        PPU_OUTPUT_BLOCK_DONE(out);
        out += 8;
        in += 8;
    }
}

static FRNES_COLD void ppu_output_fill_line(uint8_t color) {
    uint32_t* out = (uint32_t*)(ppu_output + (ppu_scanline << 8));
    uint32_t word = color * 0x01010101;
    for (uint32_t block = 0; block < 8; block++) {
        out[0] = word; out[1] = word; out[2] = word; out[3] = word;
        out[4] = word; out[5] = word; out[6] = word; out[7] = word;
        PPU_OUTPUT_BLOCK_DONE(out);
        out += 8;
    }
}


// Background fetch latches and shift registers
static uint8_t bg_next_tile;
// the background pattern table as of the tile's nametable fetch (Mesen forms the pattern address there, so a $2000
// write between the nametable and pattern fetches only affects the next tile)
static uint16_t bg_next_table;
static uint8_t bg_next_attribute;
static uint8_t bg_next_low;
static uint8_t bg_next_high;
static uint16_t bg_shift_pattern_low;
static uint16_t bg_shift_pattern_high;
static uint16_t bg_shift_attribute_low;
static uint16_t bg_shift_attribute_high;

// Sprites for the scanline being drawn. Evaluation at dot 257 fills the sprite_ arrays for the next
// scanline and the pattern fetch at dot 340 makes them current.
#define PPU_MAX_SPRITES_PER_LINE 8
static uint8_t sprite_count;
static uint8_t sprite_zero_on_line;
static uint8_t sprite_y[PPU_MAX_SPRITES_PER_LINE];
static uint8_t sprite_tile[PPU_MAX_SPRITES_PER_LINE];
static uint8_t sprite_attribute[PPU_MAX_SPRITES_PER_LINE];
static uint8_t sprite_x[PPU_MAX_SPRITES_PER_LINE];
static uint8_t sprite_pattern_low[PPU_MAX_SPRITES_PER_LINE];
static uint8_t sprite_pattern_high[PPU_MAX_SPRITES_PER_LINE];
static uint8_t next_sprite_count;
static uint8_t next_sprite_zero_on_line;

static void ppu_build_tables();

// the last dot of the odd frame's pre-render line at which rendering switching on still skips its last dot
#ifndef PPU_ODD_SKIP_LAST_DOT
#define PPU_ODD_SKIP_LAST_DOT 339
#endif

// where the PPU is when the CPU's first instruction starts
#ifndef PPU_POWER_ON_DOT
#define PPU_POWER_ON_DOT 28
#endif

// where the PPU is when the CPU starts, for a recording made on another emulator (INPUT_RECORDING_FLAG_NESHAWK_ALIGN):
// call after initialize_ppu, before the first frame runs
FRNES_COLD_TEXT void ppu_set_power_on_dot(uint32_t dot, uint32_t odd) {
    ppu_dot = dot;
    ppu_registers -> ODDFRAME = odd;
}

int initialize_ppu() {
    printf("ppu - initializing...\n");
    ppu_registers -> PPUCTRL = 0;
    ppu_registers -> PPUMASK = 0;
    ppu_registers -> PPUSTATUS = 0;
    ppu_registers -> OAMADDR = 0;
    ppu_registers -> PPU_W = 0;
    ppu_registers -> PPU_V = 0;
    ppu_registers -> PPU_T = 0;
    ppu_registers -> PPUDATA = 0;
#ifndef PPU_POWER_ON_ODD
#define PPU_POWER_ON_ODD 0
#endif
    ppu_registers -> ODDFRAME = PPU_POWER_ON_ODD;
    ppu_scanline = 0;
    ppu_dot = PPU_POWER_ON_DOT;
    ppu_frame_count = 0;
    ppu_pending_dots = 0;
    ppu_frame_ready = 0;
    sprite_count = next_sprite_count = 0;
    sprite_zero_on_line = next_sprite_zero_on_line = 0;
    memset(ppu_framebuffer, 0, sizeof(ppu_framebuffer));
    // palette RAM's contents at power on (as blargg measured them, and as Mesen 2 starts with)
    static const uint8_t palette_power_on[0x20] = {
        0x09, 0x01, 0x00, 0x01, 0x00, 0x02, 0x02, 0x0D, 0x08, 0x10, 0x08, 0x24, 0x00, 0x00, 0x04, 0x2C,
        0x09, 0x01, 0x34, 0x03, 0x00, 0x04, 0x00, 0x14, 0x08, 0x3A, 0x00, 0x02, 0x00, 0x20, 0x2C, 0x08
    };
    memcpy((uint8_t*)PALETTE_RAM_ADDR, palette_power_on, sizeof(palette_power_on));
    ppu_palette_dirty = PPU_PALETTE_DIRTY_ALL;
    ppu_build_tables();
    printf("...done\n");

    return 0;
}

void shutdown_ppu() {
    printf("ppu - shutting down ...\n");
    printf("...done\n");
}

// Pattern tables and nametables through their MMU mappings (nametable pages are kept coherent across aliases by
// ppu_nametable_write)
static inline uint8_t ppu_read_pattern(uint16_t address) {
    return ((uint8_t*)PATTERN_TABLES_ADDR)[address & 0x1fff];
}

/*
 * Mappers that watch or redirect the PPU's pattern fetches (MMC2 / MMC4 latches, MMC5 extended attributes and split
 * sprite CHR) get their own copies of the fetch paths: the functions below take a constant 'hooks' and are
 * instantiated twice, and each call site picks the copy with ppu_fetch_hooks (worked out once per scanline), so
 * the copies without hooks are the plain code. (Testing the mapper flags inside these paths cost the MM3 Needle Man
 * stage 1.1 ms per frame.) The hooked copies are named hooked_* so they stay out of the linker script's per-frame
 * .text.ppu_* group.
 */
#define ppu_fetch_hooks mapper_fetch_hooks

// the high plane byte of a pattern fetch, which is what MMC2 / MMC4 watch for tiles $FD / $FE
static inline __attribute__((always_inline)) uint8_t ppu_read_pattern_high(uint16_t address, const int hooks) {
    uint8_t value = ((uint8_t*)PATTERN_TABLES_ADDR)[address & 0x1fff];
    if (hooks && mapper_chr_latch) {
        mapper_latch_read(address & 0x1fff);
    }
    return value;
}

static inline uint8_t ppu_read_nametable(uint16_t address) {
    return ((uint8_t*)NAMETABLE0_ADDR)[address & 0x0fff];
}

/**
 * Loopy v register updates
 */
static inline void ppu_increment_x() {
    if ((ppu_registers -> PPU_V & 0x001f) == 0x001f) {
        ppu_registers -> PPU_V = (ppu_registers -> PPU_V & ~0x001f) ^ 0x0400;
    } else {
        ppu_registers -> PPU_V++;
    }
}

static inline void ppu_increment_y() {
    uint16_t v = ppu_registers -> PPU_V;
    if ((v & 0x7000) != 0x7000) {
        v += 0x1000;
    } else {
        v &= ~0x7000;
        uint16_t coarse_y = (v & 0x03e0) >> 5;
        if (coarse_y == 29) {
            coarse_y = 0;
            v ^= 0x0800;
        } else if (coarse_y == 31) {
            coarse_y = 0;
        } else {
            coarse_y++;
        }
        v = (v & ~0x03e0) | (coarse_y << 5);
    }
    ppu_registers -> PPU_V = v;
}

/*
 * OAM row corruption: turning rendering off during the secondary OAM clear (dots 0-63) or the sprite fetches (dots
 * 256-319) of a rendering line leaves a pending corruption of one OAM row (8 bytes), chosen by the dot. When rendering
 * starts again on a rendering line (or the next frame begins with it on), OAM row 0 is copied over that row.
 */
static uint32_t ppu_oam_corrupt_rows = 0;
#ifndef PPU_OAM_CORRUPT_DELAY
#define PPU_OAM_CORRUPT_DELAY 2
#endif

// The sprite X counters are loaded on dot 339 and only count down while rendering. When rendering comes back on part
// way through a line whose previous line passed dot 339 with it off, every object starts drawing at once, as if at X
// = 0 from that pixel: ppu_sprite_x_origin (0xffffffff when not in effect)
static uint32_t ppu_sprite_x_line = 0xffffffff;
static uint32_t ppu_sprite_x_origin = 0xffffffff;
// the last scanline whose sprite fetches (dots 257-320) ran with rendering on: when its dot 339 then passed with
// rendering off, those objects were loaded but never set counting, and the sprite unit keeps them "halted" with full
// shifters, so they are drawn at once, from the first pixel, on the next scanline that renders (AccuracyCoin's Stale
// Sprite Shift Regs test 3)
static uint32_t ppu_sprite_fetch_line = 0xffffffff;
// how many of that line's objects were fetched (rendering can stop part way through the fetches)
static uint32_t ppu_sprite_fetched = 0;

/*
 * OAMADDR is sprite evaluation's pointer (dots 65-256): rendering stopped part way through leaves it there, and
 * secondary OAM's address too (neither is reset until a line renders again). If rendering is on at the start of the
 * pre-render line with OAMADDR's row not secondary OAM's, the 2C02 copies OAMADDR's 8 byte row over that row (Mesen
 * 2: "OAM reliably corrupts if the row changes at the start of pre-render"; Rolling Thunder stops rendering mid
 * evaluation on line 219 and skips that frame's OAM DMA, and row 7 lands on row 0, taking sprite 0 off screen).
 */
static uint32_t ppu_evaluate_to(uint32_t scanline, uint32_t commit, uint32_t stop_dot);
static uint32_t ppu_evaluate_end_oamaddr, ppu_evaluate_end_secondary;
static uint32_t ppu_oam2_row = 0;

static void ppu_oam_corruption();

// the start of a pre-render line with rendering on: the OAM corruptions (out of the per line path, which is hot)
static __attribute__((noinline)) void ppu_prerender_oam() {
    if (ppu_oam_corrupt_rows) {
        ppu_oam_corruption();
    }
    if ((uint32_t)(ppu_registers -> OAMADDR >> 3) != ppu_oam2_row) {
        // OAMADDR's row over secondary OAM's row (see ppu_oam2_row)
        uint8_t* oam = (uint8_t*)SPRRAM_ADDR;
        memcpy(&oam[ppu_oam2_row << 3], &oam[(ppu_registers -> OAMADDR >> 3) << 3], 8);
        ppu_oam_generation++;
    }
    // (line 0 renders and starts secondary OAM's address over)
    ppu_oam2_row = 0;
}

// the objects loaded but never set counting become the ones drawn (see ppu_sprite_fetch_line)
static void ppu_take_stale_sprites() {
    sprite_count = (next_sprite_count < ppu_sprite_fetched) ? next_sprite_count : ppu_sprite_fetched;
    sprite_zero_on_line = next_sprite_zero_on_line;
    ppu_sprite_x_line = ppu_sprite_fetch_line;
}

static void ppu_oam_corruption() {
    uint8_t* oam = (uint8_t*)SPRRAM_ADDR;
    for (uint32_t row = 1; row < 32; row++) {
        if (ppu_oam_corrupt_rows & (1u << row)) {
            memcpy(&oam[row << 3], oam, 8);
        }
    }
    ppu_oam_corrupt_rows = 0;
    ppu_oam_generation++;
}

/*
 * The scroll counters (the coarse X / Y increments and the horizontal / vertical copies) see rendering switched on
 * two dots after the PPUMASK write's dot (Mesen 2: _prevRenderingEnabled follows _renderingEnabled a dot later):
 * Battletoads switches the background on at dot 255 of a scanline in its Wookie Hole descent and its sprite 0 hit
 * only lines up when that scanline's dot 256 Y increment doesn't happen (it froze waiting for the hit). The dots
 * before ppu_scroll_resume_dot run without them; it's reset at the start of each scanline.
 */
uint32_t ppu_scroll_resume_dot = 0;
#ifndef PPU_SCROLL_ENABLE_DELAY
#define PPU_SCROLL_ENABLE_DELAY 4
#endif

static void ppu_oam2_switched(uint32_t is_on);

// rendering switched on with a scanline IRQ armed: the batch was planned without its mapper clocks (no A12 edges while
// rendering is off), so it ends at the clock that can raise the IRQ (Hard Drivin', RAMBO-1: otherwise the IRQ is taken
// where the batch happens to end, cutcheck differs from frame 372)
__attribute__((cold, noinline)) static void ppu_mapper_clocks_resumed() {
    cpu_cut_batch_for_event(ppu_cycles_to_mapper_clock(mapper_clocks_to_irq()));
}

FRNES_COLD void ppu_rendering_switched(uint8_t old_mask, uint8_t new_mask) {
    uint32_t was_on = old_mask & (PPUMASK_ENABLE_BACKGROUND_RENDERING | PPUMASK_ENABLE_SPRITE_RENDERING);
    uint32_t is_on = new_mask & (PPUMASK_ENABLE_BACKGROUND_RENDERING | PPUMASK_ENABLE_SPRITE_RENDERING);
    if (!was_on == !is_on) {
        return;
    }
    if (is_on && mapper_irq_armed) {
        ppu_mapper_clocks_resumed();
    }
#ifdef PPU_RACE_DEBUG
    printf("RENDSWITCH sl=%lu dot=%lu on=%u rows=%08lx\n", (unsigned long)ppu_scanline, (unsigned long)ppu_dot, is_on ? 1 : 0,
        (unsigned long)ppu_oam_corrupt_rows);
#endif
    if ((ppu_scanline >= PPU_VISIBLE_SCANLINES) && (ppu_scanline != PPU_PRERENDER_SCANLINE)) {
        return;
    }
    if ((ppu_scanline == PPU_PRERENDER_SCANLINE) && ppu_registers -> ODDFRAME && (ppu_dot + ppu_pending_dots == 339)) {
        // The odd frame's pre-render dot 340 is skipped when rendering is on as the PPU passes dot 338 (Blargg's
        // 10-even_odd_timing), the renderer looks at dot 339 (and the line goes on to 341 once rendering is off): a
        // switch seen at dot 339 is corrected here, the per dot code left as it is. Switched on: no skip, the PPU is held
        // back a dot (the renderer skips one); switched off: skipped all the same, the PPU takes the dot now.
        if (is_on) {
            if (ppu_pending_dots) {
                ppu_pending_dots--;
            } else {
                ppu_dot--;
            }
        } else {
            ppu_pending_dots++;
        }
    }
    ppu_oam2_switched(is_on);
    if (is_on) {
        ppu_scroll_resume_dot = ppu_dot + PPU_SCROLL_ENABLE_DELAY;
        // a line evaluating sprites before the next pre-render starts secondary OAM's address over
        if ((ppu_scanline < PPU_VISIBLE_SCANLINES - 1) || (ppu_scanline == PPU_PRERENDER_SCANLINE) ||
            ((ppu_scanline == PPU_VISIBLE_SCANLINES - 1) && (ppu_dot < 65))) {
            ppu_oam2_row = 0;
        }
        if (ppu_oam_corrupt_rows) {
            ppu_oam_corruption();
        }
        uint32_t previous_line = ppu_scanline ? ppu_scanline - 1 : PPU_PRERENDER_SCANLINE;
        if ((ppu_scanline < PPU_VISIBLE_SCANLINES) && ppu_dot && (ppu_sprite_x_line != previous_line)) {
            ppu_sprite_x_origin = ppu_dot;
            // (what was fetched on a line whose dot 339 passed with rendering off is what the sprite unit holds:
            // Tecmo Bowl stops rendering in line 23's fetches and starts it again on line 169)
            if ((ppu_sprite_fetch_line != 0xffffffff) && (ppu_sprite_fetch_line != ppu_sprite_x_line)) {
                ppu_take_stale_sprites();
            }
        }
        return;
    }
    // the write lands at the end of the CPU cycle and the switch takes effect a dot after that
    uint32_t dot = ppu_dot + PPU_OAM_CORRUPT_DELAY;
    if ((ppu_scanline < PPU_VISIBLE_SCANLINES) && (dot >= 65) && (dot <= 256)) {
        // evaluation stops where it is: OAMADDR keeps its pointer (see ppu_oam2_row)
        ppu_evaluate_to(ppu_scanline, 0, dot);
        ppu_registers -> OAMADDR = ppu_evaluate_end_oamaddr;
        ppu_oam2_row = ppu_evaluate_end_secondary & 0x1f;
    }
    if (dot < 64) {
        ppu_oam_corrupt_rows |= 1u << (dot >> 1);
    } else if ((dot >= 256) && (dot < 320)) {
        uint32_t offset = (dot - 256) & 7;
        ppu_oam_corrupt_rows |= 1u << ((((dot - 256) >> 3) << 2) + ((offset < 3) ? offset : 3));
    }
}

// v after a $2007 access: +1 or +32, but while rendering the access collides with the PPU's own fetches and
// increments coarse X and Y at once
// secondary OAM as the last ppu_evaluate() left it
__ALIGN32__ static uint8_t secondary[32];

// 8 bytes at any alignment, without a call to memset
static inline __attribute__((always_inline)) void ppu_fill8(uint8_t* p, uint8_t value) {
    p[0] = value; p[1] = value; p[2] = value; p[3] = value;
    p[4] = value; p[5] = value; p[6] = value; p[7] = value;
}
static uint32_t ppu_evaluate_to(uint32_t scanline, uint32_t commit, uint32_t stop_dot);

/*
 * The address on the PPU's bus at a dot of a rendering scanline (the last fetch's): a $2007 read while rendering
 * reloads its buffer from there, not from v (Mesen 2's model). Background fetches go nametable, attribute, pattern
 * low, pattern high, two dots each from dot 1 of each 8; dots 257-320 fetch two garbage nametable bytes and the
 * sprite's pattern bytes for each of the 8 sprite slots (tile $FF for empty ones); 337-340 two nametable bytes.
 */
FRNES_COLD uint16_t ppu_rendering_bus_address(uint32_t dot) {
    uint16_t v = ppu_registers -> PPU_V;
    // (dot 257's nametable fetch comes before the X copy, and stays on the bus through 258)
    uint16_t v_fetched_257 = v;
    // v as it will be at that dot: the coarse X increments of dots 8, 16 ... 256, 328 and 336 in between
    for (uint32_t d = ppu_dot; (d < dot) && (d <= 336); d++) {
        if (!(d & 7) && ((d <= 256) || (d >= 328))) {
            v = ((v & 0x001f) == 0x001f) ? ((v & ~0x001f) ^ 0x0400) : (v + 1);
        }
        if (d == 256) {
            // the Y increment, then the X copy from t on 257
            if ((v & 0x7000) != 0x7000) {
                v += 0x1000;
            } else {
                v &= ~0x7000;
                uint16_t y = (v & 0x03e0) >> 5;
                if (y == 29) {
                    y = 0;
                    v ^= 0x0800;
                } else if (y == 31) {
                    y = 0;
                } else {
                    y++;
                }
                v = (v & ~0x03e0) | (y << 5);
            }
        } else if (d == 257) {
            v_fetched_257 = v;
            v = (v & ~0x041f) | (ppu_registers -> PPU_T & 0x041f);
        }
    }
    if (dot <= 257) {
        v_fetched_257 = v;
    }
    uint16_t nametable = 0x2000 | (v & 0x0fff);
    if (dot == 0) {
        dot = 336;
    }
    if (((dot >= 1) && (dot <= 256)) || ((dot >= 321) && (dot <= 336))) {
        uint32_t phase = (dot - 1) & 7;
        if (phase < 2) {
            return nametable;
        }
        if (phase < 4) {
            return 0x23c0 | (v & 0x0c00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07);
        }
        uint16_t tile = ((uint8_t*)NAMETABLE0_ADDR)[v & 0x0fff];
        uint16_t address = (tile << 4) | (v >> 12) | ((ppu_registers -> PPUCTRL & PPUCTRL_BACKGROUND_TABLE_ADDRESS) ? 0x1000 : 0);
        return (phase < 6) ? address : address + 8;
    }
    if ((dot >= 257) && (dot <= 320)) {
        uint32_t phase = (dot - 257) & 7;
        if ((dot - 257) < 2) {
            return 0x2000 | (v_fetched_257 & 0x0fff);
        }
        if (phase < 4) {
            return nametable;
        }
        uint32_t slot = (dot - 257) >> 3;
        ppu_evaluate_to(ppu_scanline, 0, 256);
        uint32_t height = (ppu_registers -> PPUCTRL & PPUCTRL_SPRITE_SIZE) ? 16 : 8;
        // (the row from the scanline and Y even when the slot is empty or out of range: $FF filled slots fetch tile
        // $FF's row (scanline - $FF), flipped by attribute $FF)
        uint32_t row = ppu_scanline - secondary[slot << 2];
        uint32_t tile = secondary[(slot << 2) + 1];
        uint32_t attribute = secondary[(slot << 2) + 2];
        uint16_t address;
        if (attribute & 0x80) {
            row ^= height - 1;
        }
        if (height == 16) {
            address = ((tile & 1) << 12) | ((tile & 0xfe) << 4) | ((row & 8) << 1) | (row & 7);
        } else {
            address = ((ppu_registers -> PPUCTRL & PPUCTRL_SPRITE_TABLE_ADDRESS) ? 0x1000 : 0) | (tile << 4) | (row & 7);
        }
        // (Mesen fetches both pattern bytes on the slot's 5th dot: the high byte's address stays on the bus)
        return address + 8;
    }
    return nametable;
}

void ppu_data_increment() {
    if (ppu_oam_busy_dot()) {
        ppu_increment_x();
        ppu_increment_y();
    } else {
        ppu_registers -> PPU_V += (ppu_registers -> PPUCTRL & PPUCTRL_VRAM_INCREMENT) ? 32 : 1;
    }
}

static inline void ppu_copy_x() {
    ppu_registers -> PPU_V = (ppu_registers -> PPU_V & ~0x041f) | (ppu_registers -> PPU_T & 0x041f);
}

static inline void ppu_copy_y() {
    ppu_registers -> PPU_V = (ppu_registers -> PPU_V & ~0x7be0) | (ppu_registers -> PPU_T & 0x7be0);
}

static inline void ppu_load_background_shifters() {
    bg_shift_pattern_low = (bg_shift_pattern_low & 0xff00) | bg_next_low;
    bg_shift_pattern_high = (bg_shift_pattern_high & 0xff00) | bg_next_high;
    bg_shift_attribute_low = (bg_shift_attribute_low & 0xff00) | ((bg_next_attribute & 1) ? 0xff : 0x00);
    bg_shift_attribute_high = (bg_shift_attribute_high & 0xff00) | ((bg_next_attribute & 2) ? 0xff : 0x00);
}

/**
 * Sprite evaluation, finds the sprites on the scanline after 'scanline': dots 65-256 of the hardware walk, one OAM
 * byte read on each odd dot and handled on the even one (Mesen's model). It starts wherever OAMADDR points, so a
 * misaligned address shifts which bytes are taken as Y / tile / attribute / X (and only resyncs when the byte read
 * after a copied object isn't itself in range), and once secondary OAM is full the overflow check walks OAM with
 * the hardware bug (object and byte index both advancing). Secondary OAM is cleared to $FF first, so a partially
 * copied object keeps $FF in its missing bytes.
 */
// the dot the last ppu_evaluate() found the overflow on
static uint32_t ppu_evaluate_overflow_dot;

// secondary OAM as the last ppu_evaluate() left it (declared above)

// the OAM copy buffer (what a $2004 read sees) at the dot ppu_evaluate_to() stopped at
static uint8_t ppu_copy_buffer;

static uint32_t ppu_evaluate_to(uint32_t scanline, uint32_t commit, uint32_t stop_dot);

static uint32_t ppu_evaluate(uint32_t scanline, uint32_t commit) {
    return ppu_evaluate_to(scanline, commit, 256);
}

static uint32_t ppu_evaluate_to(uint32_t scanline, uint32_t commit, uint32_t stop_dot) {
    uint32_t height = (ppu_registers -> PPUCTRL & PPUCTRL_SPRITE_SIZE) ? 16 : 8;
    const uint8_t* oam = (const uint8_t*)SPRRAM_ADDR;
    for (uint32_t i = 0; i < 8; i++) {
        ((uint32_t*)secondary)[i] = 0xffffffff;
    }
    uint32_t address_high = (ppu_registers -> OAMADDR >> 2) & 0x3f;
    uint32_t address_low = ppu_registers -> OAMADDR & 3;
    uint32_t secondary_address = 0;
    uint32_t in_range = 0;
    uint32_t copy_done = 0;
    uint32_t bug_counter = 0;
    uint32_t zero = 0;
    uint32_t overflow = 0;
    ppu_copy_buffer = 0xff;
    for (uint32_t dot = 65; dot <= 256; dot += 2) {
        uint8_t value = oam[(address_high << 2) | (address_low & 3)];
        if (dot > stop_dot) {
            break;
        }
        ppu_copy_buffer = value;
        if (dot + 1 > stop_dot) {
            break;
        }
        if (copy_done) {
            // with OAM walked to its end, writes are disabled and become reads of secondary OAM too
            ppu_copy_buffer = secondary[secondary_address & 0x1f];
            address_high = (address_high + 1) & 0x3f;
            continue;
        }
        if (!in_range && ((uint32_t)(scanline - value) < height)) {
            in_range = 1;
        }
        if (secondary_address >= 32) {
            // with secondary OAM full, its writes become reads
            ppu_copy_buffer = secondary[secondary_address & 0x1f];
        }
        if (secondary_address < 32) {
            secondary[secondary_address] = value;
            if (in_range) {
                // the first Y read being in range makes this "sprite 0", whatever OAMADDR started at
                if (dot == 65) {
                    zero = 1;
                }
                address_low++;
                secondary_address++;
                if (address_low >= 4) {
                    address_high = (address_high + 1) & 0x3f;
                    address_low = 0;
                    if (!address_high) {
                        copy_done = 1;
                    }
                }
                if (!(secondary_address & 3)) {
                    in_range = 0;
                    if (address_low && ((uint32_t)(scanline - value) >= height)) {
                        address_low = 0;
                    }
                }
            } else {
                address_high = (address_high + 1) & 0x3f;
                address_low = 0;
                if (!address_high) {
                    copy_done = 1;
                }
            }
        } else if (in_range) {
            // a ninth object in range: overflow, then the walk carries on for 3 more bytes and stops
            if (!overflow) {
                overflow = 1;
                ppu_evaluate_overflow_dot = dot;
            }
            if (++address_low == 4) {
                address_high = (address_high + 1) & 0x3f;
                address_low = 0;
            }
            if (!bug_counter) {
                bug_counter = 3;
            } else if (!--bug_counter) {
                copy_done = 1;
                address_low = 0;
            }
        } else {
            // the hardware overflow bug: both the object and the byte index advance
            address_high = (address_high + 1) & 0x3f;
            address_low = (address_low + 1) & 3;
            if (!address_high) {
                copy_done = 1;
            }
        }
    }
    // (where the walk stopped: OAMADDR is the walk's pointer on the hardware, see ppu_rendering_switched)
    ppu_evaluate_end_oamaddr = (address_high << 2) | (address_low & 3);
    ppu_evaluate_end_secondary = secondary_address;
    if (commit) {
        uint32_t found = (secondary_address + 3) >> 2;
        for (uint32_t i = 0; i < found; i++) {
            sprite_y[i] = secondary[i << 2];
            sprite_tile[i] = secondary[(i << 2) + 1];
            sprite_attribute[i] = secondary[(i << 2) + 2];
            sprite_x[i] = secondary[(i << 2) + 3];
        }
        next_sprite_count = found;
        next_sprite_zero_on_line = zero;
        if (overflow) {
            ppu_registers -> PPUSTATUS |= PPUSTATUS_SPRITE_OVERFLOW_FLAG;
        }
    }
    return overflow;
}

// what a $2004 read sees on a dot of a rendering line (Mesen's model): $FF while secondary OAM is cleared (dots 1-64),
// the OAM copy buffer during evaluation (65-256), the secondary OAM byte being fetched (257-320), then the first
// byte of secondary OAM
/*
 * The secondary OAM address only counts on the sprite fetch dots (257-320) that run with rendering on: switching
 * rendering off and on again within them leaves it short, and the reads of dots 321-340 see another byte than the
 * first (AccuracyCoin's Misaligned OAM2 Address). Rendering switching off at dot oam2_off_dot of line oam2_off_line,
 * and the increments missed on line oam2_missed_line.
 */
#ifndef PPU_OAM2_SWITCH_DELAY
#define PPU_OAM2_SWITCH_DELAY 3
#endif
static uint32_t oam2_off_dot, oam2_off_line = 0xffffffff, oam2_off_frame;
static uint32_t oam2_missed, oam2_missed_line = 0xffffffff, oam2_missed_frame;

static void ppu_oam2_switched(uint32_t is_on) {
    uint32_t dot = ppu_dot + PPU_OAM2_SWITCH_DELAY;
    if (!is_on) {
        oam2_off_dot = dot;
        oam2_off_line = ppu_scanline;
        oam2_off_frame = ppu_frame_count;
        return;
    }
    if ((oam2_off_line != ppu_scanline) || (oam2_off_frame != ppu_frame_count)) {
        return;
    }
    // (257 itself clears the address: its increment never counts)
    uint32_t missed = 0;
    for (uint32_t c = (oam2_off_dot > 258) ? oam2_off_dot : 258; (c < dot) && (c <= 320); c++) {
        missed += !((c - 1) & 4);
    }
    if (missed) {
        oam2_missed = missed;
        oam2_missed_line = ppu_scanline;
        oam2_missed_frame = ppu_frame_count;
    }
}

FRNES_COLD uint8_t ppu_oam_read_while_rendering(uint32_t dot) {
    if ((dot >= 1) && (dot <= 64)) {
        return 0xff;
    }
    uint32_t scanline = (dot == 0) ? ((ppu_scanline == 0) ? PPU_PRERENDER_SCANLINE : ppu_scanline - 1) : ppu_scanline;
    ppu_evaluate_to(scanline, 0, (dot >= 65) && (dot <= 256) ? dot : 256);
    if ((dot >= 65) && (dot <= 256)) {
        return ppu_copy_buffer;
    }
    if ((dot >= 257) && (dot <= 320)) {
        uint32_t step = (dot - 257) & 7;
        return secondary[(((dot - 257) >> 3) << 2) + ((step > 3) ? 3 : step)];
    }
    if ((dot >= 321) && (oam2_missed_line == ppu_scanline) && (oam2_missed_frame == ppu_frame_count)) {
        return secondary[(32 - oam2_missed) & 31];
    }
    return secondary[0];
}

/**
 * Sprite evaluation, finds the sprites on the scanline after 'scanline'.
 *
 * OAM changes rarely compared to how often it is evaluated (once per scanline), so the sprites in range of each
 * scanline are bucketed in OAM order once and reused until OAM (ppu_oam_generation, bumped by $2004 writes and OAM
 * DMA) or the sprite height changes. That is the same as the hardware walk when evaluation starts at OAMADDR 0 and
 * finds fewer than 8 sprites; anything else (a misaligned OAMADDR, 8 or more sprites, where the overflow bug's
 * walk decides the overflow flag) goes through ppu_evaluate().
 */
uint32_t ppu_oam_generation = 0;
static uint32_t oam_bucket_key = 0xffffffff;
static uint8_t oam_bucket_count[PPU_VISIBLE_SCANLINES];
static uint8_t oam_bucket[PPU_VISIBLE_SCANLINES][PPU_MAX_SPRITES_PER_LINE];

static void ppu_build_oam_buckets(uint32_t height) {
    const uint8_t* oam = (const uint8_t*)SPRRAM_ADDR;
    memset(oam_bucket_count, 0, sizeof(oam_bucket_count));
    for (uint32_t i = 0; i < 64; i++) {
        uint32_t y = oam[i << 2];
        for (uint32_t line = y; (line < y + height) && (line < PPU_VISIBLE_SCANLINES); line++) {
            uint32_t n = oam_bucket_count[line];
            if (n < PPU_MAX_SPRITES_PER_LINE) {
                oam_bucket[line][n] = i;
            }
            // counts past 8 only say "8 or more"
            if (n < 255) {
                oam_bucket_count[line] = n + 1;
            }
        }
    }
}

FRNES_HOT(5_evaluate) static void ppu_evaluate_sprites(uint32_t scanline) {
    if ((ppu_registers -> OAMADDR != 0) || (scanline >= PPU_VISIBLE_SCANLINES)) {
        ppu_evaluate(scanline, 1);
        return;
    }
    uint32_t height = (ppu_registers -> PPUCTRL & PPUCTRL_SPRITE_SIZE) ? 16 : 8;
    uint32_t key = (ppu_oam_generation << 1) | (height >> 4);
    if (key != oam_bucket_key) {
        oam_bucket_key = key;
        ppu_build_oam_buckets(height);
    }
    uint32_t count = oam_bucket_count[scanline];
    if (count >= PPU_MAX_SPRITES_PER_LINE) {
        ppu_evaluate(scanline, 1);
        return;
    }
    const uint8_t* oam = (const uint8_t*)SPRRAM_ADDR;
    const uint8_t* bucket = oam_bucket[scanline];
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t* sprite = &oam[bucket[i] << 2];
        sprite_y[i] = sprite[0];
        sprite_tile[i] = sprite[1];
        sprite_attribute[i] = sprite[2];
        sprite_x[i] = sprite[3];
    }
    next_sprite_count = count;
    // the first object evaluated (OAM entry 0 here) is the one that counts as sprite 0
    next_sprite_zero_on_line = count && (bucket[0] == 0);
}

// reverses the bits of a byte, for horizontally flipped sprites
static inline uint8_t ppu_reverse_bits(uint8_t b) {
    b = (b & 0xf0) >> 4 | (b & 0x0f) << 4;
    b = (b & 0xcc) >> 2 | (b & 0x33) << 2;
    b = (b & 0xaa) >> 1 | (b & 0x55) << 1;
    return b;
}

/**
 * Sprite pattern fetches for the sprites found by evaluation, 'scanline' is the line they will be drawn on
 */
// fetches the pattern row of object i of secondary OAM for 'scanline' (with the sprite size at the time)
static inline __attribute__((always_inline)) void ppu_fetch_sprite_impl(uint32_t i, uint32_t scanline, const int hooks) {
    uint32_t height = (ppu_registers -> PPUCTRL & PPUCTRL_SPRITE_SIZE) ? 16 : 8;
    uint32_t row = (scanline - 1 - sprite_y[i]) & 0x0f;
    // The pre-render line fetches for scanline 0 from what secondary OAM still holds (normally the objects of
    // the line after 239, which never show), with the in-range check done against line 261 & 255 = 5: an
    // object there is drawn on scanline 0 with row 5 - Y
    if (scanline == 0) {
        row = 5 - sprite_y[i];
        if (row >= height) {
            sprite_pattern_low[i] = sprite_pattern_high[i] = 0;
            return;
        }
    }
    if (sprite_attribute[i] & 0x80) {
        row = (height - 1) - row;
    }
    uint16_t address;
    if (height == 16) {
        address = ((sprite_tile[i] & 1) << 12) | ((sprite_tile[i] & 0xfe) << 4);
        if (row >= 8) {
            address += 16;
        }
    } else {
        address = ((ppu_registers -> PPUCTRL & PPUCTRL_SPRITE_TABLE_ADDRESS) ? 0x1000 : 0) | (sprite_tile[i] << 4);
    }
    address += row & 7;
    uint8_t low, high;
    if (hooks && mapper_sprite_chr_split && (height == 16)) {
        const uint8_t* bytes = mapper_sprite_chr(address);
        low = bytes[0];
        high = bytes[8];
    } else {
        low = ppu_read_pattern(address);
        high = ppu_read_pattern_high(address + 8, hooks);
    }
    if (sprite_attribute[i] & 0x40) {
        low = ppu_reverse_bits(low);
        high = ppu_reverse_bits(high);
    }
    sprite_pattern_low[i] = low;
    sprite_pattern_high[i] = high;
}

static __attribute__((noinline)) void hooked_fetch_sprite(uint32_t i, uint32_t scanline) {
    ppu_fetch_sprite_impl(i, scanline, 1);
}

/*
 * Once a frame, at the start of the pre-render line, with 8x16 sprites (cold): A12 follows the tiles the line's sprite
 * fetches read from secondary OAM (bit 0: odd tiles are at $1000), which on the pre-render line is whatever it still
 * holds: the objects found for line 240, the rest $FF from the clear, or, before any line has been evaluated since
 * power on, zeros (Mesen 2; Rolling Thunder's first frame with rendering on gets no clock there: its RAMBO-1 IRQs came
 * a line early). Without an odd tile, A12 doesn't rise and the line gets no clock.
 */
__attribute__((cold, noinline)) static void ppu_mapper_prerender_8x16() {
    if (mapper_clock_dot == 260) {
        // (an evaluation has run: the fast path's buckets were built, or ppu_evaluate() filled secondary OAM)
        uint32_t evaluated = (oam_bucket_key != 0xffffffff) || (((uint32_t*)secondary)[7] != 0);
        uint32_t odd = (next_sprite_count < PPU_MAX_SPRITES_PER_LINE) && evaluated;
        for (uint32_t i = 0; (i < next_sprite_count) && !odd; i++) {
            odd = sprite_tile[i] & 1;
        }
        if (!odd) {
            // (the line's A12 clock is taken: ppu_clock_mapper_at skips it)
            ppu_mapper_clocked |= 1;
        }
    }
}

FRNES_HOT(6_fetch) static void ppu_fetch_sprites(uint32_t scanline) {
    if (__builtin_expect(ppu_fetch_hooks, 0)) {
        for (uint32_t i = 0; i < next_sprite_count; i++) {
            hooked_fetch_sprite(i, scanline);
        }
    } else {
        for (uint32_t i = 0; i < next_sprite_count; i++) {
            ppu_fetch_sprite_impl(i, scanline, 0);
        }
    }
    sprite_count = next_sprite_count;
    sprite_zero_on_line = next_sprite_zero_on_line;
}

/**
 * Composes the output pixel at x on the current scanline
 */
static inline void ppu_render_pixel(uint32_t x) {
    uint8_t mask = ppu_registers -> PPUMASK;
    uint8_t bg_pixel = 0;
    uint8_t bg_palette = 0;

    if ((mask & PPUMASK_ENABLE_BACKGROUND_RENDERING) && ((x >= 8) || (mask & PPUMASK_SHOW_BACKGROUND_IN_LEFTMOST_8_PIXELS))) {
        uint16_t bit = 0x8000 >> ppu_registers -> PPU_X;
        bg_pixel = ((bg_shift_pattern_low & bit) ? 1 : 0) | ((bg_shift_pattern_high & bit) ? 2 : 0);
        bg_palette = ((bg_shift_attribute_low & bit) ? 1 : 0) | ((bg_shift_attribute_high & bit) ? 2 : 0);
    }

    uint8_t sprite_pixel = 0;
    uint8_t sprite_palette = 0;
    uint8_t sprite_behind = 0;
    if (sprite_count && (mask & PPUMASK_ENABLE_SPRITE_RENDERING) && ((x >= 8) || (mask & PPUMASK_SHOW_SPRITES_IN_LEFTMOST_8_PIXELS))) {
        for (uint32_t i = 0; i < sprite_count; i++) {
            uint32_t offset = x - ((ppu_sprite_x_origin != 0xffffffff) ? ppu_sprite_x_origin : sprite_x[i]);
            if (offset >= 8) {
                continue;
            }
            uint8_t pixel = ((sprite_pattern_low[i] >> (7 - offset)) & 1) | (((sprite_pattern_high[i] >> (7 - offset)) & 1) << 1);
            if (!pixel) {
                continue;
            }
            if ((i == 0) && sprite_zero_on_line && bg_pixel && (x != 255)) {
                ppu_registers -> PPUSTATUS |= PPUSTATUS_SPRITE_0_HIT_FLAG;
            }
            sprite_pixel = pixel;
            sprite_palette = sprite_attribute[i] & 3;
            sprite_behind = sprite_attribute[i] & 0x20;
            break;
        }
    }

    uint8_t palette_index;
    if (sprite_pixel && (!bg_pixel || !sprite_behind)) {
        palette_index = 0x10 | (sprite_palette << 2) | sprite_pixel;
    } else if (bg_pixel) {
        palette_index = (bg_palette << 2) | bg_pixel;
    } else {
        palette_index = 0;
    }

    uint8_t color = ((uint8_t*)PALETTE_RAM_ADDR)[palette_index];
    if (mask & PPUMASK_GREYSCALE) {
        color &= 0x30;
    }
    ppu_line_buffer[x] = color;
    ppu_line_buffer_used = 1;
}

/**
 * Emulates one dot of a visible or pre-render scanline with rendering enabled
 */
static inline __attribute__((always_inline)) void ppu_rendering_dot_impl(const int hooks);
static __attribute__((noinline)) void hooked_rendering_dot() {
    ppu_rendering_dot_impl(1);
}

static inline void ppu_rendering_dot() {
    if (__builtin_expect(ppu_fetch_hooks, 0)) {
        hooked_rendering_dot();
    } else {
        ppu_rendering_dot_impl(0);
    }
}

static inline __attribute__((always_inline)) void ppu_rendering_dot_impl(const int hooks) {
    uint32_t dot = ppu_dot;

    if (mapper_clock_dot && (dot == ppu_mapper_dot())) {
        ppu_clock_mapper();
    }

    if (((dot >= 2) && (dot <= 257)) || ((dot >= 321) && (dot <= 337))) {
        bg_shift_pattern_low <<= 1;
        bg_shift_pattern_high <<= 1;
        bg_shift_attribute_low <<= 1;
        bg_shift_attribute_high <<= 1;

        uint16_t v = ppu_registers -> PPU_V;
        switch ((dot - 1) & 7) {
            case 0:
                ppu_load_background_shifters();
                bg_next_tile = ppu_read_nametable(0x2000 | (v & 0x0fff));
                bg_next_table = (ppu_registers -> PPUCTRL & PPUCTRL_BACKGROUND_TABLE_ADDRESS) ? 0x1000 : 0;
                break;
            case 2:
                bg_next_attribute = ppu_read_nametable(0x23c0 | (v & 0x0c00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07));
                if (v & 0x0040) {
                    bg_next_attribute >>= 4;
                }
                if (v & 0x0002) {
                    bg_next_attribute >>= 2;
                }
                if (hooks && mapper_ext_attributes) {
                    uint32_t palette;
                    mapper_ext_tile(v & 0x03ff, bg_next_tile, &palette);
                    bg_next_attribute = palette;
                }
                break;
            case 4:
                if (hooks && mapper_ext_attributes) {
                    uint32_t palette;
                    bg_next_low = mapper_ext_tile(v & 0x03ff, bg_next_tile, &palette)[(v >> 12) & 7];
                    break;
                }
                bg_next_low = ppu_read_pattern(bg_next_table | (bg_next_tile << 4) | ((v >> 12) & 7));
                break;
            case 6:
                if (hooks && mapper_ext_attributes) {
                    uint32_t palette;
                    bg_next_high = mapper_ext_tile(v & 0x03ff, bg_next_tile, &palette)[((v >> 12) & 7) | 8];
                    break;
                }
                bg_next_high = ppu_read_pattern_high(bg_next_table | (bg_next_tile << 4) | ((v >> 12) & 7) | 8, hooks);
                break;
            case 7:
                if (dot >= ppu_scroll_resume_dot) {
                    ppu_increment_x();
                }
                break;
        }
    }

    if (dot == 256) {
        if (dot >= ppu_scroll_resume_dot) {
            ppu_increment_y();
        }
    } else if (dot == 257) {
        ppu_load_background_shifters();
        if (dot >= ppu_scroll_resume_dot) {
            ppu_copy_x();
        }
        // (the pre-render line doesn't evaluate: secondary OAM keeps what the last evaluation left, see
        // ppu_fetch_sprites)
        if (ppu_scanline < PPU_VISIBLE_SCANLINES) {
            ppu_evaluate_sprites(ppu_scanline);
        }
        // OAMADDR is cleared during the sprite fetches (dots 257-320) of every rendering scanline
        ppu_registers -> OAMADDR = 0;
    } else if (dot == 339) {
        // (the sprite X counters are loaded here: see ppu_rendering_switched)
        ppu_sprite_x_line = ppu_scanline;
    } else if (dot == 340) {
        sprite_count = next_sprite_count;
        sprite_zero_on_line = next_sprite_zero_on_line;
    }
    if ((dot >= 280) && (dot <= 304) && (ppu_scanline == PPU_PRERENDER_SCANLINE) && (dot >= ppu_scroll_resume_dot)) {
        ppu_copy_y();
    }
    if ((dot >= 264) && (dot <= 320) && !(dot & 7)) {
        // each object's pattern is fetched in its own 8 dots (257-320), with the sprite size at that time; the
        // objects fetched so far are what the sprite unit holds if rendering stops before the end of the line
        uint32_t i = (dot - 264) >> 3;
        ppu_sprite_fetch_line = ppu_scanline;
        ppu_sprite_fetched = i + 1;
        if (i < next_sprite_count) {
            ppu_fetch_sprite_impl(i, (ppu_scanline == PPU_PRERENDER_SCANLINE) ? 0 : ppu_scanline + 1, hooks);
            sprite_count = i + 1;
            sprite_zero_on_line = next_sprite_zero_on_line;
        } else if (i == 0) {
            sprite_count = 0;
        }
    }

    if ((dot >= 1) && (dot <= 256) && (ppu_scanline < PPU_VISIBLE_SCANLINES)) {
        ppu_render_pixel(dot - 1);
    }
}

/**
 * Lazy scanline rendering
 *
 * Emulating every dot is too slow on the SH4, so a rendering scanline normally starts in LAZY mode: the PPU only
 * advances its dot counter, and when the scanline ends the visible pixels are produced in one pass with tile span
 * loops (ppu_render_line_fast), the v register gets the summarized effect of dots 1-256, and dots 257-340 (sprite
 * evaluation, scroll copies, prefetch) run through the per-dot code. Reads of $2002 in the middle of a lazy
 * scanline are answered from the sprite 0 hit / overflow positions predicted by the fast pass. Anything that can
 * change rendering mid-scanline (a PPU register write, $2004 / $2007 reads, OAM DMA) calls ppu_sync(), which
 * restores the state saved at dot 0 and replays the scanline dot by dot up to the current dot, after which the
 * rest of that scanline stays in PERDOT mode. The per-dot path is the accuracy reference.
 */
#ifdef PPU_STATS
uint32_t ppu_stat_lazy_lines, ppu_stat_replays, ppu_stat_replay_dots, ppu_stat_perdot_dots, ppu_stat_fast_renders;
uint32_t ppu_stat_run_calls, ppu_stat_status_syncs, ppu_stat_replays_hblank, ppu_stat_replays_early, ppu_stat_replays_tile, ppu_stat_replays_tile_dots;
uint64_t ppu_stat_render_us, ppu_stat_tail_us, ppu_stat_apu_us, ppu_stat_ppu_us;
#define PPU_STAT(x) (x)
#else
#define PPU_STAT(x)
#endif

uint8_t ppu_line_mode = PPU_LINE_PERDOT;

// prediction for the current lazy scanline, valid once the fast pass has run
static uint8_t lazy_line_rendered;
// fine X changes during the current lazy scanline ($2005 first writes, see ppu_scroll_write_lazy): from pixel
// lazy_x_pixel[i] on, fine X is lazy_x_value[i] & 7; before the first, line_start.fine_x. Bit 3 (LAZY_SPRITES_TOGGLE):
// PPUMASK's sprite enable flips at that pixel ($2001 writes, ppu_mask_write_lazy), so before the first switch it is
// PPUMASK's as it is now, flipped once for each switch with the bit
#define LAZY_SPRITES_TOGGLE 0x08
// (bit 4, LAZY_V_SWITCH: a second $2006 write at dot pixel + 1 set v to lazy_v_value, see ppu_other_write_lazy; fine X
// unchanged)
#define LAZY_V_SWITCH 0x10
#define PPU_LAZY_X_SWITCHES 8
static uint8_t lazy_x_switches;
static uint8_t lazy_x_pixel[PPU_LAZY_X_SWITCHES], lazy_x_value[PPU_LAZY_X_SWITCHES];
static uint32_t lazy_hit_dot;           // sprite 0 hit is set once ppu_dot passes this, 0 = no hit
// the last v switch (LAZY_V_SWITCH): the dot it happened at and the v it set. Not cleared at a line's start (no store
// on the hot path): nonzero once one has happened, the line's switch list says whether this line has one
// (in the cold .bss, after everything else's: no other variable moves, and only the switch paths read them)
static FRNES_COLD_BSS uint16_t lazy_v_dot, lazy_v_value;
#ifdef FRNES_HOST
#include <stdlib.h>
static int host_midv_off = -1;
uint8_t host_midv_lines[240];
uint32_t host_midv_count;
#endif

// state at dot 0 of the current scanline
static struct {
    uint16_t v;
    uint8_t bg_next_tile, bg_next_attribute, bg_next_low, bg_next_high;
    uint16_t bg_next_table;
    uint16_t bg_shift_pattern_low, bg_shift_pattern_high, bg_shift_attribute_low, bg_shift_attribute_high;
    uint8_t sprite_count, sprite_zero_on_line, next_sprite_count, next_sprite_zero_on_line;
    uint8_t sprite_y[PPU_MAX_SPRITES_PER_LINE], sprite_tile[PPU_MAX_SPRITES_PER_LINE];
    uint8_t sprite_attribute[PPU_MAX_SPRITES_PER_LINE], sprite_x[PPU_MAX_SPRITES_PER_LINE];
    uint8_t sprite_pattern_low[PPU_MAX_SPRITES_PER_LINE], sprite_pattern_high[PPU_MAX_SPRITES_PER_LINE];
    uint16_t mapper_latches;
    uint8_t fine_x;
} line_start;

static void ppu_save_line_start() {
    line_start.v = ppu_registers -> PPU_V;
    line_start.fine_x = ppu_registers -> PPU_X;
    lazy_x_switches = 0;
    line_start.bg_next_tile = bg_next_tile;
    line_start.bg_next_table = bg_next_table;
    line_start.bg_next_attribute = bg_next_attribute;
    line_start.bg_next_low = bg_next_low;
    line_start.bg_next_high = bg_next_high;
    line_start.bg_shift_pattern_low = bg_shift_pattern_low;
    line_start.bg_shift_pattern_high = bg_shift_pattern_high;
    line_start.bg_shift_attribute_low = bg_shift_attribute_low;
    line_start.bg_shift_attribute_high = bg_shift_attribute_high;
    line_start.sprite_count = sprite_count;
    line_start.sprite_zero_on_line = sprite_zero_on_line;
    line_start.next_sprite_count = next_sprite_count;
    line_start.next_sprite_zero_on_line = next_sprite_zero_on_line;
    memcpy(line_start.sprite_y, sprite_y, PPU_MAX_SPRITES_PER_LINE);
    memcpy(line_start.sprite_tile, sprite_tile, PPU_MAX_SPRITES_PER_LINE);
    memcpy(line_start.sprite_attribute, sprite_attribute, PPU_MAX_SPRITES_PER_LINE);
    memcpy(line_start.sprite_x, sprite_x, PPU_MAX_SPRITES_PER_LINE);
    memcpy(line_start.sprite_pattern_low, sprite_pattern_low, PPU_MAX_SPRITES_PER_LINE);
    memcpy(line_start.sprite_pattern_high, sprite_pattern_high, PPU_MAX_SPRITES_PER_LINE);
    if (__builtin_expect(ppu_fetch_hooks, 0) && mapper_chr_latch) {
        line_start.mapper_latches = mapper_latch_state();
    }
}

static void ppu_restore_line_start() {
    ppu_registers -> PPU_V = line_start.v;
    bg_next_tile = line_start.bg_next_tile;
    bg_next_table = line_start.bg_next_table;
    bg_next_attribute = line_start.bg_next_attribute;
    bg_next_low = line_start.bg_next_low;
    bg_next_high = line_start.bg_next_high;
    bg_shift_pattern_low = line_start.bg_shift_pattern_low;
    bg_shift_pattern_high = line_start.bg_shift_pattern_high;
    bg_shift_attribute_low = line_start.bg_shift_attribute_low;
    bg_shift_attribute_high = line_start.bg_shift_attribute_high;
    sprite_count = line_start.sprite_count;
    sprite_zero_on_line = line_start.sprite_zero_on_line;
    next_sprite_count = line_start.next_sprite_count;
    next_sprite_zero_on_line = line_start.next_sprite_zero_on_line;
    memcpy(sprite_y, line_start.sprite_y, PPU_MAX_SPRITES_PER_LINE);
    memcpy(sprite_tile, line_start.sprite_tile, PPU_MAX_SPRITES_PER_LINE);
    memcpy(sprite_attribute, line_start.sprite_attribute, PPU_MAX_SPRITES_PER_LINE);
    memcpy(sprite_x, line_start.sprite_x, PPU_MAX_SPRITES_PER_LINE);
    memcpy(sprite_pattern_low, line_start.sprite_pattern_low, PPU_MAX_SPRITES_PER_LINE);
    memcpy(sprite_pattern_high, line_start.sprite_pattern_high, PPU_MAX_SPRITES_PER_LINE);
    if (__builtin_expect(ppu_fetch_hooks, 0) && mapper_chr_latch) {
        mapper_latch_restore(line_start.mapper_latches);
    }
}

// Expands a pattern byte into 8 one byte pixels (leftmost pixel first) as two 32 bit words
static uint32_t ppu_bit_spread[256][2];
// a pattern byte's two nibbles as tile_colors byte offsets (index << 2): bits 0-9 for the left 4 pixels, bits
// 16-25 for the right 4; the high pattern byte's table is the same shifted left by 4 (its nibble is index bits 4-7)
static uint32_t ppu_pattern_offsets[256];
// a nibble's 4 bits as bytes 0 / 1, bit 3 (the leftmost pixel) in the lowest byte
static const uint32_t ppu_nibble_spread[16] = {
    0x00000000, 0x01000000, 0x00010000, 0x01010000, 0x00000100, 0x01000100, 0x00010100, 0x01010100,
    0x00000001, 0x01000001, 0x00010001, 0x01010001, 0x00000101, 0x01000101, 0x00010101, 0x01010101
};

static void ppu_build_tables() {
    for (uint32_t value = 0; value < 256; value++) {
        uint32_t words[2] = { 0, 0 };
        for (uint32_t b = 0; b < 8; b++) {
            words[b >> 2] |= ((value >> (7 - b)) & 1) << ((b & 3) << 3);
        }
        ppu_bit_spread[value][0] = words[0];
        ppu_bit_spread[value][1] = words[1];
        ppu_pattern_offsets[value] = ((value >> 4) << 2) | ((value & 0x0f) << 18);
    }
}

// the dot sprite evaluation on the current scanline sets the overflow flag on, 0 for none (cached per scanline,
// OAM contents, OAMADDR and sprite size)
#ifdef PPU_STATS
uint32_t ppu_stat_overflow_predictions = 0, ppu_stat_overflow_evaluations = 0;
#endif
static uint32_t ppu_predict_overflow_dot() {
    PPU_STAT(ppu_stat_overflow_predictions++);
    static uint32_t key_scanline = 0xffffffff, key_generation, key_frame, key_setup, cached_dot;
    uint32_t height = (ppu_registers -> PPUCTRL & PPUCTRL_SPRITE_SIZE) ? 16 : 8;
    // evaluation from OAMADDR 0 can only set the overflow flag once it has found 8 sprites on the line (from the
    // sprite buckets, when they are up to date): with exactly 8 the hardware's buggy scan of the rest of OAM can
    // still set it (Mesen; Bandai's m16e)
    if ((ppu_registers -> OAMADDR == 0) && (oam_bucket_key == ((ppu_oam_generation << 1) | (height >> 4))) &&
        (oam_bucket_count[ppu_scanline] < PPU_MAX_SPRITES_PER_LINE)) {
        return 0;
    }
    uint32_t setup = ppu_registers -> OAMADDR | ((ppu_registers -> PPUCTRL & PPUCTRL_SPRITE_SIZE) << 8);
    if ((key_scanline != ppu_scanline) || (key_generation != ppu_oam_generation) || (key_frame != ppu_frame_count) || (key_setup != setup)) {
        key_scanline = ppu_scanline;
        key_generation = ppu_oam_generation;
        key_frame = ppu_frame_count;
        key_setup = setup;
        cached_dot = ppu_evaluate(ppu_scanline, 0) ? ppu_evaluate_overflow_dot : 0;
        PPU_STAT(ppu_stat_overflow_evaluations++);
    }
    return cached_dot;
}

/**
 * Renders the visible pixels of the current scanline from the dot 0 state, assuming nothing changes during the
 * scanline, and predicts when sprite 0 hit happens. Reads PPU state only.
 */
static void ppu_render_line_fast_body();
/**
 * Background tiles 2-32 of a scanline (the ones fetched during the scanline, from v as of dot 0). The 8 scanlines of
 * a tile row read the same names and attributes, so the pattern address (without fine Y) and palette color table
 * of each tile are worked out once per row, walking the nametable row and switching nametables where coarse X
 * wraps, and reused until v's row / coarse X, the background pattern table or the nametables change.
 */
extern uint32_t ppu_nametable_generation;
static uint32_t row_key = 0xffffffff;
static uint32_t row_generation;
static const uint8_t* row_pattern[31];
static const uint32_t* row_table[31];


static inline __attribute__((always_inline)) void ppu_build_row_impl(uint16_t v, uint32_t (*tile_colors)[256], const int hooks) {
    const uint8_t* patterns = (const uint8_t*)PATTERN_TABLES_ADDR +
        ((ppu_registers -> PPUCTRL & PPUCTRL_BACKGROUND_TABLE_ADDRESS) ? 0x1000 : 0);
    const uint8_t* nametable = (const uint8_t*)NAMETABLE0_ADDR + (v & 0x0c00);
    uint32_t row = v & 0x03e0;
    // attribute bytes of this row of 4x4 tile blocks, and the shift selecting the top / bottom 2x2 quadrants
    uint32_t attribute_row = 0x03c0 | ((v >> 4) & 0x38);
    uint32_t attribute_shift_y = (v >> 4) & 4;
    uint32_t x = v & 0x1f;
    for (uint32_t tile = 0; tile < 31; tile++) {
        uint32_t name = nametable[row | x];
        uint32_t attribute = (nametable[attribute_row | (x >> 2)] >> (attribute_shift_y | (x & 2))) & 3;
        if (hooks && mapper_ext_attributes) {
            // MMC5 extended attributes: the tile's 4 kB CHR bank and palette come from ExRAM
            row_pattern[tile] = mapper_ext_tile(row | x, name, &attribute);
        } else {
            row_pattern[tile] = patterns + (name << 4);
        }
        row_table[tile] = tile_colors[attribute];
        if (++x == 32) {
            // on to the other horizontal nametable
            x = 0;
            nametable = (const uint8_t*)((uintptr_t)nametable ^ 0x0400);
        }
    }
}

FRNES_HOT(7_row) static void ppu_build_row(uint16_t v, uint32_t (*tile_colors)[256]) {
    ppu_build_row_impl(v, tile_colors, 0);
}

static __attribute__((noinline)) void hooked_build_row(uint16_t v, uint32_t (*tile_colors)[256]) {
    ppu_build_row_impl(v, tile_colors, 1);
}

/*
 * ppu_expand_tiles for MMC2 / MMC4 cartridges: fetching tile $FD / $FE switches the CHR bank for the tiles after it.
 * Kept out of line so the common loop stays as it was.
 */
static __attribute__((noinline)) void hooked_expand_tiles(uint32_t fine_y, uint32_t* line, uint32_t* opaque,
    const int with_opaque) {
    for (uint32_t tile = 0; tile < 31; tile++) {
        const uint8_t* bytes = row_pattern[tile] + fine_y;
        uint32_t low = bytes[0];
        uint32_t high = bytes[8];
        uint32_t address = (uint32_t)((uintptr_t)bytes - PATTERN_TABLES_ADDR) + 8;
        if (((address & 0x0ff0) == 0x0fd0) || ((address & 0x0ff0) == 0x0fe0)) {
            mapper_latch_read(address);
        }
        uint32_t offsets = ppu_pattern_offsets[low] | (ppu_pattern_offsets[high] << 4);
        const uint8_t* colors = (const uint8_t*)row_table[tile];
        line[0] = *(const uint32_t*)(colors + (offsets & 0x3fc));
        line[1] = *(const uint32_t*)(colors + (offsets >> 16));
        line += 2;
        if (with_opaque) {
            opaque[0] = ppu_bit_spread[low | high][0];
            opaque[1] = ppu_bit_spread[low | high][1];
            opaque += 2;
        }
    }
}

static inline __attribute__((always_inline)) void ppu_expand_tiles(uint16_t v, uint32_t (*tile_colors)[256], uint32_t* line,
    uint32_t* opaque, const int with_opaque) {
    uint32_t key = (v & 0x0fff) | ((ppu_registers -> PPUCTRL & PPUCTRL_BACKGROUND_TABLE_ADDRESS) << 8);
    if ((key != row_key) || (ppu_nametable_generation != row_generation)) {
        row_key = key;
        row_generation = ppu_nametable_generation;
        if (__builtin_expect(ppu_fetch_hooks, 0) && mapper_ext_attributes) {
            hooked_build_row(v, tile_colors);
        } else {
            ppu_build_row(v, tile_colors);
        }
    }
    uint32_t fine_y = (v >> 12) & 7;
    if (__builtin_expect(ppu_fetch_hooks, 0) && mapper_chr_latch) {
        hooked_expand_tiles(fine_y, line, opaque, with_opaque);
        return;
    }
    const uint8_t* const* pattern = row_pattern;
    const uint32_t* const* table = row_table;
    for (uint32_t tile = 0; tile < 31; tile++) {
        const uint8_t* bytes = pattern[tile] + fine_y;
        uint32_t low = bytes[0];
        uint32_t high = bytes[8];

        uint32_t offsets = ppu_pattern_offsets[low] | (ppu_pattern_offsets[high] << 4);
        const uint8_t* colors = (const uint8_t*)table[tile];
        line[0] = *(const uint32_t*)(colors + (offsets & 0x3fc));
        line[1] = *(const uint32_t*)(colors + (offsets >> 16));
        line += 2;
        if (with_opaque) {
            opaque[0] = ppu_bit_spread[low | high][0];
            opaque[1] = ppu_bit_spread[low | high][1];
            opaque += 2;
        }
    }
}

#ifdef PPU_PERF
#include <dc/perfctr.h>
uint64_t ppu_stat_render_perf, ppu_stat_tail_perf, ppu_stat_sprite_perf;
#endif
static void ppu_render_line_fast() {
#ifdef PPU_STATS
    uint64_t start = timer_us_gettime64();
#ifdef PPU_PERF
    uint64_t perf_start = perf_cntr_count(PRFC1);
#endif
    ppu_render_line_fast_body();
#ifdef PPU_PERF
    ppu_stat_render_perf += perf_cntr_count(PRFC1) - perf_start;
#endif
    ppu_stat_render_us += timer_us_gettime64() - start;
#else
    ppu_render_line_fast_body();
#endif
}
// the fast pass's buffers: sprites (bits 0-1 pixel, 2-3 palette, bit 6 behind background, bit 7 sprite 0), the colors
// of the background (0-15) and sprite (16-31) values, and the background line as colors and opaque flags
__ALIGN32__ static uint8_t sprite_line[256];
static uint8_t colors[32];
__ALIGN32__ static uint32_t color_line[68];
__ALIGN32__ static uint32_t opaque_line[68];

/*
 * The fast pass's composition for a scanline whose fine X or sprite enable changed partway (lazy_x_switches): pixel x
 * takes the background byte at x + its fine X (as the per-dot path's shift register bit), then the sprites go over it
 * where they're on. Not in the hot group: only raster effects come here (Castlevania III's intro and Lagrange Point's
 * battles write $2005 mid line on ~50 lines a frame, Ninja Gaiden II's transitions $2001). The background goes a run
 * at a time, from one switch to the next, and the sprites are composed only in the 8 pixel columns that have any, as
 * the fast pass does
 */
/*
 * v as the per dot path has it at dot 'dot' of a lazy line with a v switch: from dot 0's, one coarse X step at each of
 * dots 8, 16, ... before 'dot'; from the switch's dot on, the switched v with the steps from there
 */
static FRNES_COLD_TEXT uint16_t ppu_v_switch_at(uint32_t dot) {
    uint16_t v;
    uint32_t from;
    if (dot < lazy_v_dot) {
        v = line_start.v;
        from = 1;
    } else {
        v = lazy_v_value;
        from = lazy_v_dot;
    }
    uint16_t saved = ppu_registers -> PPU_V;
    ppu_registers -> PPU_V = v;
    for (uint32_t step = (from + 7) & ~7u; step < dot; step += 8) {
        if (step) {
            ppu_increment_x();
        }
    }
    v = ppu_registers -> PPU_V;
    ppu_registers -> PPU_V = saved;
    return v;
}

/*
 * A lazy line with a v switch (a second $2006 write mid line, ppu_addr_write_lazy): the tiles fetched from the switch
 * on, into the fast pass's background line, each fetch (name at dot 8k + 1, attribute 8k + 3, pattern low 8k + 5 and
 * high 8k + 7, tile k + 2 of the line) from v as the per dot path has it at its dot; and v at the line's dot 256 for
 * its end (ppu_finish_lazy_line's and ppu_sync's flip of the nametable bit, as 32 coarse X steps, is undone)
 */
static FRNES_COLD_TEXT void ppu_v_switch_tiles(uint32_t show_bg) {
    uint32_t i = 0;
    while ((i < lazy_x_switches) && !(lazy_x_value[i] & LAZY_V_SWITCH)) {
        i++;
    }
    if (i == lazy_x_switches) {
        return;
    }
    if (show_bg) {
        uint8_t* line = (uint8_t*)color_line;
        uint8_t* opaque = (uint8_t*)opaque_line;
        uint16_t table = (ppu_registers -> PPUCTRL & PPUCTRL_BACKGROUND_TABLE_ADDRESS) ? 0x1000 : 0;
        for (uint32_t k = 0; k < 31; k++) {
            if (8 * k + 7 < lazy_v_dot) {
                continue;
            }
            uint16_t v_name = ppu_v_switch_at(8 * k + 1), v_attribute = ppu_v_switch_at(8 * k + 3);
            uint16_t v_low = ppu_v_switch_at(8 * k + 5), v_high = ppu_v_switch_at(8 * k + 7);
            uint8_t name = ppu_read_nametable(0x2000 | (v_name & 0x0fff));
            uint8_t attribute = ppu_read_nametable(0x23c0 | (v_attribute & 0x0c00) | ((v_attribute >> 4) & 0x38) | ((v_attribute >> 2) & 0x07));
            attribute = (attribute >> (((v_attribute >> 4) & 4) | (v_attribute & 2))) & 3;
            uint8_t low = ppu_read_pattern(table | (name << 4) | ((v_low >> 12) & 7));
            uint8_t high = ppu_read_pattern_high(table | (name << 4) | ((v_high >> 12) & 7) | 8, 0);
            for (uint32_t b = 0; b < 8; b++) {
                uint32_t value = ((low >> (7 - b)) & 1) | (((high >> (7 - b)) & 1) << 1);
                line[(k + 2) * 8 + b] = colors[(attribute << 2) | value];
                opaque[(k + 2) * 8 + b] = value != 0;
            }
        }
    }
    ppu_registers -> PPU_V = ppu_v_switch_at(257) ^ 0x0400;
}

static __attribute__((noinline)) void ppu_render_line_switches() {
    uint8_t mask = ppu_registers -> PPUMASK;
    uint32_t* out = (uint32_t*)(ppu_output + (ppu_scanline << 8));
    uint32_t show_bg = (mask & PPUMASK_ENABLE_BACKGROUND_RENDERING);
    // the sprite enable at pixel 0 (sprites_at), and whether it's on anywhere on the line / changes on it
    uint32_t sprites_at = (mask & PPUMASK_ENABLE_SPRITE_RENDERING) != 0, sprites_somewhere = sprites_at, toggles = 0;
    for (uint32_t i = 0; i < lazy_x_switches; i++) {
        if (lazy_x_value[i] & LAZY_SPRITES_TOGGLE) {
            sprites_at ^= 1;
            sprites_somewhere = 1;
            toggles = 1;
        }
    }
    uint32_t show_sprites = sprite_count && sprites_somewhere;
    // (a v switch's tiles: ppu_v_switch_tiles, out of line and cold, looks for one; only once a switch has happened)
    if (lazy_v_dot) {
        ppu_v_switch_tiles(show_bg);
    }
    __ALIGN32__ uint8_t line[256];
    __ALIGN32__ uint8_t opaque[256];
    // (sprites on at each pixel, when they change on the line)
    __ALIGN32__ uint8_t sprites_on[256];
    const uint8_t* bg = (const uint8_t*)color_line;
    const uint8_t* bg_opaque = (const uint8_t*)opaque_line;
    uint32_t with_opaque = show_bg && show_sprites;
    uint32_t fine_x = line_start.fine_x, x = 0;
    for (uint32_t next = 0; next <= lazy_x_switches; next++) {
        uint32_t end = (next < lazy_x_switches) ? lazy_x_pixel[next] : 256;
        if (end > x) {
            const uint8_t* from = bg + x + fine_x;
            for (uint32_t i = x; i < end; i++) {
                line[i] = *from++;
            }
            if (with_opaque) {
                from = bg_opaque + x + fine_x;
                for (uint32_t i = x; i < end; i++) {
                    opaque[i] = *from++;
                }
            }
            if (toggles) {
                memset(sprites_on + x, sprites_at, end - x);
            }
            x = end;
        }
        if (next < lazy_x_switches) {
            fine_x = lazy_x_value[next] & 7;
            sprites_at ^= (lazy_x_value[next] & LAZY_SPRITES_TOGGLE) != 0;
        }
    }
    if (show_sprites && !with_opaque) {
        memset(opaque, 0, sizeof(opaque));
    }
    if (show_bg && !(mask & PPUMASK_SHOW_BACKGROUND_IN_LEFTMOST_8_PIXELS)) {
        for (uint32_t x = 0; x < 8; x++) {
            line[x] = colors[0];
            opaque[x] = 0;
        }
    }
    if (show_sprites) {
        uint32_t columns = 0;
        for (int32_t i = sprite_count - 1; i >= 0; i--) {
            uint8_t flags = ((sprite_attribute[i] & 3) << 2) | ((sprite_attribute[i] & 0x20) ? 0x40 : 0) | (((i == 0) && sprite_zero_on_line) ? 0x80 : 0);
            uint32_t x = sprite_x[i];
            uint8_t low = sprite_pattern_low[i];
            uint8_t high = sprite_pattern_high[i];
            if (!(low | high)) {
                continue;
            }
            columns |= (1u << (x >> 3));
            if (x < 248) {
                columns |= (1u << ((x + 7) >> 3));
            }
            for (uint32_t b = 0; (b < 8) && (x < 256); b++, x++) {
                uint8_t pixel = ((low >> (7 - b)) & 1) | (((high >> (7 - b)) & 1) << 1);
                if (pixel) {
                    sprite_line[x] = pixel | flags;
                }
            }
        }
        if (!(mask & PPUMASK_SHOW_SPRITES_IN_LEFTMOST_8_PIXELS)) {
            ((uint32_t*)sprite_line)[0] = ((uint32_t*)sprite_line)[1] = 0;
        }
        while (columns) {
            uint32_t column = __builtin_ctz(columns);
            columns &= columns - 1;
            for (uint32_t x = column << 3; x < (column << 3) + 8; x++) {
                uint8_t sprite = sprite_line[x];
                if (!sprite) {
                    continue;
                }
                sprite_line[x] = 0;
                if (toggles ? !sprites_on[x] : !sprites_at) {
                    continue;
                }
                if ((sprite & 0x80) && opaque[x] && (x != 255) && !lazy_hit_dot) {
                    lazy_hit_dot = x + 1;
                }
                if (!opaque[x] || !(sprite & 0x40)) {
                    line[x] = colors[0x10 | (sprite & 0x0f)];
                }
            }
        }
    }
    const uint32_t* in = (const uint32_t*)line;
    for (uint32_t block = 0; block < 8; block++) {
        for (uint32_t word = 0; word < 8; word++) {
            out[word] = in[word];
        }
        PPU_OUTPUT_BLOCK_DONE(out);
        out += 8;
        in += 8;
    }
}

FRNES_HOT(4_render) static void ppu_render_line_fast_body() {
    // sprites: bits 0-1 pixel, 2-3 palette, bit 6 behind background, bit 7 sprite 0
    uint8_t mask = ppu_registers -> PPUMASK;
    uint32_t* out = (uint32_t*)(ppu_output + (ppu_scanline << 8));
    uint8_t* palette = (uint8_t*)PALETTE_RAM_ADDR;

    lazy_line_rendered = 1;
    lazy_hit_dot = 0;
    PPU_STAT(ppu_stat_fast_renders++);

    // colors for background (0-15) and sprite (16-31) values, transparent pixels show the backdrop color, plus
    // for each background palette the 4 colors of every combination of 4 pixels; rebuilt only when palette RAM
    // or the greyscale bit changes
    __ALIGN32__ static uint32_t tile_colors[4][256];
    if (ppu_palette_dirty) {
        uint32_t dirty = ppu_palette_dirty;
        ppu_palette_dirty = 0;
        uint8_t grey = (mask & PPUMASK_GREYSCALE) ? 0x30 : 0x3f;
        for (uint32_t i = 0; i < 32; i++) {
            colors[i] = palette[(i & 3) ? i : 0] & grey;
        }
        // index: low nibble of the low pattern byte | high pattern byte nibble << 4, bit 3 is the leftmost pixel.
        // Built 16 entries at a time: the low nibble's pixel values (bit 0 of each byte) plus the high nibble's
        // (bit 1), mapped through the palette's 4 colors one byte at a time
        for (uint32_t p = 0; p < 4; p++) {
            if (!(dirty & (1 << p))) {
                continue;
            }
            const uint8_t* c = &colors[p << 2];
            uint32_t* table = tile_colors[p];
            for (uint32_t high = 0; high < 16; high++) {
                uint32_t high_bits = ppu_nibble_spread[high] << 1;
                for (uint32_t low = 0; low < 16; low++) {
                    uint32_t pixels = ppu_nibble_spread[low] | high_bits;
                    table[(high << 4) | low] = c[pixels & 3] | (c[(pixels >> 8) & 3] << 8) | (c[(pixels >> 16) & 3] << 16) | (c[pixels >> 24] << 24);
                }
            }
        }
    }

    uint32_t show_bg = (mask & PPUMASK_ENABLE_BACKGROUND_RENDERING);
    // (with switches on the line, the sprites may be on for part of it: ppu_render_line_switches works it out)
    uint32_t show_sprites = sprite_count && ((mask & PPUMASK_ENABLE_SPRITE_RENDERING) || lazy_x_switches);

    if (!show_bg && !show_sprites) {
        ppu_output_fill_line(colors[0]);
        return;
    }

    // background expanded straight to colors, 4 pixels per lookup, for the 33 tiles touched by the scanline (plus
    // which pixels are opaque when there are sprites to compose); the line starts at byte PPU_X
    uint32_t fine_x = ppu_registers -> PPU_X;
    if (show_bg) {
        uint32_t* line = color_line;
        uint32_t* opaque = opaque_line;
        // tiles 0 and 1 were prefetched into the shift registers at the end of the previous scanline
        for (uint32_t tile = 0; tile < 2; tile++) {
            uint32_t shift = tile ? 0 : 8;
            uint8_t low = line_start.bg_shift_pattern_low >> shift;
            uint8_t high = line_start.bg_shift_pattern_high >> shift;
            uint32_t* table = tile_colors[((line_start.bg_shift_attribute_low >> shift) & 1) | (((line_start.bg_shift_attribute_high >> shift) & 1) << 1)];
            line[0] = table[(low >> 4) | (high & 0xf0)];
            line[1] = table[(low & 0x0f) | ((high & 0x0f) << 4)];
            line += 2;
            if (show_sprites) {
                opaque[0] = ppu_bit_spread[low | high][0];
                opaque[1] = ppu_bit_spread[low | high][1];
                opaque += 2;
            }
        }
        // tiles 2-32 are fetched during the scanline, starting at v as of dot 0
        if (show_sprites) {
            ppu_expand_tiles(line_start.v, tile_colors, line, opaque, 1);
        } else {
            ppu_expand_tiles(line_start.v, tile_colors, line, opaque, 0);
        }
        // the left 8 pixels can be masked off, they show the backdrop color and count as transparent
        if (!(mask & PPUMASK_SHOW_BACKGROUND_IN_LEFTMOST_8_PIXELS) && !lazy_x_switches) {
            // (straight line byte stores: as a loop or memset, unaligned 8 byte fills are calls to the library memset,
            // whose code then competes for the I-cache on every scanline)
            ppu_fill8((uint8_t*)color_line + fine_x, colors[0]);
            ppu_fill8((uint8_t*)opaque_line + fine_x, 0);
        }
    } else {
        // sprites over the backdrop color
        uint32_t backdrop = colors[0] * 0x01010101;
        for (uint32_t i = 0; i < 68; i++) {
            color_line[i] = backdrop;
            opaque_line[i] = 0;
        }
    }

    if (__builtin_expect(lazy_x_switches, 0)) {
        ppu_render_line_switches();
        return;
    }
#ifdef PPU_PERF
    uint64_t sprite_perf_start = perf_cntr_count(PRFC1);
#endif
    if (show_sprites) {
        // sprite pixels, lowest index on top, with a bit per 8 pixel column that has any (sprite_line is all
        // zeroes between lines: the columns used are cleared again after composing)
        uint32_t columns = 0;
        for (int32_t i = sprite_count - 1; i >= 0; i--) {
            uint8_t flags = ((sprite_attribute[i] & 3) << 2) | ((sprite_attribute[i] & 0x20) ? 0x40 : 0) | (((i == 0) && sprite_zero_on_line) ? 0x80 : 0);
            uint32_t x = sprite_x[i];
            uint8_t low = sprite_pattern_low[i];
            uint8_t high = sprite_pattern_high[i];
            if (!(low | high)) {
                continue;
            }
            columns |= (1u << (x >> 3));
            if (x < 248) {
                columns |= (1u << ((x + 7) >> 3));
            }
            for (uint32_t b = 0; (b < 8) && (x < 256); b++, x++) {
                uint8_t pixel = ((low >> (7 - b)) & 1) | (((high >> (7 - b)) & 1) << 1);
                if (pixel) {
                    sprite_line[x] = pixel | flags;
                }
            }
        }
        if (!(mask & PPUMASK_SHOW_SPRITES_IN_LEFTMOST_8_PIXELS)) {
            ((uint32_t*)sprite_line)[0] = ((uint32_t*)sprite_line)[1] = 0;
        }
        // compose the columns with sprite pixels over the background colors
        uint8_t* background = (uint8_t*)color_line + fine_x;
        uint8_t* opaque = (uint8_t*)opaque_line + fine_x;
        while (columns) {
            uint32_t column = __builtin_ctz(columns);
            columns &= columns - 1;
            uint32_t* clear = (uint32_t*)&sprite_line[column << 3];
            for (uint32_t x = column << 3; x < (column << 3) + 8; x++) {
                uint8_t sprite = sprite_line[x];
                if (sprite) {
                    uint8_t bg_opaque = opaque[x];
                    if ((sprite & 0x80) && bg_opaque && (x != 255) && !lazy_hit_dot) {
                        lazy_hit_dot = x + 1;
                    }
                    if (!bg_opaque || !(sprite & 0x40)) {
                        background[x] = colors[0x10 | (sprite & 0x0f)];
                    }
                }
            }
            clear[0] = clear[1] = 0;
        }
    }

#ifdef PPU_PERF
    ppu_stat_sprite_perf += perf_cntr_count(PRFC1) - sprite_perf_start;
#endif
    // output, 4 pixels per 32 bit word shifted by fine X, 8 words per block
    uint32_t* in = &color_line[fine_x >> 2];
    uint32_t shift = (fine_x & 3) << 3;
    for (uint32_t block = 0; block < 8; block++) {
        if (shift) {
            for (uint32_t word = 0; word < 8; word++) {
                out[word] = (in[word] >> shift) | (in[word + 1] << (32 - shift));
            }
        } else {
            for (uint32_t word = 0; word < 8; word++) {
                out[word] = in[word];
            }
        }
        PPU_OUTPUT_BLOCK_DONE(out);
        out += 8;
        in += 8;
    }
}

/**
 * Advances a lazy scanline to dot 'target', applying the dot events that can't wait for the end of the line
 */
// CPU cycles until the mapper's 'clocks'-th clock from now (1: the next one), assuming rendering stays on: the
// mapper dot of the rendering scanlines (0-239, 261) ahead. (The odd frames' short pre-render line is left out: the
// count rounds up, so the batch ends at most a dot after the clock)
// (the clocks counted, not walked through line by line: with the pre-render line's extra clock, and an armed counter
// more than a few lines away (up to 256): Kirby's Adventure, its DMC drums planning batches up to each fetch. A
// frame's clocks are lines 0-239's then the pre-render line's one or two: the first one after the PPU's position
// found in its line, the target the clocks after it in that order. Last in the mmc3 slot, after the functions the
// other games run, which keep their addresses)
FRNES_HOT(5_mmc3_zz_cycles_to_far_clock) uint32_t ppu_cycles_to_far_mapper_clock(uint32_t clocks) {
    uint32_t dot = ppu_mapper_dot();
    uint32_t position = ppu_scanline * PPU_DOTS_PER_SCANLINE + ppu_dot + ppu_pending_dots;
    uint32_t prerender = PPU_PRERENDER_SCANLINE * PPU_DOTS_PER_SCANLINE;
    uint32_t per_frame, rank;
    if (ppu_mapper_prerender_clock()) {
        per_frame = PPU_VISIBLE_SCANLINES + 2;
        uint32_t local = position;
        rank = 0;
        if (local >= PPU_SCANLINES_PER_FRAME * PPU_DOTS_PER_SCANLINE) {
            local -= PPU_SCANLINES_PER_FRAME * PPU_DOTS_PER_SCANLINE;
            rank = per_frame;
        }
        uint32_t line = local / PPU_DOTS_PER_SCANLINE;
        if (line < PPU_VISIBLE_SCANLINES) {
            rank += (line * PPU_DOTS_PER_SCANLINE + dot + 1 > local) ? line : line + 1;
        } else if ((line < PPU_PRERENDER_SCANLINE) || (prerender + PPU_MAPPER_PRERENDER_DOT + 1 > local)) {
            rank += PPU_VISIBLE_SCANLINES;
        } else {
            rank += (prerender + dot + 1 > local) ? PPU_VISIBLE_SCANLINES + 1 : PPU_VISIBLE_SCANLINES + 2;
        }
    } else {
        // (the first clock as ppu_cycles_to_mapper_clock finds it)
        per_frame = PPU_VISIBLE_SCANLINES + 1;
        uint32_t line = (ppu_dot + ppu_pending_dots <= dot) ? ppu_scanline : ppu_scanline + 1;
        rank = (line < PPU_VISIBLE_SCANLINES) ? line : ((line <= PPU_PRERENDER_SCANLINE) ? PPU_VISIBLE_SCANLINES : per_frame);
    }
    rank += clocks - 1;
    // (at most 2 frames ahead: no division, a library call on the SH4)
    uint32_t frames = 0;
    while (rank >= per_frame) {
        rank -= per_frame;
        frames++;
    }
    uint32_t target = frames * (PPU_SCANLINES_PER_FRAME * PPU_DOTS_PER_SCANLINE) +
        ((rank < PPU_VISIBLE_SCANLINES) ? rank * PPU_DOTS_PER_SCANLINE + dot :
        prerender + (((per_frame == PPU_VISIBLE_SCANLINES + 2) && (rank == PPU_VISIBLE_SCANLINES)) ? PPU_MAPPER_PRERENDER_DOT : dot)) + 1;
    if (target <= position) {
        target += PPU_SCANLINES_PER_FRAME * PPU_DOTS_PER_SCANLINE;
    }
    return (target - position + 2) / 3;
}

FRNES_HOT(5_mmc3_cycles_to_clock) uint32_t ppu_cycles_to_mapper_clock(uint32_t clocks) {
    if (ppu_mapper_prerender_clock() || (clocks > 8)) {
        return ppu_cycles_to_far_mapper_clock(clocks);
    }
    uint32_t dot = ppu_mapper_dot();
    uint32_t scanline = ppu_scanline;
    uint32_t position = scanline * PPU_DOTS_PER_SCANLINE + ppu_dot + ppu_pending_dots;
    uint32_t target_scanline = (ppu_dot + ppu_pending_dots <= dot) ? scanline : scanline + 1;
    uint32_t offset = 0;
    for (;;) {
        if ((target_scanline >= PPU_VISIBLE_SCANLINES) && (target_scanline < PPU_PRERENDER_SCANLINE)) {
            target_scanline = PPU_PRERENDER_SCANLINE;
        }
        if (target_scanline >= PPU_SCANLINES_PER_FRAME) {
            target_scanline = 0;
            offset += PPU_SCANLINES_PER_FRAME * PPU_DOTS_PER_SCANLINE;
        }
        if (--clocks == 0) {
            break;
        }
        target_scanline++;
    }
    uint32_t target = offset + target_scanline * PPU_DOTS_PER_SCANLINE + dot + 1;
    if (target <= position) {
        target += PPU_SCANLINES_PER_FRAME * PPU_DOTS_PER_SCANLINE;
    }
    return (target - position + 2) / 3;
}

/**
 * Dots 258-320 only fetch the sprite patterns (one object every 8 dots, at 264-320), copy vertical scroll on the
 * pre-render line (280-304) and clock the mapper at its dot: the dots in between do nothing in ppu_rendering_dot.
 * Returns the next dot from 'dot' on that does something (dot itself if it does).
 */
static inline uint32_t ppu_next_tail_event(uint32_t dot) {
    if ((dot < 258) || (dot > 320)) {
        return dot;
    }
    uint32_t next = (dot <= 264) ? 264 : ((dot + 7) & ~7);
    if (next > 321) {
        next = 321;
    }
    if (ppu_scanline == PPU_PRERENDER_SCANLINE) {
        if (dot <= 304) {
            uint32_t copy = (dot < 280) ? 280 : dot;
            if (copy < next) {
                next = copy;
            }
        }
    }
    if (mapper_clock_dot) {
        uint32_t clock = ppu_mapper_dot();
        if ((clock >= dot) && (clock < next)) {
            next = clock;
        }
    }
    return next;
}

static inline void ppu_lazy_advance(uint32_t target) {
    if (mapper_clock_dot) {
        uint32_t dot = ppu_mapper_dot();
        if ((ppu_dot <= dot) && (target > dot)) {
            ppu_clock_mapper();
        }
    }
    // status flags are cleared at dot 1 of the pre-render scanline
    if ((ppu_scanline == PPU_PRERENDER_SCANLINE) && (ppu_dot <= 1) && (target > 1)) {
        ppu_registers -> PPUSTATUS &= ~(PPUSTATUS_SPRITE_0_HIT_FLAG | PPUSTATUS_SPRITE_OVERFLOW_FLAG | PPUSTATUS_VBLANK_FLAG);
    }
    ppu_dot = target;
}

/**
 * Finishes a lazy scanline: produces its pixels (if not done yet), applies the effect of dots 1-256 on v,
 * and runs dots 257 onwards through the per-dot code
 */
// the scanline tail's prefetch of a background tile's pattern for mappers with fetch hooks
static __attribute__((noinline)) void hooked_prefetch_tile(uint16_t v, uint32_t name, uint16_t pattern_base) {
    if (mapper_ext_attributes) {
        uint32_t palette;
        const uint8_t* bytes = mapper_ext_tile(v & 0x03ff, name, &palette);
        bg_next_attribute = palette;
        bg_next_low = bytes[(v >> 12) & 7];
        bg_next_high = bytes[((v >> 12) & 7) | 8];
    } else {
        bg_next_low = ppu_read_pattern(pattern_base | (name << 4) | ((v >> 12) & 7));
        bg_next_high = ppu_read_pattern_high(pattern_base | (name << 4) | ((v >> 12) & 7) | 8, 1);
    }
}

// the pre-render line fetches background tiles over dots 1-256 like any rendering line (no pixels): with MMC2 / MMC4
// those fetches can flip the CHR latches before scanline 0 (Punch-Out!!'s first line)
static __attribute__((noinline)) void hooked_prerender_fetches() {
    uint16_t v = ppu_registers -> PPU_V;
    uint16_t pattern_base = ((ppu_registers -> PPUCTRL & PPUCTRL_BACKGROUND_TABLE_ADDRESS) ? 0x1000 : 0);
    for (uint32_t tile = 0; tile < 32; tile++) {
        uint8_t name = ppu_read_nametable(0x2000 | (v & 0x0fff));
        ppu_read_pattern_high(pattern_base | (name << 4) | ((v >> 12) & 7) | 8, 1);
        v = ((v & 0x001f) == 0x001f) ? ((v & ~0x001f) ^ 0x0400) : (v + 1);
    }
}

static void ppu_finish_lazy_line(uint32_t line_length) {
    if (ppu_scanline < PPU_VISIBLE_SCANLINES) {
        if (!lazy_line_rendered) {
            ppu_render_line_fast();
        }
        if (lazy_hit_dot) {
            ppu_registers -> PPUSTATUS |= PPUSTATUS_SPRITE_0_HIT_FLAG;
        }
    }
    if (__builtin_expect(ppu_fetch_hooks, 0) && mapper_chr_latch && (ppu_scanline == PPU_PRERENDER_SCANLINE)) {
        hooked_prerender_fetches();
    }
    // dots 8, 16, ... 256 increment coarse x 32 times, which comes back to the same coarse x in the other
    // horizontal nametable, then dot 256 increments y
    ppu_registers -> PPU_V ^= 0x0400;
    ppu_increment_y();
#ifdef PPU_STATS
    uint64_t tail_start = timer_us_gettime64();
#ifdef PPU_PERF
    uint64_t tail_perf_start = perf_cntr_count(PRFC1);
#endif
#endif

    // the same as running dots 257-340 through ppu_rendering_dot, without the per-dot overhead:
    // dot 257 copies horizontal scroll and evaluates sprites for the next scanline
    ppu_copy_x();
    if (ppu_scanline < PPU_VISIBLE_SCANLINES) {
        ppu_evaluate_sprites(ppu_scanline);
        ppu_registers -> OAMADDR = 0;
    } else {
        // (no evaluation on the pre-render line, secondary OAM keeps what the last one left: see ppu_fetch_sprites)
        ppu_registers -> OAMADDR = 0;
        // dots 280-304 of the pre-render line copy vertical scroll
        ppu_copy_y();
    }
    // dots 321-336 prefetch the first two tiles of the next scanline into the shift registers, dot 337 starts
    // fetching the third
    uint16_t pattern_base = ((ppu_registers -> PPUCTRL & PPUCTRL_BACKGROUND_TABLE_ADDRESS) ? 0x1000 : 0);
    for (uint32_t tile = 0; tile < 2; tile++) {
        uint16_t v = ppu_registers -> PPU_V;
        uint8_t name = ppu_read_nametable(0x2000 | (v & 0x0fff));
        uint8_t attribute = ppu_read_nametable(0x23c0 | (v & 0x0c00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07));
        bg_next_attribute = (attribute >> (((v >> 4) & 4) | (v & 2))) & 3;
        if (__builtin_expect(ppu_fetch_hooks, 0)) {
            hooked_prefetch_tile(v, name, pattern_base);
        } else {
            bg_next_low = ppu_read_pattern(pattern_base | (name << 4) | ((v >> 12) & 7));
            bg_next_high = ppu_read_pattern_high(pattern_base | (name << 4) | ((v >> 12) & 7) | 8, 0);
        }
        bg_shift_pattern_low = (bg_shift_pattern_low << 8) | bg_next_low;
        bg_shift_pattern_high = (bg_shift_pattern_high << 8) | bg_next_high;
        bg_shift_attribute_low = (bg_shift_attribute_low << 8) | ((bg_next_attribute & 1) ? 0xff : 0x00);
        bg_shift_attribute_high = (bg_shift_attribute_high << 8) | ((bg_next_attribute & 2) ? 0xff : 0x00);
        ppu_increment_x();
    }
    bg_next_tile = ppu_read_nametable(0x2000 | (ppu_registers -> PPU_V & 0x0fff));
    bg_next_table = (ppu_registers -> PPUCTRL & PPUCTRL_BACKGROUND_TABLE_ADDRESS) ? 0x1000 : 0;
    // dot 340 fetches the sprite patterns (the short pre-render line skips it, but fetches them all the same)
    ppu_fetch_sprites((ppu_scanline == PPU_PRERENDER_SCANLINE) ? 0 : ppu_scanline + 1);
    (void)line_length;
#ifdef PPU_STATS
    ppu_stat_tail_us += timer_us_gettime64() - tail_start;
#ifdef PPU_PERF
    ppu_stat_tail_perf += perf_cntr_count(PRFC1) - tail_perf_start;
#endif
#endif
}

/**
 * ppu_sync's replay of a lazy scanline that took fine X changes ($2005, see ppu_scroll_write_lazy), applied at the dots
 * they were written on (pixel x is output on dot x + 1). Out of the hot code: only mid-line raster effects come here.
 */
static __attribute__((noinline)) void ppu_replay_switches(uint32_t target) {
    uint8_t status = ppu_registers -> PPUSTATUS & PPUSTATUS_VBLANK_FLAG;
    uint8_t fine_x = ppu_registers -> PPU_X, mask = ppu_registers -> PPUMASK;
    uint32_t next = 0;
    ppu_restore_line_start();
    ppu_registers -> PPU_X = line_start.fine_x;
    // (the sprite enable as it was at dot 0: as now, flipped back by each switch's toggle)
    for (uint32_t i = 0; i < lazy_x_switches; i++) {
        if (lazy_x_value[i] & LAZY_SPRITES_TOGGLE) {
            ppu_registers -> PPUMASK ^= PPUMASK_ENABLE_SPRITE_RENDERING;
        }
    }
    for (ppu_dot = 0; ppu_dot < target; ppu_dot++) {
        if ((ppu_scanline == PPU_PRERENDER_SCANLINE) && (ppu_dot == 1)) {
            ppu_registers -> PPUSTATUS &= ~(PPUSTATUS_SPRITE_0_HIT_FLAG | PPUSTATUS_SPRITE_OVERFLOW_FLAG | PPUSTATUS_VBLANK_FLAG);
            status = 0;
        }
        while ((next < lazy_x_switches) && ((uint32_t)lazy_x_pixel[next] + 1 == ppu_dot)) {
            ppu_registers -> PPU_X = lazy_x_value[next] & 7;
            // (a v switch, LAZY_V_SWITCH: v as the second $2006 write set it, at its dot)
            if (lazy_x_value[next] & LAZY_V_SWITCH) {
                ppu_registers -> PPU_V = lazy_v_value;
            }
            if (lazy_x_value[next++] & LAZY_SPRITES_TOGGLE) {
                ppu_registers -> PPUMASK ^= PPUMASK_ENABLE_SPRITE_RENDERING;
            }
        }
        ppu_rendering_dot();
    }
    ppu_registers -> PPU_X = fine_x;
    ppu_registers -> PPUMASK = mask;
    ppu_registers -> PPUSTATUS = (ppu_registers -> PPUSTATUS & ~PPUSTATUS_VBLANK_FLAG) | status;
    ppu_line_mode = PPU_LINE_PERDOT;
}

/*
 * ppu_sync's replay of a lazy visible scanline from a tile boundary instead of dot 0 (Rad Racer and Rad Racer II write
 * a $2006 pair at dots ~180-230 on ~25 road lines a frame, each a replay of the line dot by dot before). The fast pass
 * draws the line into the per-dot line buffer, the background's fetch state (v, the shifters, the next tile's
 * latches) is worked out as the per-dot path leaves it after dot 8m, the last tile boundary before the target, by
 * running only its background steps, and dots 8m + 1 up to the target then run dot by dot as a replay would: the
 * pixels before 8m are the fast pass's, which are the per-dot path's (it is the accuracy reference the fast pass is
 * checked against), sprite 0 hit before them comes from its prediction, and sprite evaluation and the mapper's
 * clock only happen after dot 256. Returns 0, having done nothing, where a replay from dot 0 is needed: the pre-render
 * line, fetch hooks (MMC2 / MMC4 latches, MMC5), a mapper clocked before dot 257, rendering turned on mid line
 */
static FRNES_COLD_TEXT int ppu_replay_from_tile(uint32_t target) {
    uint32_t m = (target - 1) >> 3;
    // (and only after a line whose sprite fetches and dot 339 ran with rendering on: the sprites the fast pass draws
    // are then the ones the per-dot path holds. SMB3 turns rendering off for its status bar's first line and back on
    // at its dot 339, and the next line's sprites differ)
    uint32_t previous_line = ppu_scanline ? ppu_scanline - 1 : PPU_PRERENDER_SCANLINE;
    if ((m < 2) || (ppu_scanline >= PPU_VISIBLE_SCANLINES) || ppu_fetch_hooks || ppu_scroll_resume_dot ||
        (mapper_clock_dot && (ppu_mapper_dot() < 257)) || (ppu_sprite_x_origin != 0xffffffff) ||
        (ppu_sprite_x_line != previous_line) || (ppu_sprite_fetch_line != previous_line) || (ppu_sprite_fetched != 8)) {
        return 0;
    }
    PPU_STAT(ppu_stat_replays_tile++);
    PPU_STAT(ppu_stat_replays_tile_dots += target - 8 * m);
    // the whole line by the fast pass, into the line buffer (pixels 0 to 8m - 1 are kept)
    uint8_t* output = ppu_output;
    uint32_t output_sq = ppu_output_sq;
    ppu_output = (uint8_t*)((uintptr_t)ppu_line_buffer - ((uintptr_t)ppu_scanline << 8));
    ppu_output_sq = 0;
    ppu_render_line_fast();
    ppu_output = output;
    ppu_output_sq = output_sq;
    ppu_line_buffer_used = 1;
    if (lazy_hit_dot && (lazy_hit_dot <= 8 * m)) {
        ppu_registers -> PPUSTATUS |= PPUSTATUS_SPRITE_0_HIT_FLAG;
    }
    // the background steps of dots 1 to 8m (ppu_rendering_dot_impl's, without hooks)
    ppu_restore_line_start();
    uint32_t last = 8 * m;
    uint32_t first = 2;
    if (m >= 4) {
        // (only the last three tile groups: what the steps leave after dot 8m is v, moved on one tile per group, the
        // next tile's latches, fetched in the last group, and the 16 bit shifters, which by then hold only the two
        // tiles loaded at dots 8m - 15 and 8m - 7, fetched in the two groups before; the bits of earlier loads have
        // been shifted out. So v is moved on m - 3 tiles and the steps run from dot 8(m - 3) + 1, whose load is shifted
        // out in turn. The fetches have no side effects here: no fetch hooks, no mapper clocked before dot 257.
        // Rockman 6's title: its $2006 writes at dots ~230 made this loop ~224 dots a line on ~94 lines a frame)
        for (uint32_t i = 0; i < m - 3; i++) {
            ppu_increment_x();
        }
        first = 8 * (m - 3) + 1;
    }
    for (uint32_t dot = first; dot <= last; dot++) {
        bg_shift_pattern_low <<= 1;
        bg_shift_pattern_high <<= 1;
        bg_shift_attribute_low <<= 1;
        bg_shift_attribute_high <<= 1;
        uint16_t v = ppu_registers -> PPU_V;
        switch ((dot - 1) & 7) {
            case 0:
                ppu_load_background_shifters();
                bg_next_tile = ppu_read_nametable(0x2000 | (v & 0x0fff));
                bg_next_table = (ppu_registers -> PPUCTRL & PPUCTRL_BACKGROUND_TABLE_ADDRESS) ? 0x1000 : 0;
                break;
            case 2:
                bg_next_attribute = ppu_read_nametable(0x23c0 | (v & 0x0c00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07));
                if (v & 0x0040) {
                    bg_next_attribute >>= 4;
                }
                if (v & 0x0002) {
                    bg_next_attribute >>= 2;
                }
                break;
            case 4:
                bg_next_low = ppu_read_pattern(bg_next_table | (bg_next_tile << 4) | ((v >> 12) & 7));
                break;
            case 6:
                bg_next_high = ppu_read_pattern_high(bg_next_table | (bg_next_tile << 4) | ((v >> 12) & 7) | 8, 0);
                break;
            case 7:
                ppu_increment_x();
                break;
        }
    }
    for (ppu_dot = last + 1; ppu_dot < target; ppu_dot++) {
        ppu_rendering_dot();
    }
    ppu_line_mode = PPU_LINE_PERDOT;
    return 1;
}

/**
 * Brings the current scanline up to date before something changes rendering state or reads it in a way the lazy
 * prediction can't answer. Replays the scanline dot by dot from its saved dot 0 state.
 */
FRNES_HOT(3_sync_ppu_sync) void ppu_sync() {
    if (ppu_line_mode != PPU_LINE_LAZY) {
        return;
    }
    if (ppu_dot == 0) {
        // nothing emulated yet, but the saved dot 0 state is about to go stale
        ppu_line_mode = PPU_LINE_PERDOT;
        return;
    }
    uint32_t target = ppu_dot;
    if (target > 256) {
        // In hblank the visible part of the scanline is final: produce it with the fast pass and the summarized
        // effect of dots 1-256 (as ppu_finish_lazy_line does), then run only dots 257 up to now dot by dot, instead
        // of replaying the whole scanline (MMC3 games write in hblank, Pinbot ~90 times a frame)
        PPU_STAT(ppu_stat_replays_hblank++);
        if (ppu_scanline < PPU_VISIBLE_SCANLINES) {
            if (!lazy_line_rendered) {
                ppu_render_line_fast();
            }
            if (lazy_hit_dot) {
                ppu_registers -> PPUSTATUS |= PPUSTATUS_SPRITE_0_HIT_FLAG;
            }
        }
        ppu_registers -> PPU_V ^= 0x0400;
        ppu_increment_y();
        sprite_zero_on_line = next_sprite_zero_on_line;
        for (ppu_dot = 257; ppu_dot < target; ppu_dot++) {
            ppu_dot = ppu_next_tail_event(ppu_dot);
            if (ppu_dot >= target) {
                break;
            }
            ppu_rendering_dot();
        }
        ppu_dot = target;
        ppu_line_mode = PPU_LINE_PERDOT;
        return;
    }
    PPU_STAT(ppu_stat_replays++);
    PPU_STAT(ppu_stat_replay_dots += target);
    PPU_STAT(ppu_stat_replays_early += (target < 65));
    if (__builtin_expect(lazy_x_switches, 0)) {
        ppu_replay_switches(target);
        return;
    }
    if ((target > 24) && ppu_replay_from_tile(target)) {
        return;
    }
    uint8_t status = ppu_registers -> PPUSTATUS & PPUSTATUS_VBLANK_FLAG;
    ppu_restore_line_start();
    for (ppu_dot = 0; ppu_dot < target; ppu_dot++) {
        if ((ppu_scanline == PPU_PRERENDER_SCANLINE) && (ppu_dot == 1)) {
            ppu_registers -> PPUSTATUS &= ~(PPUSTATUS_SPRITE_0_HIT_FLAG | PPUSTATUS_SPRITE_OVERFLOW_FLAG | PPUSTATUS_VBLANK_FLAG);
            status = 0;
        }
        ppu_rendering_dot();
    }
    // the replay sets sprite 0 / overflow itself, keep vblank as it was
    ppu_registers -> PPUSTATUS = (ppu_registers -> PPUSTATUS & ~PPUSTATUS_VBLANK_FLAG) | status;
    ppu_line_mode = PPU_LINE_PERDOT;
}

/**
 * Before a $2005 write ('data'): whether the lazy scanline can take it without being replayed dot by dot. The second
 * write only changes t, which dots 1-256 don't read (dot 257 copies it, when the line is finished). The first one also
 * changes fine X, which only moves the pixels from the write's dot on: the fast pass takes it as a switch at that
 * pixel (Castlevania III's intro writes $2005 about 110 times a frame mid line for its wavy scroll)
 */
static int ppu_scroll_write_lazy(uint8_t data) {
    if ((ppu_line_mode != PPU_LINE_LAZY) || (ppu_dot > 256)) {
        return 0;
    }
    uint8_t fine_x = data & 7;
    if (ppu_registers -> PPU_W || (fine_x == ppu_registers -> PPU_X) || (ppu_scanline >= PPU_VISIBLE_SCANLINES)) {
        return 1;
    }
    if (lazy_line_rendered || (lazy_x_switches == PPU_LAZY_X_SWITCHES)) {
        return 0;
    }
    // dots before ppu_dot are done: pixel ppu_dot - 1 (output on dot ppu_dot) is the first with the new fine X
    uint8_t pixel = ppu_dot ? ppu_dot - 1 : 0;
    if (lazy_x_switches && (lazy_x_pixel[lazy_x_switches - 1] == pixel)) {
        lazy_x_value[lazy_x_switches - 1] = (lazy_x_value[lazy_x_switches - 1] & LAZY_SPRITES_TOGGLE) | fine_x;
    } else {
        lazy_x_pixel[lazy_x_switches] = pixel;
        lazy_x_value[lazy_x_switches] = fine_x;
        lazy_x_switches++;
    }
    return 1;
}

/**
 * Before a $2000 write that changes more than the NMI enable ('data'): whether the lazy scanline can take it without
 * being replayed. A write changing only the nametable bits (and so only t) during dots 1-256 of a visible line changes
 * nothing the line's dots read: dot 257 copies t's horizontal bits, when the line is finished
 */
static int ppu_ctrl_write_lazy(uint8_t data) {
    return (ppu_line_mode == PPU_LINE_LAZY) && (ppu_dot != 0) && (ppu_dot <= 256) && (ppu_scanline < PPU_VISIBLE_SCANLINES) &&
        !((ppu_registers -> PPUCTRL ^ data) & ~PPUCTRL_BASE_NAMETABLE_ADDRESS);
}

/**
 * Before a $2001 write ('data'): whether the lazy scanline can take it without being replayed. One that only switches
 * the sprites on or off during dots 1-256 of a visible line, the background staying on (rendering, and so the sprite
 * evaluation and fetches, carry on as they were), only changes which pixels from the write's dot on show sprites (and
 * can set sprite 0 hit): the fast pass takes it as a switch at that pixel, with the fine X in effect there. One that
 * changes nothing but the emphasis bits (or nothing at all) is taken as it is, anywhere: FrNES doesn't draw emphasis
 * (its palettes' first 64 colors only), so nothing any dot draws or reads changes. (Wall (Demo) switches blue emphasis
 * on and off twice a line, ~196 lines a frame, each one replayed.) Drawing emphasis would have to revisit this: then
 * it is a switch at the write's pixel, as the sprite enable. Greyscale (bit 0) changes the colors, not taken
 */
#define PPU_MASK_EMPHASIS (PPUMASK_EMPHASIZE_RED | PPUMASK_EMPHASIZE_GREEN | PPUMASK_EMPHASIZE_BLUE)
static int ppu_mask_write_lazy(uint8_t data) {
    uint8_t mask = ppu_registers -> PPUMASK;
    uint8_t changed = (mask ^ data) & ~PPU_MASK_EMPHASIS;
    if (!changed) {
        return 1;
    }
    if ((ppu_line_mode != PPU_LINE_LAZY) || (ppu_dot == 0) || (ppu_dot > 256) || (ppu_scanline >= PPU_VISIBLE_SCANLINES) ||
        (changed != PPUMASK_ENABLE_SPRITE_RENDERING) || !(mask & PPUMASK_ENABLE_BACKGROUND_RENDERING) ||
        lazy_line_rendered) {
        return 0;
    }
    uint8_t pixel = ppu_dot - 1;
    if (lazy_x_switches && (lazy_x_pixel[lazy_x_switches - 1] == pixel)) {
        lazy_x_value[lazy_x_switches - 1] ^= LAZY_SPRITES_TOGGLE;
        return 1;
    }
    if (lazy_x_switches == PPU_LAZY_X_SWITCHES) {
        return 0;
    }
    lazy_x_pixel[lazy_x_switches] = pixel;
    lazy_x_value[lazy_x_switches] = ppu_registers -> PPU_X | LAZY_SPRITES_TOGGLE;
    lazy_x_switches++;
    return 1;
}

/**
 * Before a $2002 read: applies the predicted sprite 0 hit / overflow for the dots already emulated
 */
/**
 * $2002 reads racing the VBlank flag: a read one dot before it is set reads it clear and keeps it (and the NMI)
 * from being set this frame; a read on the dot it is set or the next one reads it set but cancels the NMI
 */
static uint8_t ppu_vbl_suppress = 0;
#ifndef PPU_NMI_DISABLE_WINDOW
#define PPU_NMI_DISABLE_WINDOW 3
#endif

// set when a catch-up raises the VBlank NMI, with the number of dots it ran past it (see cpu_run_devices)
uint8_t ppu_nmi_raised = 0;
uint32_t ppu_nmi_dots_after = 0;

#ifndef PPU_SPRITE_FLAGS_CLEAR_READ_DOT
#define PPU_SPRITE_FLAGS_CLEAR_READ_DOT 1
#endif
FRNES_HOT(3_sync_ppu_status_race) void ppu_status_race() {
#ifdef PPU_RACE_DEBUG
    if ((ppu_scanline == PPU_PRERENDER_SCANLINE) || ((ppu_scanline == 260) && (ppu_dot > 330))) {
        printf("RACE sl=%lu dot=%lu st=%02x\n", (unsigned long)ppu_scanline, (unsigned long)ppu_dot, ppu_registers -> PPUSTATUS);
    }
#endif
    // all three flags are cleared on dot 1 of the pre-render line, but the sprite 0 hit and overflow flags are read
    // at the end of the read cycle, so a read starting just before then already sees them cleared (the VBlank flag
    // is latched at the start of the read)
    // (not on dot 0: the previous scanline just ended, the flags are still set, SMB's sprite 0 wait loop in Mesen)
    if ((ppu_scanline == PPU_PRERENDER_SCANLINE) && (ppu_dot >= 1) && (ppu_dot <= PPU_SPRITE_FLAGS_CLEAR_READ_DOT)) {
        ppu_registers -> PPUSTATUS &= ~(PPUSTATUS_SPRITE_0_HIT_FLAG | PPUSTATUS_SPRITE_OVERFLOW_FLAG);
        return;
    }
    if (ppu_scanline != PPU_VBLANK_SCANLINE) {
        return;
    }
    if (ppu_dot == 1) {
        ppu_vbl_suppress = 1;
    } else if ((ppu_dot == 2) || (ppu_dot == 3)) {
        cpu_registers -> NMI_Request = 0;
    }
}

/**
 * $2000 writes racing VBlank: the flag clearing at dot 1 of the pre-render line is already seen by a write on that
 * dot (no NMI from enabling it there), and disabling the NMI within a dot of the flag being set stops the NMI it
 * raised
 */
int ppu_vblank_flag_for_nmi() {
    if ((ppu_scanline == PPU_PRERENDER_SCANLINE) && (ppu_dot == 1)) {
        return 0;
    }
    return (ppu_registers -> PPUSTATUS & PPUSTATUS_VBLANK_FLAG) != 0;
}

void ppu_nmi_disable_race() {
    if ((ppu_scanline == PPU_VBLANK_SCANLINE) && (ppu_dot <= PPU_NMI_DISABLE_WINDOW)) {
        cpu_registers -> NMI_Request = 0;
    }
}

// dots past the PPU's position at which a $2002 read sees the sprite 0 hit and overflow flags of a lazy scanline:
// none, the hit for pixel x shows from dot x + 1 on as on the per-dot path (Mesen 2.2.1: a $2002 poll in Punch-Out!!
// with the PPU done with dot 252 misses the hit of a sprite at x = 252; AccuracyCoin passes either way)
#ifndef PPU_STATUS_SPRITE_FLAGS_DELAY
#define PPU_STATUS_SPRITE_FLAGS_DELAY 0
#endif
// and the overflow flag only from a dot later than ppu_predict_overflow_dot gives (Mesen 2.2.1 with AccuracyCoin's
// $2002 flag timing, test 2, at Mesen's own CPU / PPU alignment: see APU_DMC_RATE_MESEN in apu.c)
#ifndef PPU_STATUS_OVERFLOW_LAG
#define PPU_STATUS_OVERFLOW_LAG 1
#endif

FRNES_HOT(3_sync_ppu_status) void ppu_sync_status() {
    PPU_STAT(ppu_stat_status_syncs++);
    // (dot 0: the line hasn't started, ppu_run sets it up on its first dot; the lazy state is still the previous
    // line's, the pre-render line's for scanline 0, and rendering from it drew row 0 from the wrong v)
    if ((ppu_scanline >= PPU_VISIBLE_SCANLINES) || (ppu_dot == 0)) {
        return;
    }
    if (ppu_line_mode == PPU_LINE_LAZY) {
        if (!lazy_line_rendered) {
            ppu_render_line_fast();
        }
        uint32_t read_dot = ppu_dot + PPU_STATUS_SPRITE_FLAGS_DELAY;
        if (lazy_hit_dot && (read_dot > lazy_hit_dot)) {
            ppu_registers -> PPUSTATUS |= PPUSTATUS_SPRITE_0_HIT_FLAG;
        }
    } else if (ppu_dot > 257) {
        // a per-dot line sets sprite 0 hit as it renders, and the overflow flag when it evaluates at dot 257; before
        // that, the overflow flag comes from the prediction as on a lazy line (Rad Racer polls $2002 at dot ~245 of a
        // line it wrote $2005 on)
        return;
    }
    // (evaluation, which sets the overflow flag, starts at dot 65)
    if ((ppu_dot > 65 + PPU_STATUS_OVERFLOW_LAG) && !(ppu_registers -> PPUSTATUS & PPUSTATUS_SPRITE_OVERFLOW_FLAG) && PPU_RENDERING_ENABLED()) {
        uint32_t overflow_dot = ppu_predict_overflow_dot();
        if (overflow_dot && (ppu_dot > overflow_dot + PPU_STATUS_OVERFLOW_LAG)) {
            ppu_registers -> PPUSTATUS |= PPUSTATUS_SPRITE_OVERFLOW_FLAG;
        }
    }
}


/*
 * After a $2002 read's catch-up (6502_readbyte.c), the CPU cycle before which none of the status flags can change, so
 * the reads before it need no catch-up (ppu_status_stable). Conservative, a whole line early: vblank sets at line
 * 241; sprite 0 hits no sooner than the first line it is on (OAM entry 0's y + 1, with rendering and both layers on);
 * the overflow flag sets no sooner than the first line whose evaluation finds 8 or more sprites (the OAM buckets).
 * The window keeps what it was worked out from (OAM's generation, PPUMASK, the sprite size, OAMADDR), which
 * ppu_status_stable compares: a register write or OAM DMA that changes them ends it with no code in the write paths.
 * Only from a visible line, and with OAMADDR 0 (another one starts evaluation elsewhere in OAM)
 */
FRNES_COLD void ppu_status_stable_update(void) {
    // (a PC found not to be a poll loop's: looked at again, the code there may have been switched)
    extern uint16_t ppu_poll_reject_reset(void);
    ppu_poll_reject_reset();
    ppu_registers -> STATUS_STABLE_UNTIL = cpu_devices_synced_cycle;
    ppu_registers -> STATUS_STABLE_OAM = ppu_oam_generation;
    ppu_registers -> STATUS_STABLE_MASK = ppu_registers -> PPUMASK;
    ppu_registers -> STATUS_STABLE_CTRL = ppu_registers -> PPUCTRL & PPUCTRL_SPRITE_SIZE;
    ppu_registers -> STATUS_STABLE_OAMADDR = ppu_registers -> OAMADDR;
    uint32_t line = ppu_scanline;
    if (line >= PPU_VISIBLE_SCANLINES) {
        return;
    }
    uint32_t until = PPU_VBLANK_SCANLINE;
    if (PPU_RENDERING_ENABLED()) {
        if (ppu_registers -> OAMADDR != 0) {
            return;
        }
        uint32_t height = (ppu_registers -> PPUCTRL & PPUCTRL_SPRITE_SIZE) ? 16 : 8;
        const uint8_t* oam = (const uint8_t*)SPRRAM_ADDR;
        if (!(ppu_registers -> PPUSTATUS & PPUSTATUS_SPRITE_0_HIT_FLAG) &&
            ((ppu_registers -> PPUMASK & (PPUMASK_ENABLE_BACKGROUND_RENDERING | PPUMASK_ENABLE_SPRITE_RENDERING)) == (PPUMASK_ENABLE_BACKGROUND_RENDERING | PPUMASK_ENABLE_SPRITE_RENDERING))) {
            uint32_t first = oam[0] + 1, last = oam[0] + height;
            if ((first <= line) && (line <= last)) {
                return;
            }
            if ((first > line) && (first < until)) {
                until = first;
            }
        }
        if (!(ppu_registers -> PPUSTATUS & PPUSTATUS_SPRITE_OVERFLOW_FLAG)) {
            uint32_t key = (ppu_oam_generation << 1) | (height >> 4);
            if (key != oam_bucket_key) {
                oam_bucket_key = key;
                ppu_build_oam_buckets(height);
            }
            for (uint32_t l = line; (l < until) && (l < PPU_VISIBLE_SCANLINES); l++) {
                if (oam_bucket_count[l] >= PPU_MAX_SPRITES_PER_LINE) {
                    until = l;
                    break;
                }
            }
        }
    }
    if (until > line) {
        ppu_registers -> STATUS_STABLE_UNTIL = ppu_dot_cpu_cycle(until, 0) - 1;
    }
}

/*
 * A $2002 poll loop's iterations skipped, exactly: within a stable window every $2002 read returns the same value, so
 * a loop of only "LDA $2002 (AND #imm) / Bxx back" or "BIT $2002 / Bxx back" that went round once with this value
 * (the previous read, at the same PC, one loop's cycles ago, returned it too) goes round again until a flag can change.
 * The read's instruction takes whole iterations' cycles (READ_SKIP), as many as end before the window's end, the
 * batch's end and the APU's next event, with no interrupt pending: the read after them is the one the loop would make
 * there, on the same cycle. In cold code: a few calls a frame once a loop is found
 */
static uint16_t poll_last_pc FRNES_COLD_BSS, poll_reject_pc FRNES_COLD_BSS;
static uint8_t poll_last_value FRNES_COLD_BSS, poll_last_loop FRNES_COLD_BSS, poll_last_irq FRNES_COLD_BSS;
static uint32_t poll_last_cycle FRNES_COLD_BSS;

FRNES_COLD uint16_t ppu_poll_reject_reset(void) {
    poll_reject_pc = 0;
    return 0;
}

#ifdef FRNES_HOST
// (host verify mode, POLL_VERIFY=1: a skip isn't made; the iterations it would have skipped are checked as they run:
// the same PC and value every loop's cycles until its end, no other $2002 read in between)
#include <stdlib.h>
static int poll_verify = -1;
static uint32_t verify_until, verify_next, verify_skips, verify_errors;
static uint16_t verify_pc;
static uint8_t verify_value, verify_loop, verify_active;
static int32_t verify_b_batch, verify_b_apu, verify_b_until; static uint32_t verify_b_mapper, verify_b_now;
static void poll_verify_report(void) {
    printf("POLLVERIFY skips=%u errors=%u\n", (unsigned)verify_skips, (unsigned)verify_errors);
}
static void poll_verify_fail(const char* what, uint16_t pc, uint8_t value) {
    if (verify_errors++ < 20) {
        printf("POLLVERIFY FAIL %s now=%u pc=%04X value=%02X expected pc=%04X value=%02X next=%u until=%u\n", what,
            (unsigned)cpu_cycle_now, pc, value, verify_pc, verify_value, (unsigned)verify_next, (unsigned)verify_until);
    }
    verify_active = 0;
}
// a $2002 read: inside a predicted skip it has to be the loop's next iteration (1: handled)
static int poll_verify_read(uint16_t pc, uint8_t value) {
    if (poll_verify < 0) {
        poll_verify = getenv("POLL_VERIFY") != NULL;
        if (poll_verify) {
            atexit(poll_verify_report);
        }
    }
    if (!poll_verify || !verify_active) {
        return 0;
    }
    if ((pc != verify_pc) || (value != verify_value) || (cpu_cycle_now != verify_next)) {
        poll_verify_fail("iteration", pc, value);
        return 0;
    }
    verify_next += verify_loop;
    if ((int32_t)(cpu_cycle_now - verify_until) >= 0) {
        // (the read after the skipped ones: the loop's own, from here on as without a skip)
        verify_active = 0;
        poll_last_cycle = cpu_cycle_now;
        return 1;
    }
    return 1;
}
#endif

FRNES_COLD void ppu_poll_skip(void) {
    extern int32_t apu_cycles_to_event;
    uint16_t pc = cpu_registers -> IO_PC;
    // (used once: a later read through a path that doesn't set it never sees this one's)
    cpu_registers -> IO_PC = 0;
    uint8_t value = (ppu_registers -> PPUSTATUS & 0xe0) | (ppu_registers -> PPUOPENBUS & 0x1f);
#ifdef FRNES_HOST
    if (poll_verify_read(pc, value)) {
        return;
    }
#endif
    if ((poll_last_pc == pc) && (poll_last_value == value) && (cpu_cycle_now - poll_last_cycle == poll_last_loop)) {
        // (an NMI pending would be taken after this instruction; an IRQ only if it wasn't there one iteration ago:
        // asserted then and the loop came round all the same, it is masked, and nothing in the loop unmasks it)
        if (cpu_registers -> NMI_Request || (cpu_registers -> IRQ_Request && !poll_last_irq)) {
            poll_last_cycle = cpu_cycle_now;
            poll_last_irq = cpu_registers -> IRQ_Request;
            return;
        }
        // (signed: a read already past the batch's end, in the instruction that ends it, skips nothing)
        int32_t limit = (int32_t)(ppu_registers -> STATUS_STABLE_UNTIL - cpu_cycle_now);
        int32_t batch = (int32_t)(cpu_batch_end - cpu_cycle_now);
        int32_t apu = (int32_t)(cpu_devices_synced_cycle + apu_cycles_to_event - cpu_cycle_now);
        if (batch < limit) {
            limit = batch;
        }
        if (apu < limit) {
            limit = apu;
        }
        // (a mapper's CPU cycle counter: the batch was sized for it, and a batch cut mid way ends after this instruction)
        if (mapper_cpu_timer) {
            uint32_t mapper = mapper_cycles_to_event();
            if ((limit > 0) && (mapper < (uint32_t)limit)) {
                limit = (int32_t)mapper;
            }
        }
        if (cpu_batch_cut) {
            limit = 0;
        }
        uint32_t skip = (limit > 1) ? (((uint32_t)limit - 1) / poll_last_loop) * poll_last_loop : 0;
        if (skip > 0xff00) {
            skip = (0xff00 / poll_last_loop) * poll_last_loop;
        }
        cpu_registers -> READ_SKIP = (uint16_t)skip;
        poll_last_irq = cpu_registers -> IRQ_Request;
#ifdef FRNES_HOST
        if (poll_verify && skip) {
            cpu_registers -> READ_SKIP = 0;
            verify_skips++;
            verify_active = 1;
            verify_pc = pc;
            verify_value = value;
            verify_loop = poll_last_loop;
            verify_next = cpu_cycle_now + poll_last_loop;
            verify_until = cpu_cycle_now + skip;
            verify_b_batch = batch; verify_b_apu = apu; verify_b_until = (int32_t)(ppu_registers -> STATUS_STABLE_UNTIL - cpu_cycle_now);
            verify_b_mapper = mapper_cpu_timer ? mapper_cycles_to_event() : 0xffffffff; verify_b_now = cpu_cycle_now;
            poll_last_cycle = cpu_cycle_now;
            return;
        }
#endif
        poll_last_cycle = cpu_cycle_now + skip;
        return;
    }
    // a loop at this PC? The read's instruction is the 3 bytes before it, the branch back to it after (or after an AND #)
    uint16_t start = pc - 3;
    uint32_t loop = 0;
    if ((pc != 0) && !((start >= 0x2000) && (start < 0x6000))) {
        uint8_t op = NES_WINDOW_BYTE(start);
        uint16_t operand = NES_WINDOW_BYTE((uint16_t)(start + 1)) | (NES_WINDOW_BYTE((uint16_t)(start + 2)) << 8);
        if (((op == 0xad) || (op == 0x2c)) && ((operand & 0xe007) == 0x2002)) {
            uint16_t q = pc;
            loop = 4;
            if ((op == 0xad) && (NES_WINDOW_BYTE(q) == 0x29)) {
                q += 2;
                loop += 2;
            }
            uint8_t branch = NES_WINDOW_BYTE(q);
            uint16_t target = (uint16_t)(q + 2 + (int8_t)NES_WINDOW_BYTE((uint16_t)(q + 1)));
            if (((branch & 0x1f) == 0x10) && (target == start)) {
                loop += 3 + ((((uint16_t)(q + 2) ^ target) & 0xff00) ? 1 : 0);
            } else {
                loop = 0;
            }
        }
    }
    if (!loop) {
        poll_reject_pc = pc;
        return;
    }
    poll_last_pc = pc;
    poll_last_value = value;
    poll_last_loop = (uint8_t)loop;
    poll_last_cycle = cpu_cycle_now;
    poll_last_irq = cpu_registers -> IRQ_Request;
}

// (in the read slot, before read_byte_6502_c: tools/hot_slots.py)
FRNES_HOT(8_read_status) void ppu_status_read_sync(void) {
    if (ppu_registers -> STATUS_SKIP) {
        // (and a poll loop's iterations, if this read is one's: not at a PC already found not to be)
        if (cpu_registers -> IO_PC != poll_reject_pc) {
            ppu_poll_skip();
        }
        return;
    }
#ifdef FRNES_HOST
    if ((poll_verify > 0) && verify_active) {
        extern int32_t apu_cycles_to_event;
        printf("POLLVERIFY why: until-now=%d oam=%d mask=%d ctrl=%d oamaddr=%d apu=%d (now-synced=%d) batch_end-now=%d\n", (int)(ppu_registers -> STATUS_STABLE_UNTIL - cpu_cycle_now),
            ppu_registers -> STATUS_STABLE_OAM == ppu_oam_generation, ppu_registers -> STATUS_STABLE_MASK == ppu_registers -> PPUMASK,
            ppu_registers -> STATUS_STABLE_CTRL == (ppu_registers -> PPUCTRL & PPUCTRL_SPRITE_SIZE), ppu_registers -> STATUS_STABLE_OAMADDR == ppu_registers -> OAMADDR,
            (int)apu_cycles_to_event, (int)(cpu_cycle_now - cpu_devices_synced_cycle), (int)(cpu_batch_end - cpu_cycle_now));
        printf("POLLVERIFY at prediction: now=%u batch=%d apu=%d until=%d mapper=%u timer=%u cut=%u\n", (unsigned)verify_b_now, (int)verify_b_batch, (int)verify_b_apu, (int)verify_b_until, (unsigned)verify_b_mapper, (unsigned)mapper_cpu_timer, (unsigned)cpu_batch_cut);
        poll_verify_fail("caught up read", cpu_registers -> IO_PC, ppu_registers -> PPUSTATUS);
    }
#endif
    ppu_sync_status();
    ppu_status_race();
    // (how long the flags stay as they are: not with a mapper the PPU's catch-ups hook. One they clock (MMC3's
    // scanline counter) is clocked by the next catch-up in the same order, and an armed IRQ ends the batch at its clock)
    // (the MMC2 / MMC4 latches are hooks, but only switch CHR for the fetches after them, which the CPU can't see: a
    // skipped poll loop's catch-up later does them all the same. Fire Emblem Gaiden's battles wait for sprite 0 hit
    // ~1350 times a frame)
    if (!(mapper_ext_attributes | mapper_sprite_chr_split)) {
        ppu_status_stable_update();
    }
}

int ppu_take_frame() {
    uint32_t ready = ppu_frame_ready;
    ppu_frame_ready = 0;
    return ready;
}

uint32_t ppu_cycles_to_next_event() {
    // the next point the CPU loop has to stop at: the PPU passing dot 1 of scanline 241 (vblank / NMI), or the
    // end of the frame (so it can be presented before the next frame's scanlines are rendered)
    uint32_t position = ppu_scanline * PPU_DOTS_PER_SCANLINE + ppu_dot + ppu_pending_dots;
    uint32_t vblank = PPU_VBLANK_SCANLINE * PPU_DOTS_PER_SCANLINE + 2;
    uint32_t target = (position < vblank) ? vblank : PPU_SCANLINES_PER_FRAME * PPU_DOTS_PER_SCANLINE;
    // (an odd frame with rendering on skips the pre-render line's last dot: the frame ends a dot sooner)
    if ((target != vblank) && ppu_registers -> ODDFRAME && PPU_RENDERING_ENABLED() &&
        (position < PPU_PRERENDER_SCANLINE * PPU_DOTS_PER_SCANLINE + 340)) {
        target--;
    }
    if (target <= position) {
        return 1;
    }
    // the end of the frame rounded down: a batch that stops a little early is followed by a short one, one that
    // stops late puts the frame boundary (where the frame is presented) at a later instruction
    if (target != vblank) {
        uint32_t cycles = (target - position) / 3;
        return cycles ? cycles : 1;
    }
    return (target - position + 2) / 3;
}

uint32_t ppu_cycles_to_vblank_nmi() {
    // CPU cycles from the PPU's current position to dot 1 of scanline 241, where the VBlank NMI comes up
    uint32_t position = ppu_scanline * PPU_DOTS_PER_SCANLINE + ppu_dot + ppu_pending_dots;
    uint32_t vblank = PPU_VBLANK_SCANLINE * PPU_DOTS_PER_SCANLINE + 1;
    if (position > vblank) {
        vblank += PPU_SCANLINES_PER_FRAME * PPU_DOTS_PER_SCANLINE;
    }
    return (vblank - position) / 3;
}

// while ppu_run() runs: the position (scanline * 341 + dot, past the end of the frame for any left over) its dots
// end at, the start of CPU cycle cpu_devices_synced_cycle
static uint32_t ppu_run_end_position;
static uint8_t ppu_running;

uint32_t ppu_dot_cpu_cycle(uint32_t scanline, uint32_t dot) {
    int32_t position = ppu_scanline * PPU_DOTS_PER_SCANLINE + ppu_dot;
    int32_t reference = ppu_running ? (int32_t)ppu_run_end_position : position + (int32_t)ppu_pending_dots;
    int32_t target = scanline * PPU_DOTS_PER_SCANLINE + dot;
    // (a target a little behind the PPU is a moment just past, a cycle already gone: a clock on a per dot line is
    // handled with the PPU at its dot, and MMC5's IRQ comes 2 dots before it. Only one more than half a frame
    // behind is next frame's: a line 0 target from the pre-render line)
    if (target < position - (PPU_SCANLINES_PER_FRAME * PPU_DOTS_PER_SCANLINE) / 2) {
        // already passed this frame: the next one's (this frame ends a dot sooner if odd and rendering)
        target += PPU_SCANLINES_PER_FRAME * PPU_DOTS_PER_SCANLINE;
        if (ppu_registers -> ODDFRAME && PPU_RENDERING_ENABLED() &&
            (position <= PPU_PRERENDER_SCANLINE * PPU_DOTS_PER_SCANLINE + PPU_ODD_SKIP_LAST_DOT)) {
            target--;
        }
    }
    // dot n from the start of cycle cpu_devices_synced_cycle (negative: earlier) is seen during cycle
    // floor((n + 2) / 3) from it
    int32_t n = target - reference + 2;
    int32_t cycles = (n >= 0) ? (n / 3) : -((2 - n) / 3);
    return cpu_devices_synced_cycle + cycles;
}

int ppu_run(uint32 cpu_cycles) {
    PPU_STAT(ppu_stat_run_calls++);
    uint32_t dots = cpu_cycles * 3 + ppu_pending_dots;
    // a catch-up with rendering off past the line's dot 1 (the vblank / pre-render flags) that ends before the line
    // does only fills the scanline's backdrop, as the loop's rendering off path below does (register writes with
    // rendering off: palette uploads mid frame, a $2006 / $2007 write every few cycles). (Placed out of the way: the
    // hot code after it keeps its offsets in the slot, and so its I-cache sets against the assembly core's)
    if (__builtin_expect(!PPU_RENDERING_ENABLED() && (ppu_dot >= 2) && (ppu_dot + dots < PPU_DOTS_PER_SCANLINE), 0)) {
        uint32_t dot = ppu_dot;
        ppu_pending_dots = 0;
        ppu_line_mode = PPU_LINE_PERDOT;
        if ((ppu_scanline < PPU_VISIBLE_SCANLINES) && (dot <= PPU_FRAMEBUFFER_WIDTH)) {
            uint32_t first = ((ppu_blank_filled_dot > dot) ? ppu_blank_filled_dot : dot) - 1;
            uint32_t last = dot + dots - 1;
            if (last > PPU_FRAMEBUFFER_WIDTH) {
                last = PPU_FRAMEBUFFER_WIDTH;
            }
            if (last > first) {
                // (mostly a few dots between two register writes: no memset call)
                uint8_t color = ppu_blank_color();
                uint8_t* out = ppu_line_buffer + first;
                uint8_t* end = ppu_line_buffer + last;
                do {
                    *out++ = color;
                } while (out != end);
                ppu_line_buffer_used = 1;
            }
        }
        ppu_dot = dot + dots;
        return 0;
    }
    int frame_done = 0;
    ppu_pending_dots = 0;
    ppu_run_end_position = ppu_scanline * PPU_DOTS_PER_SCANLINE + ppu_dot + dots;
    ppu_running = 1;

    while (dots) {
        uint32_t scanline = ppu_scanline;
        uint32_t rendering_line = (scanline < PPU_VISIBLE_SCANLINES) || (scanline == PPU_PRERENDER_SCANLINE);

        if (rendering_line && PPU_RENDERING_ENABLED()) {
            // the pre-render line is one dot shorter on odd frames when rendering
            // (only if rendering was on when the PPU got to dot 339: switched on later, the line runs to 340)
            uint32_t line_length = ((scanline == PPU_PRERENDER_SCANLINE) && ppu_registers -> ODDFRAME &&
                (ppu_dot <= PPU_ODD_SKIP_LAST_DOT)) ? 340 : PPU_DOTS_PER_SCANLINE;

            if (ppu_dot == 0) {
                // (MMC3 with only the background at $1000: the pre-render line's extra clock, given with its A12 dot
                // (5) at the start of the line, so the per dot paths don't look for it; nothing reads the counter in
                // between)
                if ((scanline == PPU_PRERENDER_SCANLINE) && mapper_clock_dot) {
                    if (ppu_mapper_prerender_clock()) {
                        ppu_clock_mapper_at(PPU_MAPPER_PRERENDER_DOT, 2);
                    }
                    // (with 8x16 sprites the line's A12 clock depends on what secondary OAM still holds)
                    if (__builtin_expect(ppu_registers -> PPUCTRL & PPUCTRL_SPRITE_SIZE, 0)) {
                        ppu_mapper_prerender_8x16();
                    }
                }
                uint32_t stale_sprites = (scanline < PPU_VISIBLE_SCANLINES) && (ppu_sprite_fetch_line != 0xffffffff) &&
                    (ppu_sprite_fetch_line != ppu_sprite_x_line);
                if (stale_sprites) {
                    // (objects fetched on a line whose dot 339 passed with rendering off: drawn at once from x 0, then
                    // the line's own fetches and dot 339 go on as usual; the per dot path draws it)
                    ppu_take_stale_sprites();
                    ppu_sprite_x_origin = 0;
                }
                if (stale_sprites) {
                    ppu_line_mode = PPU_LINE_PERDOT;
                } else {
                    ppu_line_mode = PPU_LINE_LAZY;
                    lazy_line_rendered = 0;
                    ppu_save_line_start();
                }
            }

            if (ppu_line_mode == PPU_LINE_LAZY) {
                uint32_t step = line_length - ppu_dot;
                if (step > dots) {
                    ppu_lazy_advance(ppu_dot + dots);
                    dots = 0;
                } else {
                    dots -= step;
                    ppu_lazy_advance(line_length);
                    ppu_finish_lazy_line(line_length);
                    ppu_sprite_x_line = scanline;
                    ppu_sprite_fetch_line = scanline;
                    ppu_sprite_fetched = 8;
                    PPU_STAT(ppu_stat_lazy_lines++);
                    ppu_dot = PPU_DOTS_PER_SCANLINE;
                }
            } else {
                // rendering, one dot at a time, skipping the idle dots of the sprite fetches
                uint32_t event = ppu_next_tail_event(ppu_dot);
                if (event != ppu_dot) {
                    uint32_t skip = event - ppu_dot;
                    if (skip > dots) {
                        skip = dots;
                    }
                    ppu_dot += skip;
                    dots -= skip;
                    continue;
                }
                if ((scanline == PPU_PRERENDER_SCANLINE) && (ppu_dot == 1)) {
                    ppu_registers -> PPUSTATUS &= ~(PPUSTATUS_SPRITE_0_HIT_FLAG | PPUSTATUS_SPRITE_OVERFLOW_FLAG | PPUSTATUS_VBLANK_FLAG);
                }
                ppu_rendering_dot();
                PPU_STAT(ppu_stat_perdot_dots++);
                dots--;
                ppu_dot++;
                if (ppu_dot == line_length) {
                    if (line_length == 340) {
                        ppu_fetch_sprites(0);
                    }
                    ppu_dot = PPU_DOTS_PER_SCANLINE;
                }
            }
        } else {
            // not rendering, skip ahead to the end of the scanline or the next event
            ppu_line_mode = PPU_LINE_PERDOT;
            uint32_t step = PPU_DOTS_PER_SCANLINE - ppu_dot;
            if (step > dots) {
                step = dots;
            }
            if ((ppu_dot <= 1) && (ppu_dot + step > 1)) {
                if (scanline == PPU_VBLANK_SCANLINE) {
                    // a $2002 read just before this dot keeps the flag (and the NMI) from being set this frame
                    if (!ppu_vbl_suppress) {
                        ppu_registers -> PPUSTATUS |= PPUSTATUS_VBLANK_FLAG;
                        if (ppu_registers -> PPUCTRL & PPUCTRL_VBLANK_NMI_ENABLE) {
                            cpu_registers -> NMI_Request = 1;
                            // how far past the NMI this catch-up runs (dot 1 is the (2 - ppu_dot)th dot from here)
                            ppu_nmi_raised = 1;
                            ppu_nmi_dots_after = dots - (2 - ppu_dot);
                        }
                    }
                    ppu_vbl_suppress = 0;
                } else if (scanline == PPU_PRERENDER_SCANLINE) {
                    ppu_registers -> PPUSTATUS &= ~(PPUSTATUS_SPRITE_0_HIT_FLAG | PPUSTATUS_SPRITE_OVERFLOW_FLAG | PPUSTATUS_VBLANK_FLAG);
                }
            }
            // a visible scanline with rendering off shows the backdrop color, or the palette entry v points at when it
            // is in $3F00-$3FFF, for the dots this catch-up covers (pixel x is dot x + 1): register accesses sync the
            // PPU first, so a v or palette change shows from its dot on. (If rendering gets turned on part way
            // through, the per-dot path fills in the rest of the line buffer.)
            if ((scanline < PPU_VISIBLE_SCANLINES) && (ppu_dot <= PPU_FRAMEBUFFER_WIDTH)) {
                uint32_t from = (ppu_blank_filled_dot > ppu_dot) ? ppu_blank_filled_dot : ppu_dot;
                uint32_t first = from ? from - 1 : 0;
                uint32_t last = ppu_dot + step - 1;
                if (last > PPU_FRAMEBUFFER_WIDTH) {
                    last = PPU_FRAMEBUFFER_WIDTH;
                }
                if (last > first) {
                    memset(ppu_line_buffer + first, ppu_blank_color(), last - first);
                    ppu_line_buffer_used = 1;
                }
            }
            ppu_dot += step;
            dots -= step;
        }

        if (ppu_dot >= PPU_DOTS_PER_SCANLINE) {
            // scanlines that went through the per-dot path (or had rendering off) are output from the line buffer
            if (ppu_line_buffer_used) {
                if (scanline < PPU_VISIBLE_SCANLINES) {
                    ppu_output_line(ppu_line_buffer);
                }
                ppu_line_buffer_used = 0;
            }
            ppu_dot = 0;
            ppu_blank_filled_dot = 0;
            ppu_mapper_clocked = 0;
            ppu_scroll_resume_dot = 0;
            ppu_sprite_x_origin = 0xffffffff;
            if (++ppu_scanline == PPU_VISIBLE_SCANLINES) {
                ppu_bus_frame_count++;
            }
            if ((ppu_scanline == PPU_PRERENDER_SCANLINE) && PPU_RENDERING_ENABLED()) {
                ppu_prerender_oam();
            }
            if (ppu_scanline == PPU_SCANLINES_PER_FRAME) {
                ppu_scanline = 0;
                ppu_frame_count++;
                ppu_registers -> ODDFRAME ^= 1;
                frame_done = 1;
                ppu_frame_ready = 1;
                // stop at the frame boundary so the frame can be presented before the next one is rendered
                ppu_pending_dots = dots;
                break;
            }
        }
    }

    ppu_running = 0;
    return frame_done;
}

// (test builds' traces)
uint32_t ppu_current_dot() {
    return ppu_dot;
}

#if 1
// save states (state.c): the PPU's position, its rendering pipeline and sprite evaluation (its registers are in
// memory.c's register page, OAM and the palette in its buffers); not the picture or anything worked out from the rest
#include "state.h"
#define PPU_STATE(X, A) X(ppu_frame_count) X(ppu_bus_frame_count) X(ppu_scanline) X(ppu_dot) X(ppu_pending_dots) \
    X(ppu_frame_ready) A(ppu_open_bus_frame) X(ppu_mapper_clock_a12_dot) X(ppu_mapper_clocked) X(ppu_line_buffer_used) \
    X(ppu_blank_filled_dot) X(bg_next_tile) X(bg_next_table) X(bg_next_attribute) X(bg_next_low) X(bg_next_high) \
    X(bg_shift_pattern_low) X(bg_shift_pattern_high) X(bg_shift_attribute_low) X(bg_shift_attribute_high) \
    X(sprite_count) X(sprite_zero_on_line) A(sprite_y) A(sprite_tile) A(sprite_attribute) A(sprite_x) \
    A(sprite_pattern_low) A(sprite_pattern_high) X(next_sprite_count) X(next_sprite_zero_on_line) A(secondary) \
    X(ppu_oam_corrupt_rows) X(ppu_sprite_x_line) X(ppu_sprite_x_origin) X(ppu_sprite_fetch_line) X(ppu_sprite_fetched) \
    X(ppu_evaluate_end_oamaddr) X(ppu_evaluate_end_secondary) X(ppu_oam2_row) X(ppu_scroll_resume_dot) \
    X(ppu_evaluate_overflow_dot) X(ppu_copy_buffer) X(oam2_off_dot) X(oam2_off_line) X(oam2_off_frame) X(oam2_missed) \
    X(oam2_missed_line) X(oam2_missed_frame) X(ppu_line_mode) X(lazy_line_rendered) X(lazy_x_switches) \
    X(lazy_hit_dot) A(lazy_x_pixel) A(lazy_x_value) X(line_start) X(ppu_vbl_suppress) X(ppu_nmi_raised) \
    X(ppu_nmi_dots_after) X(ppu_run_end_position) X(ppu_running)

// (by value into a buffer and one state_put / state_get: a call per variable made this code big enough, in this link
// time optimized unit, to move the hot paths' inlining)
#define PPU_STATE_X_SAVE(var) { __typeof__(var) state_value = (var); memcpy(at, &state_value, sizeof(state_value)); at += sizeof(state_value); }
#define PPU_STATE_X_LOAD(var) { __typeof__(var) state_value; memcpy(&state_value, at, sizeof(state_value)); (var) = state_value; at += sizeof(state_value); }
#define PPU_STATE_A_SAVE(array) for (uint32_t state_i = 0; state_i < sizeof(array) / sizeof((array)[0]); state_i++) PPU_STATE_X_SAVE((array)[state_i])
#define PPU_STATE_A_LOAD(array) for (uint32_t state_i = 0; state_i < sizeof(array) / sizeof((array)[0]); state_i++) PPU_STATE_X_LOAD((array)[state_i])

FRNES_COLD_TEXT void ppu_state_save(state_writer_t* writer) {
    uint8_t buffer[STATE_SIZE(PPU_STATE)];
    uint8_t* at = buffer;
    PPU_STATE(PPU_STATE_X_SAVE, PPU_STATE_A_SAVE)
    state_put(writer, buffer, sizeof(buffer));
}

FRNES_COLD_TEXT int ppu_state_load(state_reader_t* reader, uint16_t version) {
    if ((version != PPU_STATE_VERSION) || (reader -> size != STATE_SIZE(PPU_STATE))) {
        return -1;
    }
    uint8_t buffer[STATE_SIZE(PPU_STATE)];
    uint8_t* at = buffer;
    state_get(reader, buffer, sizeof(buffer));
    PPU_STATE(PPU_STATE_X_LOAD, PPU_STATE_A_LOAD)
    // (the caches of OAM, the nametables and the palette are worked out again)
    ppu_oam_generation++;
    ppu_nametable_generation++;
    oam_bucket_key = 0xffffffff;
    row_key = 0xffffffff;
    ppu_palette_dirty = PPU_PALETTE_DIRTY_ALL;
    return 0;
}
#endif

/**
 * Before a $2002-$2004, $2006 or $2007 write: whether the lazy scanline can take it without being brought up to date. A first write only
 * changes t (its high bits) and the write toggle, which a lazy line's dots 1-256 don't read and its saved dot 0 state
 * doesn't hold: dot 257's copy, when the line is finished, takes it from t as the per dot path does. Up to dot 256
 * (after, dot 257 has copied t already). The second write (v = t) still brings the line up to date. (Rockman 6's
 * title: a $2006 pair at dots ~205 / ~230 on every other line while the logo animates, ~94 lines a frame; the replay
 * now starts at the second write.) The others aren't taken. Out of line and cold: ppu_sync_lazy_write is in the
 * frame slot
 */
static FRNES_COLD_TEXT int ppu_other_write_lazy(uint32_t reg, uint8_t data) {
    if (reg != 6) {
        return 0;
    }
    if (!ppu_registers -> PPU_W) {
        return ppu_dot <= 256;
    }
    // the second write, v = t, mid line: taken as a switch the fast pass draws (ppu_v_switch_tiles) and the replays
    // apply (ppu_replay_switches), on a visible line with the background on, before the fast pass has run, once a line
    // and with no fetch hooks; dots 2-256 (from 257, dot 257's copy and the next line's fetches would follow it)
    uint32_t dot = ppu_dot;
    if ((ppu_line_mode != PPU_LINE_LAZY) || (ppu_scanline >= PPU_VISIBLE_SCANLINES) || (dot < 2) || (dot > 256) ||
        lazy_line_rendered || (lazy_x_switches == PPU_LAZY_X_SWITCHES) || ppu_fetch_hooks ||
        !(ppu_registers -> PPUMASK & PPUMASK_ENABLE_BACKGROUND_RENDERING)) {
        return 0;
    }
    for (uint32_t i = 0; i < lazy_x_switches; i++) {
        if (lazy_x_value[i] & LAZY_V_SWITCH) {
            return 0;
        }
    }
#ifdef FRNES_HOST
    // the host's self-check (HOST_MIDV_LINES): each frame's event lines, their pixels compared between a normal run
    // and HOST_MIDV_OFF's, where the second write isn't taken and the line goes per dot as before (perf_runs/rm6)
    if (host_midv_off < 0) {
        host_midv_off = getenv("HOST_MIDV_OFF") != NULL;
    }
    if (host_midv_count < sizeof(host_midv_lines)) {
        host_midv_lines[host_midv_count++] = ppu_scanline;
    }
    if (host_midv_off) {
        return 0;
    }
#endif
    lazy_v_dot = dot;
    lazy_v_value = (ppu_registers -> PPU_T & 0xff00) | data;
    lazy_x_pixel[lazy_x_switches] = dot - 1;
    lazy_x_value[lazy_x_switches++] = ppu_registers -> PPU_X | LAZY_V_SWITCH;
    return 1;
}

__attribute__((noinline)) int ppu_sync_lazy_write(uint32_t reg, uint8_t data) {
    switch (reg) {
        case 0: return ppu_ctrl_write_lazy(data);
        case 1: return ppu_mask_write_lazy(data);
        case 5: return ppu_scroll_write_lazy(data);
    }
    return ppu_other_write_lazy(reg, data);
}
