#include "mapper.h"
#include "memory.h"
#include "cartridge.h"
#include "apu.h"
#include "apu_synth.h"
#include "ppu.h"
#include "cpu.h"

uint8_t mapper_irq_flag = 0;
// set when a mapper that saves its own memory (board_saves: the FDS's disk sides) changed it
volatile uint8_t mapper_save_written = 0;
uint32_t mapper_clock_dot = 0;
uint32_t mapper_irq_armed = 0;

static uint32_t mapper_number = 0;
// MMC6 (see mmc6_init)
static uint8_t mmc6;
static void mmc6_write(uint16_t address, uint8_t data);
static void bandai_write(uint16_t address, uint8_t data);
static void mapper_write_select();
static void bandai_cpu_run(uint32_t cycles);
static uint8_t bandai_irq_enabled, bandai_submapper;
static uint16_t bandai_counter;
extern uint32_t ppu_nametable_generation;

static void update_irq_line() {
    // (shared with the APU's frame counter and DMC IRQs)
    cpu_irq_update_line();
}

static void prg_ram_access(uint32_t mode);

// (page counts are powers of two on nearly every cartridge: a mask then, as the SH4 has no divide instruction and '%'
// is a libgcc call, ~8 of them in a 16 kB PRG switch: Zelda II's overworld ~350 switches a frame every ~18 frames)
static inline uint32_t page_wrap(uint32_t page, uint32_t pages) {
    return (pages & (pages - 1)) ? (page % pages) : (page & (pages - 1));
}

static uint8_t* prg_page(uint32_t page) {
    return ROM_pages[page_wrap(page, num_8k_ROM_pages)];
}

static uint8_t* chr_page(uint32_t page) {
    // CHR RAM cartridges keep their pages in VROM_pages too (see cartridge.c)
    uint32_t pages = num_1k_VROM_pages ? num_1k_VROM_pages : num_1k_VRAM_pages;
    return VROM_pages[pages ? page_wrap(page, pages) : 0];
}

/**
 * UxROM (mapper 2): a switchable 16 kB PRG bank at $8000, the last one fixed at $C000
 */
static void prg_16k(uint32_t slot, uint32_t bank) {
    set_6502_bank_pointer(slot, prg_page(bank * 2));
    set_6502_bank_pointer(slot + 1, prg_page(bank * 2 + 1));
}

static void prg_32k(uint32_t bank) {
    for (uint32_t i = 0; i < 4; i++) {
        set_6502_bank_pointer(i, prg_page(bank * 4 + i));
    }
}

static void chr_4k(uint32_t half, uint32_t bank) {
    for (uint32_t i = 0; i < 4; i++) {
        set_ppu_pattern_table_bank_pointer(half * 4 + i, chr_page(bank * 4 + i));
    }
}

static void chr_8k(uint32_t bank) {
    chr_4k(0, bank * 2);
    chr_4k(1, bank * 2 + 1);
}

static void uxrom_init() {
    prg_16k(0, 0);
    prg_16k(2, num_8k_ROM_pages / 2 - 1);
}

static __attribute__((noinline)) void uxrom_write(uint16_t address, uint8_t data) {
    (void)address;
    prg_16k(0, data);
}

/**
 * CNROM (mapper 3): an 8 kB CHR bank
 */
static __attribute__((noinline)) void cnrom_write(uint16_t address, uint8_t data) {
    (void)address;
    ppu_sync();
    chr_8k(data);
}

/**
 * AxROM (mapper 7): a 32 kB PRG bank and one screen mirroring, bit 4 picks the nametable
 */
static uint8_t axrom_bank;

static void axrom_init() {
    axrom_bank = 0;
    prg_32k(0);
    set_ppu_nametable_mirroring(NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW);
}

static __attribute__((noinline)) void axrom_write(uint16_t address, uint8_t data) {
    (void)address;
    uint8_t changed = axrom_bank ^ data;
    axrom_bank = data;
    // PRG banking doesn't affect rendering: only a nametable change has to bring the scanline up to date first
    // (Battletoads switches PRG banks many times a frame)
    if (changed & 0x10) {
        ppu_sync();
        set_ppu_nametable_mirroring((data & 0x10) ? NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH : NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW);
    }
    if (changed & 0x0f) {
        prg_32k(data & 0x0f);
    }
}

/**
 * MMC1 (mapper 1): registers loaded through a 5 bit serial shift register. A write with bit 7 set resets it (and
 * sets PRG mode 3); writes on consecutive CPU cycles (the two writes of a read-modify-write) only count the first.
 * With 512 kB PRG (SUROM) bit 4 of the CHR 0 register selects the 256 kB half.
 */
static uint8_t mmc1_shift, mmc1_count, mmc1_control, mmc1_chr0, mmc1_chr1, mmc1_prg;
static uint32_t mmc1_last_write_cycle;

// the PRG banks and PRG RAM (the PRG register's part, and the control register's PRG mode)
static void mmc1_apply_prg() {
    uint32_t outer = (num_8k_ROM_pages > 32) ? ((mmc1_chr0 & 0x10) ? 16 : 0) : 0;
    uint32_t last = (num_8k_ROM_pages > 32) ? 15 : (num_8k_ROM_pages / 2 - 1);
    uint32_t bank = mmc1_prg & 0x0f;
    switch ((mmc1_control >> 2) & 3) {
        case 0:
        case 1:
            prg_32k((outer + bank) >> 1);
            break;
        case 2:
            prg_16k(0, outer);
            prg_16k(2, outer + bank);
            break;
        case 3:
            prg_16k(0, outer + bank);
            prg_16k(2, outer + last);
            break;
    }
    // PRG RAM off while bit 4 of the PRG register is set (MMC1B and later, as Mesen 2 treats every MMC1 it doesn't
    // know as MMC1A); the MMC1A's (mapper 155) is always on
    if (WRAM != NULL) {
        prg_ram_access(((mmc1_prg & 0x10) && __builtin_expect(cartridge_mapper_number != 155, 1)) ? 2 : 0);
        // SXROM (512 kB PRG, 32 kB PRG RAM): bits 2-3 of the CHR 0 register select the 8 kB RAM bank
        if ((num_8k_ROM_pages > 32) && (num_8k_WRAM_pages == 4)) {
            set_wram_bank_pointer(WRAM + ((mmc1_chr0 >> 2) & 3) * 0x2000);
        }
    }
}

// the CHR banks (the CHR registers' part, and the control register's CHR mode)
static void mmc1_apply_chr() {
    if (mmc1_control & 0x10) {
        chr_4k(0, mmc1_chr0);
        chr_4k(1, mmc1_chr1);
    } else {
        chr_8k(mmc1_chr0 >> 1);
    }
}

static void mmc1_apply() {
    static const uint8_t mirroring[4] = { NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW, NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH,
                                          NAMETABLE_MIRRORING_VERTICAL, NAMETABLE_MIRRORING_HORIZONTAL };
    set_ppu_nametable_mirroring(mirroring[mmc1_control & 3]);
    mmc1_apply_prg();
    mmc1_apply_chr();
}

static void mmc1_init() {
    mmc1_shift = mmc1_count = 0;
    mmc1_control = 0x0c;
    mmc1_chr0 = mmc1_chr1 = mmc1_prg = 0;
    mmc1_last_write_cycle = 0xfffffff0;
    mmc1_apply();
}

// (MMC1 is the one board whose handler write_byte_6502_c calls directly, without the devices' catch-up first,
// mapper_write_select: only its reset write and the fifth bit of a register load change anything, so the first four
// bits go in without one. Zelda II's overworld loads the PRG register ~350 times in a frame every ~18 frames)
static __attribute__((noinline)) void mmc1_write(uint16_t address, uint8_t data) {
    uint32_t consecutive = (cpu_cycle_now == mmc1_last_write_cycle + 1);
    mmc1_last_write_cycle = cpu_cycle_now;
    if (consecutive) {
        return;
    }
    if ((data & 0x80) || (mmc1_count == 4)) {
        cpu_sync_devices();
    }
    if (data & 0x80) {
        mmc1_shift = mmc1_count = 0;
        mmc1_control |= 0x0c;
        mmc1_apply();
        return;
    }
    mmc1_shift |= (data & 1) << mmc1_count;
    if (++mmc1_count < 5) {
        return;
    }
    // PRG bank writes don't affect rendering
    if (((address >> 13) & 3) != 3) {
        ppu_sync();
    }
    // only what the register written decides: a PRG bank switch (Zelda II's overworld, ~340 a frame) doesn't map the
    // nametables and CHR again, and a value that doesn't change maps nothing. CHR 0 also picks the outer PRG bank and
    // the PRG RAM bank on the 512 kB boards (SUROM / SXROM)
    uint8_t value = mmc1_shift;
    mmc1_shift = mmc1_count = 0;
    switch ((address >> 13) & 3) {
        case 0:
            if (value != mmc1_control) {
                mmc1_control = value;
                mmc1_apply();
            }
            break;
        case 1:
            if (value != mmc1_chr0) {
                mmc1_chr0 = value;
                if (num_8k_ROM_pages > 32) {
                    mmc1_apply();
                } else {
                    mmc1_apply_chr();
                }
            }
            break;
        case 2:
            if (value != mmc1_chr1) {
                mmc1_chr1 = value;
                mmc1_apply_chr();
            }
            break;
        case 3:
            if (value != mmc1_prg) {
                mmc1_prg = value;
                mmc1_apply_prg();
            }
            break;
    }
}

/**
 * MMC2 (mapper 9, Punch-Out!!) and MMC4 (mapper 10, Fire Emblem): each 4 kB CHR half has two banks, picked by a
 * latch that the PPU sets when it fetches the pattern of tile $FD or $FE (the fetch itself still uses the old bank).
 * MMC2's left latch only sees $0FD8 / $0FE8, its right latch and both of MMC4's see $xFD8-$xFDF / $xFE8-$xFEF.
 * PRG: MMC2 switches 8 kB at $8000 (the last three fixed), MMC4 16 kB at $8000 (the last fixed).
 */
uint8_t mapper_chr_latch = 0;
// any of the PPU fetch hooks in use (mapper_chr_latch, mapper_ext_attributes, mapper_sprite_chr_split)
uint8_t mapper_fetch_hooks = 0;
extern uint8_t mapper_ext_attributes, mapper_sprite_chr_split;

static void update_fetch_hooks() {
    mapper_fetch_hooks = mapper_chr_latch | mapper_ext_attributes | mapper_sprite_chr_split;
}
static uint8_t mmc2_chr[4];       // $B000-$E000: left $FD, left $FE, right $FD, right $FE
static uint8_t mmc2_latch[2];     // 0 = $FD, 1 = $FE

static void mmc2_apply_chr(uint32_t half) {
    chr_4k(half, mmc2_chr[half * 2 + mmc2_latch[half]]);
}

static void mmc2_bus_address(uint16_t address);

static void mmc2_init() {
    memset(mmc2_chr, 0, sizeof(mmc2_chr));
    ppu_bus_address_hook = mmc2_bus_address;
    mmc2_latch[0] = mmc2_latch[1] = 1;
    mapper_chr_latch = (uint8_t)mapper_number;
    if (mapper_number == 9) {
        set_6502_bank_pointer(0, prg_page(0));
        set_6502_bank_pointer(1, prg_page(num_8k_ROM_pages - 3));
        set_6502_bank_pointer(2, prg_page(num_8k_ROM_pages - 2));
        set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
    } else {
        prg_16k(0, 0);
        prg_16k(2, num_8k_ROM_pages / 2 - 1);
    }
    mmc2_apply_chr(0);
    mmc2_apply_chr(1);
}

static FRNES_BOARD_TEXT __attribute__((noinline)) void mmc2_write(uint16_t address, uint8_t data) {
    switch (address & 0xf000) {
        case 0xa000:
            if (mapper_number == 9) {
                set_6502_bank_pointer(0, prg_page(data & 0x0f));
            } else {
                prg_16k(0, data & 0x0f);
            }
            break;
        case 0xb000: case 0xc000: case 0xd000: case 0xe000: {
            uint32_t index = ((address >> 12) & 0x0f) - 0x0b;
            ppu_sync();
            mmc2_chr[index] = data & 0x1f;
            mmc2_apply_chr(index >> 1);
        } break;
        case 0xf000:
            ppu_sync();
            set_ppu_nametable_mirroring((data & 1) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
            break;
    }
}

FRNES_BOARD_TEXT void mapper_latch_read(uint16_t address) {
    uint32_t half = (address >> 12) & 1;
    uint32_t low = address & 0x0ff8;
    if ((mapper_chr_latch == 9) && !half && (address & 7)) {
        return;
    }
    uint8_t latch;
    if (low == 0x0fd8) {
        latch = 0;
    } else if (low == 0x0fe8) {
        latch = 1;
    } else {
        return;
    }
    if (mmc2_latch[half] != latch) {
        mmc2_latch[half] = latch;
        mmc2_apply_chr(half);
    }
}

// $2006 / $2007 accesses put pattern addresses on the bus too: the latches switch on them as on the fetches
FRNES_BOARD_TEXT __attribute__((cold)) static void mmc2_bus_address(uint16_t address) {
    if (address < 0x2000) {
        mapper_latch_read(address);
    }
}

uint16_t mapper_latch_state() {
    return mmc2_latch[0] | (mmc2_latch[1] << 8);
}

void mapper_latch_restore(uint16_t state) {
    if (((state & 0xff) != mmc2_latch[0]) || ((state >> 8) != mmc2_latch[1])) {
        mmc2_latch[0] = state & 0xff;
        mmc2_latch[1] = state >> 8;
        mmc2_apply_chr(0);
        mmc2_apply_chr(1);
    }
}

/**
 * MMC5 (mapper 5, ExROM: Castlevania III, the Koei games)
 *
 * PRG: $5100 mode (32 / 16+16 / 16+8+8 / 8x4 kB), $5113 the RAM bank at $6000, $5114-$5117 the banks at $8000-$FFFF
 * (bit 7 clear maps RAM there, except $5117). CHR: $5101 mode (8 / 4 / 2 / 1 kB), $5120-$5127 and $5128-$512B the
 * two register sets, $5130 their upper bits; the set written last is used for everything (with 8x16 sprites the
 * hardware uses the A set for sprites and the B set for the background, not emulated yet). $5105 maps each nametable
 * slot to CIRAM page 0 / 1, ExRAM (VRAM page 2) or the fill page (VRAM page 3, $5106 tile / $5107 attribute).
 * $5104 ExRAM mode (extended attributes not emulated yet), $5C00-$5FFF ExRAM. $5203 / $5204 the scanline IRQ:
 * clocked at dot 4 of each rendering scanline, scanline 0 starts the frame and later scanlines count up and compare.
 * $5205 / $5206 the multiplier. PRG RAM is 64 kB.
 */
#define MMC5_VISIBLE_SCANLINES 240
#define MMC5_PRERENDER_SCANLINE 261
static uint8_t mmc5_prg_mode, mmc5_chr_mode, mmc5_exram_mode, mmc5_nametables, mmc5_fill_tile, mmc5_fill_attribute;
static uint8_t mmc5_prg[5];          // $5113-$5117
static uint16_t mmc5_chr[12];        // $5120-$512B with the upper bits
static uint8_t mmc5_chr_upper, mmc5_chr_last_b;
static uint8_t mmc5_irq_compare, mmc5_irq_enabled, mmc5_irq_pending, mmc5_in_frame, mmc5_scanline;
static uint8_t mmc5_multiplicand, mmc5_multiplier;
static uint8_t* mmc5_slot_ram[4];    // RAM mapped at $8000-$FFFF slots (NULL when ROM)

uint8_t mapper_ext_attributes = 0;

const uint8_t* mapper_ext_tile(uint32_t index, uint32_t name, uint32_t* palette) {
    uint8_t ext = ppu_vram_page_read(2, index & 0x3ff);
    *palette = ext >> 6;
    uint32_t bank = (ext & 0x3f) | (mmc5_chr_upper << 6);
    return chr_page(bank * 4 + (name >> 6)) + ((name & 0x3f) << 4);
}

static uint8_t* mmc5_ram_page(uint32_t bank) {
    return &WRAM[(bank & 7) * 0x2000];
}

static FRNES_BOARD_TEXT void mmc5_map_slot(uint32_t slot, uint8_t value) {
    if ((slot < 3) && !(value & 0x80)) {
        mmc5_slot_ram[slot] = mmc5_ram_page(value);
        set_6502_bank_pointer(slot, mmc5_slot_ram[slot]);
    } else {
        mmc5_slot_ram[slot] = NULL;
        set_6502_bank_pointer(slot, prg_page(value & 0x7f));
    }
}

static FRNES_BOARD_TEXT void mmc5_apply_prg() {
    set_wram_bank_pointer(mmc5_ram_page(mmc5_prg[0]));
    switch (mmc5_prg_mode & 3) {
        case 0:
            for (uint32_t i = 0; i < 4; i++) {
                mmc5_map_slot(i, (mmc5_prg[4] & 0xfc) | i);
            }
            break;
        case 1:
            mmc5_map_slot(0, (mmc5_prg[2] & 0xfe));
            mmc5_map_slot(1, (mmc5_prg[2] & 0xfe) | 1);
            mmc5_map_slot(2, (mmc5_prg[4] & 0xfe) | 0x80);
            mmc5_map_slot(3, (mmc5_prg[4] & 0xfe) | 1);
            break;
        case 2:
            mmc5_map_slot(0, (mmc5_prg[2] & 0xfe));
            mmc5_map_slot(1, (mmc5_prg[2] & 0xfe) | 1);
            mmc5_map_slot(2, mmc5_prg[3]);
            mmc5_map_slot(3, mmc5_prg[4]);
            break;
        case 3:
            mmc5_map_slot(0, mmc5_prg[1]);
            mmc5_map_slot(1, mmc5_prg[2]);
            mmc5_map_slot(2, mmc5_prg[3]);
            mmc5_map_slot(3, mmc5_prg[4]);
            break;
    }
}

uint8_t mapper_sprite_chr_split = 0;
static uint8_t* mmc5_sprite_pages[8];

// the 1 kB CHR pages set A (sprites) or set B (background) selects in the current CHR mode
static FRNES_BOARD_TEXT void mmc5_chr_pages(int set_b, uint8_t** pages) {
    const uint16_t* r = mmc5_chr;
    for (uint32_t i = 0; i < 8; i++) {
        uint32_t page;
        if (!set_b) {
            switch (mmc5_chr_mode & 3) {
                case 0: page = r[7] * 8 + i; break;
                case 1: page = r[(i < 4) ? 3 : 7] * 4 + (i & 3); break;
                case 2: page = r[(i & 6) + 1] * 2 + (i & 1); break;
                default: page = r[i]; break;
            }
        } else {
            switch (mmc5_chr_mode & 3) {
                case 0: page = r[11] * 8 + i; break;
                case 1: page = r[11] * 4 + (i & 3); break;
                case 2: page = r[9 + (i & 2)] * 2 + (i & 1); break;
                default: page = r[8 + (i & 3)]; break;
            }
        }
        pages[i] = chr_page(page);
    }
}

// With 8x16 sprites set A is used for sprites and set B for the background, otherwise the set written last for both
static FRNES_BOARD_TEXT void mmc5_apply_chr() {
    uint8_t* pages[8];
    int tall = (ppu_registers -> PPUCTRL & PPUCTRL_SPRITE_SIZE) != 0;
    mmc5_chr_pages(tall ? 1 : mmc5_chr_last_b, pages);
    for (uint32_t i = 0; i < 8; i++) {
        set_ppu_pattern_table_bank_pointer(i, pages[i]);
    }
    mapper_sprite_chr_split = tall;
    update_fetch_hooks();
    if (tall) {
        mmc5_chr_pages(0, mmc5_sprite_pages);
    }
}

const uint8_t* mapper_sprite_chr(uint32_t address) {
    return mmc5_sprite_pages[(address >> 10) & 7] + (address & 0x3ff);
}

void mapper_ppuctrl_written(uint8_t old_value, uint8_t new_value) {
    if ((mapper_number == 5) && ((old_value ^ new_value) & PPUCTRL_SPRITE_SIZE)) {
        // (called before PPUCTRL changes: apply with the new value)
        uint8_t saved = ppu_registers -> PPUCTRL;
        ppu_registers -> PPUCTRL = new_value;
        mmc5_apply_chr();
        ppu_registers -> PPUCTRL = saved;
    }
}

static FRNES_BOARD_TEXT void mmc5_apply_nametables() {
    // 0 / 1 CIRAM, 2 ExRAM (VRAM page 2, nametable only in ExRAM modes 0 and 1), 3 fill (VRAM page 3)
    uint8_t pages[4];
    for (uint32_t slot = 0; slot < 4; slot++) {
        pages[slot] = (mmc5_nametables >> (slot * 2)) & 3;
    }
    set_ppu_nametable_pages(pages[0], pages[1], pages[2], pages[3]);
}

static void mmc5_update_irq() {
    mapper_irq_flag = mmc5_irq_pending && mmc5_irq_enabled;
    update_irq_line();
}

static int mmc5_init() {
    apu_synth_set_expansion(APU_EXPANSION_MMC5);
    if (resize_wram(0x10000) != 0) {
        printf(" ...ERROR: no memory for MMC5 PRG RAM\n");
        return -1;
    }
    mmc5_prg_mode = 3;
    mmc5_chr_mode = 3;
    mmc5_exram_mode = 0;
    mapper_ext_attributes = 0;
    mmc5_nametables = 0;
    mmc5_fill_tile = mmc5_fill_attribute = 0;
    memset(mmc5_prg, 0, sizeof(mmc5_prg));
    mmc5_prg[4] = 0xff;
    memset(mmc5_chr, 0, sizeof(mmc5_chr));
    mmc5_chr_upper = 0;
    mmc5_chr_last_b = 0;
    mmc5_irq_compare = mmc5_irq_enabled = mmc5_irq_pending = mmc5_in_frame = mmc5_scanline = 0;
    mmc5_multiplicand = mmc5_multiplier = 0xff;
    for (uint32_t i = 0; i < 0x400; i++) {
        ppu_vram_page_write(2, i, 0);
    }
    ppu_vram_page_fill(3, 0, 0);
    mapper_clock_dot = 4;
    mmc5_apply_prg();
    mmc5_apply_chr();
    mmc5_apply_nametables();
    return 0;
}

static FRNES_BOARD_TEXT FRNES_COLD uint8_t mmc5_read_low(uint16_t address, uint8_t open_bus) {
    if (address == 0x5204) {
        // in-frame only while the PPU is rendering the visible scanlines
        if ((ppu_current_scanline() >= MMC5_VISIBLE_SCANLINES) || !(ppu_registers -> PPUMASK & (PPUMASK_ENABLE_BACKGROUND_RENDERING | PPUMASK_ENABLE_SPRITE_RENDERING))) {
            mmc5_in_frame = 0;
        }
        // (Mesen 2: the flag goes up with the first nametable fetch after the scanline detection, dot 9 of line 0 (a read at dot 9 still sees it down),
        // while the counter here was started at dot 4; no open bus bits)
        uint8_t in_frame = mmc5_in_frame && !((mmc5_scanline == 0) && (ppu_current_scanline() == 0) && (ppu_current_dot() < 10));
        uint8_t value = (mmc5_irq_pending ? 0x80 : 0) | (in_frame ? 0x40 : 0);
        mmc5_irq_pending = 0;
        mmc5_update_irq();
        return value;
    } else if (address == 0x5205) {
        return (uint8_t)(mmc5_multiplicand * mmc5_multiplier);
    } else if (address == 0x5206) {
        return (uint8_t)((mmc5_multiplicand * mmc5_multiplier) >> 8);
    } else if ((address >= 0x5c00) && (mmc5_exram_mode >= 2)) {
        return ppu_vram_page_read(2, address & 0x3ff);
    }
    return open_bus;
}

static FRNES_BOARD_TEXT FRNES_COLD void mmc5_write_low(uint16_t address, uint8_t data) {
    // sound
    if ((address <= 0x5015) && (address >= 0x5000)) {
        apu_synth_log_write(address, data, cpu_cycle_now);
        return;
    }
    if (address >= 0x5c00) {
        // ExRAM: writable as RAM in modes 0-2 (in modes 0 / 1 the PPU sees it, so the scanline is brought up to date)
        if (mmc5_exram_mode != 3) {
            if (mmc5_exram_mode < 2) {
                ppu_sync();
            }
            ppu_vram_page_write(2, address & 0x3ff, data);
        }
        return;
    }
    switch (address) {
        case 0x5100: mmc5_prg_mode = data & 3; mmc5_apply_prg(); break;
        case 0x5101: ppu_sync(); mmc5_chr_mode = data & 3; mmc5_apply_chr(); break;
        case 0x5104:
            ppu_sync();
            mmc5_exram_mode = data & 3;
            mapper_ext_attributes = (mmc5_exram_mode == 1);
            update_fetch_hooks();
            ppu_nametable_generation++;
            break;
        case 0x5105: ppu_sync(); mmc5_nametables = data; mmc5_apply_nametables(); break;
        case 0x5106: ppu_sync(); mmc5_fill_tile = data; ppu_vram_page_fill(3, mmc5_fill_tile, mmc5_fill_attribute); break;
        case 0x5107: ppu_sync(); mmc5_fill_attribute = (data & 3) * 0x55; ppu_vram_page_fill(3, mmc5_fill_tile, mmc5_fill_attribute); break;
        case 0x5113: case 0x5114: case 0x5115: case 0x5116: case 0x5117:
            mmc5_prg[address - 0x5113] = data;
            mmc5_apply_prg();
            break;
        case 0x5120: case 0x5121: case 0x5122: case 0x5123: case 0x5124: case 0x5125: case 0x5126: case 0x5127:
        case 0x5128: case 0x5129: case 0x512a: case 0x512b:
            ppu_sync();
            mmc5_chr[address - 0x5120] = data | (mmc5_chr_upper << 8);
            mmc5_chr_last_b = (address >= 0x5128);
            mmc5_apply_chr();
            break;
        case 0x5130: ppu_sync(); mmc5_chr_upper = data & 3; ppu_nametable_generation++; break;
        case 0x5203: mmc5_irq_compare = data; break;
        case 0x5204:
            mmc5_irq_enabled = data >> 7;
            mapper_irq_armed = mmc5_irq_enabled;
            mmc5_update_irq();
            break;
        case 0x5205: mmc5_multiplicand = data; break;
        case 0x5206: mmc5_multiplier = data; break;
    }
}

static FRNES_BOARD_TEXT __attribute__((noinline)) void mmc5_write(uint16_t address, uint8_t data) {
    // PRG RAM mapped into $8000-$DFFF
    uint8_t* ram = mmc5_slot_ram[(address >> 13) & 3];
    if (ram) {
        ram[address & 0x1fff] = data;
    }
}

FRNES_BOARD_TEXT __attribute__((cold, noinline)) static void mmc5_vector_nmi() {
    mmc5_irq_pending = 0;
    mmc5_scanline = 0;
    mmc5_update_irq();
}

static void mmc5_clock() {
    if (ppu_current_scanline() == MMC5_PRERENDER_SCANLINE) {
        mmc5_in_frame = 0;
        return;
    }
    if (!mmc5_in_frame) {
        mmc5_in_frame = 1;
        mmc5_scanline = 0;
    } else {
        mmc5_scanline++;
        if (mmc5_scanline == mmc5_irq_compare) {
            mmc5_irq_pending = 1;
            mmc5_update_irq();
        }
    }
}

/**
 * Simple discrete boards
 *   11 Color Dreams: $8000-$FFFF bits 0-1 32 kB PRG, bits 4-7 8 kB CHR
 *   66 GxROM: bits 4-5 32 kB PRG, bits 0-1 8 kB CHR
 *   34 BNROM (CHR RAM): 32 kB PRG at $8000-$FFFF; NINA-001 (CHR ROM): $7FFD 32 kB PRG, $7FFE / $7FFF 4 kB CHR
 *   30 UNROM 512: bits 0-4 16 kB PRG at $8000, bits 5-6 8 kB CHR RAM bank, bit 7 one screen nametable
 *   75 VRC1: $8000 / $A000 / $C000 8 kB PRG, $9000 mirroring and the CHR banks' high bits, $E000 / $F000 4 kB CHR
 *   111 GTROM / Cheapocabra (flash not emulated): $5000-$5FFF and $7000-$7FFF bits 0-3 32 kB PRG, bit 4 8 kB CHR RAM
 *       bank, bit 5 four-screen nametable RAM bank (VRAM pages 4-7 / 8-11), bits 6-7 LEDs
 */
uint8_t mapper_6000_mode = 0;
static uint8_t prg_ram_disabled;
// $6000-$7FFF: PRG RAM (mode 0), ROM (writes ignored) or PRG RAM switched off (open bus, writes ignored)
static void prg_ram_access(uint32_t mode) {
    prg_ram_disabled = (mode == 2);
    mapper_6000_mode = mode ? MAPPER_6000_ROM : 0;
    mapper_6000_reads = prg_ram_disabled;
}
uint8_t mapper_chr_ram_slots = 0;
static uint8_t vrc1_chr[2];

// whether the 8 kB CHR bank is already the one mapped: a write that maps it again changes nothing the PPU sees, so it
// needs no catch-up (Castle of Deceit's multiply routine keeps its scratch at $FFDC / $FFDD, ROM: ~55 ROL / ASL a
// frame every 4th frame, each a Color Dreams register write of the same banks mid scanline, each a replay of the
// scanline dot by dot: 19.6 ms of emulation those frames, 5.7 ms the others)
static FRNES_BOARD_TEXT int chr_8k_mapped(uint32_t bank) {
    for (uint32_t i = 0; i < 8; i++) {
        if (ppu_pattern_table_bank_pointer(i) != chr_page(bank * 8 + i)) {
            return 0;
        }
    }
    return 1;
}

static FRNES_BOARD_TEXT __attribute__((noinline)) void discrete_write(uint16_t address, uint8_t data) {
    switch (mapper_number) {
        case 11:
            prg_32k(data & 3);
            if (!chr_8k_mapped(data >> 4)) {
                ppu_sync();
                chr_8k(data >> 4);
            }
            break;
        case 66:
            prg_32k((data >> 4) & 3);
            if (!chr_8k_mapped(data & 3)) {
                ppu_sync();
                chr_8k(data & 3);
            }
            break;
        case 34:
            prg_32k(data);
            break;
        case 30:
            ppu_sync();
            prg_16k(0, data & 0x1f);
            chr_8k((data >> 5) & 3);
            if (NesHeader.byInfo1 & 8) {
                set_ppu_nametable_mirroring((data & 0x80) ? NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH : NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW);
            }
            break;
        case 75:
            switch (address & 0xf000) {
                case 0x8000: set_6502_bank_pointer(0, prg_page(data & 0x0f)); break;
                case 0xa000: set_6502_bank_pointer(1, prg_page(data & 0x0f)); break;
                case 0xc000: set_6502_bank_pointer(2, prg_page(data & 0x0f)); break;
                case 0x9000:
                    ppu_sync();
                    // (not on a four-screen board: the Vs. System's, Vs. The Goonies writing its bit 0 as it scrolls)
                    if (!(NesHeader.byInfo1 & 8)) {
                        set_ppu_nametable_mirroring((data & 1) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
                    }
                    vrc1_chr[0] = (vrc1_chr[0] & 0x0f) | ((data & 2) << 3);
                    vrc1_chr[1] = (vrc1_chr[1] & 0x0f) | ((data & 4) << 2);
                    chr_4k(0, vrc1_chr[0]);
                    chr_4k(1, vrc1_chr[1]);
                    break;
                case 0xe000:
                case 0xf000: {
                    uint32_t half = (address >> 12) & 1;
                    ppu_sync();
                    vrc1_chr[half] = (vrc1_chr[half] & 0x10) | (data & 0x0f);
                    chr_4k(half, vrc1_chr[half]);
                } break;
            }
            break;
    }
}

static FRNES_BOARD_TEXT int chr_8k_mapped(uint32_t bank);
// (the PPU caught up, ppu_sync, only for a write that maps another CHR RAM bank or nametable bank: the 32 kB PRG bank
// changes nothing it sees, and the devices were caught up before the write, cpu_sync_devices_io. The Storied Sword
// switches PRG banks dozens of times a frame mid scanline: each a replay of the line dot by dot before)
static FRNES_BOARD_TEXT void gtrom_write(uint8_t data) {
    uint32_t nt = (data & 0x20) ? 8 : 4;
    if (!chr_8k_mapped((data >> 4) & 1) || (ppu_nametable_slot_page(0) != nt) || (ppu_nametable_slot_page(1) != nt + 1) ||
        (ppu_nametable_slot_page(2) != nt + 2) || (ppu_nametable_slot_page(3) != nt + 3)) {
        ppu_sync();
    }
    prg_32k(data & 0x0f);
    chr_8k((data >> 4) & 1);
    set_ppu_nametable_pages(nt, nt + 1, nt + 2, nt + 3);
}

static uint8_t n163_write_protect;

// VRC2 boards without PRG RAM: $6000-$7FFF is the microwire latch (bit 0, the rest open bus), as Mesen 2 has it
static uint8_t vrc2_latch_mode, vrc2_latch;

void mapper_write_6000(uint16_t address, uint8_t data) {
    if (__builtin_expect(board_6000, 0)) {
        board_write_6000(address, data);
        return;
    }
    if (vrc2_latch_mode) {
        vrc2_latch = data & 1;
        return;
    }
    if (mapper_number == 16) {
        if (bandai_submapper != 5) {
            bandai_write(address, data);
        }
        return;
    }
    if (mapper_number == 159) {
        return;
    }
    if (mmc6) {
        mmc6_write(address, data);
        return;
    }
    if (mapper_number == 19) {
        // Namco 163: $F800 bit 6 enables writes, bits 0-3 protect the four 2 kB quarters
        if ((n163_write_protect & 0x40) && !(n163_write_protect & (1 << ((address >> 11) & 3)))) {
            NES_WINDOW_BYTE(address) = data;
        }
        return;
    }
    if (mapper_number == 111) {
        if (address >= 0x7000) {
            gtrom_write(data);
        }
        return;
    }
    // NINA-001 (mapper 34 with CHR ROM); the registers are also PRG RAM
    if (WRAM != NULL) {
        NES_WINDOW_BYTE(address) = data;
    }
    if (mapper_number == 34) {
        switch (address) {
            case 0x7ffd: prg_32k(data & 1); break;
            case 0x7ffe: ppu_sync(); chr_4k(0, data & 0x0f); break;
            case 0x7fff: ppu_sync(); chr_4k(1, data & 0x0f); break;
        }
    }
}

static void discrete_init() {
    vrc1_chr[0] = vrc1_chr[1] = 0;
    switch (mapper_number) {
        case 11: case 66: case 34:
            prg_32k(0);
            chr_8k(0);
            if ((mapper_number == 34) && num_1k_VROM_pages) {
                resize_wram(0x2000);
                mapper_6000_mode = MAPPER_6000_REGISTERS;
            }
            break;
        case 30:
            prg_16k(0, 0);
            prg_16k(2, num_8k_ROM_pages / 2 - 1);
            break;
        case 75:
            set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
            break;
        case 111:
            // no PRG RAM: $6000-$7FFF are the register's mirrors
            mapper_6000_mode = MAPPER_6000_REGISTERS;
            gtrom_write(0);
            break;
    }
}

/**
 * Sunsoft FME-7 (mapper 69, Gimmick!, Batman: Return of the Joker): a command register at $8000 and its parameter at
 * $A000. Commands 0-7 1 kB CHR banks, 8 the $6000 bank (bit 6 RAM / ROM, bit 7 RAM enabled), 9-B 8 kB PRG banks at
 * $8000-$DFFF ($E000 is the last bank), C mirroring, D IRQ control (bit 0 IRQ enabled, bit 7 counter running;
 * writing it acknowledges the IRQ), E / F the 16 bit counter, decremented every CPU cycle: going from 0 to $FFFF
 * raises the IRQ. Sunsoft 5B audio ($C000 / $E000) is not emulated yet.
 */
uint8_t mapper_cpu_timer = 0;
static uint8_t fme7_command, fme7_irq_control;
static uint16_t fme7_counter;
// the CPU cycle the counter was last brought up to
static uint32_t mapper_timer_cycle;

// (mapper_boards.c's counters)
void mapper_timer_start(uint32_t cycle) {
    mapper_timer_cycle = cycle;
}

static void fme7_update_timer() {
    if (!mapper_cpu_timer) {
        mapper_timer_cycle = cpu_cycle_now;
    }
    mapper_cpu_timer = (fme7_irq_control & 0x80) != 0;
}

static void fme7_init() {
    apu_synth_set_expansion(APU_EXPANSION_5B);
    // (the $6000 window can hold RAM or ROM, so it needs a mapping either way)
    resize_wram(0x2000);
    fme7_command = 0;
    fme7_irq_control = 0;
    fme7_counter = 0;
    fme7_update_timer();
    for (uint32_t i = 0; i < 3; i++) {
        set_6502_bank_pointer(i, prg_page(i));
    }
    set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
}

static FRNES_BOARD_TEXT FRNES_COLD void fme7_write(uint16_t address, uint8_t data) {
    switch (address & 0xe000) {
        // Sunsoft 5B sound
        case 0xc000:
        case 0xe000:
            apu_synth_log_write(address & 0xe000, data, cpu_cycle_now);
            break;
        case 0x8000:
            fme7_command = data & 0x0f;
            break;
        case 0xa000:
            if (mapper_cpu_timer) {
                mapper_cpu_run(cpu_cycle_now);
            }
            switch (fme7_command) {
                case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
                    ppu_sync();
                    set_ppu_pattern_table_bank_pointer(fme7_command, chr_page(data));
                    break;
                case 8:
                    // bit 6 RAM (bit 7 enables it: open bus otherwise) or a ROM bank (writes ignored)
                    if (data & 0x40) {
                        set_wram_bank_pointer(WRAM);
                        prg_ram_access((data & 0x80) ? 0 : 2);
                    } else {
                        set_wram_bank_pointer(prg_page(data & 0x3f));
                        prg_ram_access(1);
                    }
                    break;
                case 9: case 10: case 11:
                    set_6502_bank_pointer(fme7_command - 9, prg_page(data & 0x3f));
                    break;
                case 12: {
                    static const uint8_t modes[4] = { NAMETABLE_MIRRORING_VERTICAL, NAMETABLE_MIRRORING_HORIZONTAL,
                                                      NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW, NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH };
                    ppu_sync();
                    set_ppu_nametable_mirroring(modes[data & 3]);
                } break;
                case 13:
                    fme7_irq_control = data;
                    mapper_irq_flag = 0;
                    update_irq_line();
                    fme7_update_timer();
                    cpu_cut_batch_for_event(mapper_cycles_to_event());
                    break;
                case 14:
                    fme7_counter = (fme7_counter & 0xff00) | data;
                    cpu_cut_batch_for_event(mapper_cycles_to_event());
                    break;
                case 15:
                    fme7_counter = (fme7_counter & 0x00ff) | (data << 8);
                    cpu_cut_batch_for_event(mapper_cycles_to_event());
                    break;
            }
            break;
    }
}

static void fme7_cpu_run(uint32_t cycles) {
    if (cycles > fme7_counter) {
        // it went from 0 to $FFFF on the way
        if (fme7_irq_control & 1) {
            mapper_irq_flag = 1;
            update_irq_line();
        }
    }
    fme7_counter = (uint16_t)(fme7_counter - cycles);
}

/**
 * Konami VRC3 (mapper 73, Salamander): $F000 16 kB PRG at $8000; $8000-$B000 the IRQ latch nibbles, $C000 control
 * (bit 0 A, bit 1 E, bit 2 8 bit mode), $D000 acknowledge. The counter counts CPU cycles up and raises the IRQ when it
 * overflows ($FFFF, or $FF in 8 bit mode), reloading from the latch.
 * Sunsoft-3 (mapper 67, Fantasy Zone II): $8800 / $9800 / $A800 / $B800 2 kB CHR banks, $C800 the IRQ counter (high
 * then low byte), $D800 bit 4 counting, $E800 mirroring, $F800 16 kB PRG at $8000. The counter counts CPU cycles down,
 * going from 0 to $FFFF raises the IRQ and stops it.
 */
static uint16_t vrc3_latch, vrc3_counter;
static uint8_t vrc3_control, sunsoft3_toggle;
// (RAMBO-1, further down)
static void rambo_cpu_run(uint32_t cycles);
// RAMBO-1's IRQ goes up 5 dots after its scanline clock's dot (Mesen 2 counts 2 CPU clocks from the cycle A12 rose
// in, which lands part way into the second; whole cycles were a wait loop pass late in Klax) and 1 CPU cycle after a
// CPU cycle mode clock: a cycle stamped ahead of the catch-up, which the poll before each instruction's last cycle holds
// off until then (cpu_irq_raise_at; cut independent, the CPU_CUT_EVERY builds give the same frames)
#define RAMBO_IRQ_DELAY_SCANLINE_DOTS 5
#define RAMBO_IRQ_DELAY_CPU 1
static uint32_t rambo_cycles_to_event();
static void n163_cpu_run(uint32_t cycles);
static uint16_t n163_counter;

static void counter_timer_start() {
    if (!mapper_cpu_timer) {
        mapper_timer_cycle = cpu_cycle_now;
    }
}

static FRNES_BOARD_TEXT FRNES_COLD void vrc3_write(uint16_t address, uint8_t data) {
    if (mapper_cpu_timer) {
        mapper_cpu_run(cpu_cycle_now);
    }
    switch (address & 0xf000) {
        case 0x8000: case 0x9000: case 0xa000: case 0xb000: {
            uint32_t shift = ((address >> 12) - 8) * 4;
            vrc3_latch = (vrc3_latch & ~(0x0f << shift)) | ((data & 0x0f) << shift);
        } break;
        case 0xc000:
            counter_timer_start();
            vrc3_control = data & 7;
            if (data & 2) {
                vrc3_counter = vrc3_latch;
            }
            mapper_irq_flag = 0;
            update_irq_line();
            mapper_cpu_timer = (vrc3_control & 2) != 0;
            break;
        case 0xd000:
            counter_timer_start();
            mapper_irq_flag = 0;
            update_irq_line();
            vrc3_control = (vrc3_control & ~2) | ((vrc3_control & 1) << 1);
            mapper_cpu_timer = (vrc3_control & 2) != 0;
            break;
        case 0xf000:
            prg_16k(0, data & 7);
            break;
    }
    cpu_cut_batch_for_event(mapper_cycles_to_event());
}

static void vrc3_run(uint32_t cycles) {
    // past the top the counter goes on from the latch: (top - latch) cycles a wrap (tests/closed_form_test.c checks
    // this against the wrap by wrap loop)
    if (vrc3_control & 4) {
        // 8 bit mode: only the low byte counts
        uint32_t low = (vrc3_counter & 0xff) + cycles, latch = vrc3_latch & 0xff;
        if (low > 0xff) {
            low = latch + (low - 0x100) % (0x100 - latch);
            mapper_irq_flag = 1;
        }
        vrc3_counter = (vrc3_counter & 0xff00) | low;
    } else {
        uint32_t value = vrc3_counter + cycles;
        if (value > 0xffff) {
            value = vrc3_latch + (value - 0x10000) % (0x10000 - vrc3_latch);
            mapper_irq_flag = 1;
        }
        vrc3_counter = value;
    }
    update_irq_line();
}

static uint32_t vrc3_cycles_to_event() {
    return (vrc3_control & 4) ? (0x100 - (vrc3_counter & 0xff)) : (0x10000 - vrc3_counter);
}

static FRNES_BOARD_TEXT __attribute__((noinline)) void sunsoft3_write(uint16_t address, uint8_t data) {
    if (mapper_cpu_timer) {
        mapper_cpu_run(cpu_cycle_now);
    }
    switch (address & 0xf800) {
        case 0x8800: case 0x9800: case 0xa800: case 0xb800: {
            uint32_t slot = ((address >> 12) - 8) * 2;
            ppu_sync();
            set_ppu_pattern_table_bank_pointer(slot, chr_page(data * 2));
            set_ppu_pattern_table_bank_pointer(slot + 1, chr_page(data * 2 + 1));
        } break;
        case 0xc800:
            if (sunsoft3_toggle) {
                vrc3_counter = (vrc3_counter & 0xff00) | data;
            } else {
                vrc3_counter = (vrc3_counter & 0x00ff) | (data << 8);
            }
            sunsoft3_toggle ^= 1;
            break;
        case 0xd800:
            counter_timer_start();
            sunsoft3_toggle = 0;
            vrc3_control = data & 0x10;
            mapper_irq_flag = 0;
            update_irq_line();
            mapper_cpu_timer = vrc3_control != 0;
            break;
        case 0xe800: {
            static const uint8_t modes[4] = { NAMETABLE_MIRRORING_VERTICAL, NAMETABLE_MIRRORING_HORIZONTAL,
                                              NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW, NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH };
            ppu_sync();
            set_ppu_nametable_mirroring(modes[data & 3]);
        } break;
        case 0xf800:
            prg_16k(0, data & 0x0f);
            break;
    }
    cpu_cut_batch_for_event(mapper_cycles_to_event());
}

static void sunsoft3_run(uint32_t cycles) {
    if (cycles > vrc3_counter) {
        vrc3_counter = 0xffff;
        vrc3_control = 0;
        mapper_cpu_timer = 0;
        mapper_irq_flag = 1;
        update_irq_line();
    } else {
        vrc3_counter -= cycles;
    }
}

/**
 * The VRC IRQ (VRC4, VRC6, VRC7): an 8 bit counter counting up from the latch to $FF, then reloading and raising the
 * IRQ. In cycle mode (M) it counts CPU cycles, in scanline mode a prescaler counts 341 PPU dots (3 per CPU cycle).
 * Control: bit 0 A (E after an acknowledge), bit 1 E (counting), bit 2 M; writing it with E set reloads the counter
 * and the prescaler. Acknowledging clears the IRQ and copies A into E.
 */
static struct {
    uint8_t latch, counter, control;
    int32_t prescaler;
} vrc_irq;
static uint8_t vrc_irq_used;

static void vrc_irq_update_timer() {
    if (!mapper_cpu_timer) {
        mapper_timer_cycle = cpu_cycle_now;
    }
    mapper_cpu_timer = (vrc_irq.control & 2) != 0;
}

static void vrc_irq_write_control(uint8_t data) {
    if (mapper_cpu_timer) {
        mapper_cpu_run(cpu_cycle_now);
    }
    vrc_irq.control = data & 7;
    if (data & 2) {
        vrc_irq.counter = vrc_irq.latch;
        vrc_irq.prescaler = 341;
    }
    mapper_irq_flag = 0;
    update_irq_line();
    vrc_irq_update_timer();
    cpu_cut_batch_for_event(mapper_cycles_to_event());
}

static void vrc_irq_acknowledge() {
    if (mapper_cpu_timer) {
        mapper_cpu_run(cpu_cycle_now);
    }
    mapper_irq_flag = 0;
    update_irq_line();
    vrc_irq.control = (vrc_irq.control & ~2) | ((vrc_irq.control & 1) << 1);
    vrc_irq_update_timer();
    cpu_cut_batch_for_event(mapper_cycles_to_event());
}

static void vrc_irq_clock(uint32_t clocks) {
    while (clocks) {
        uint32_t to_ff = 0xff - vrc_irq.counter;
        if (clocks <= to_ff) {
            vrc_irq.counter += clocks;
            return;
        }
        clocks -= to_ff + 1;
        vrc_irq.counter = vrc_irq.latch;
        mapper_irq_flag = 1;
        update_irq_line();
    }
}

static void vrc_irq_run(uint32_t cycles) {
    if (vrc_irq.control & 4) {
        vrc_irq_clock(cycles);
        return;
    }
    int32_t p = vrc_irq.prescaler - 3 * (int32_t)cycles;
    uint32_t clocks = 0;
    if (p <= 0) {
        clocks = 1 + (uint32_t)(-p) / 341;
        p += 341 * clocks;
    }
    vrc_irq.prescaler = p;
    vrc_irq_clock(clocks);
}

static uint32_t vrc_irq_cycles_to_event() {
    uint32_t clocks = 0x100 - vrc_irq.counter;
    if (vrc_irq.control & 4) {
        return clocks;
    }
    int32_t p = vrc_irq.prescaler;
    if (p > 0) {
        // after its first clock the prescaler is always back in 339-341, so the cycles to the clocks-th clock add up
        // to ceil((p + 341 * (clocks - 1)) / 3): the same as the loop below, without up to 256 steps (with a division
        // each) every time the next event is looked up, which during DMC sample playback was many times a frame
        return ((uint32_t)p + 341 * (clocks - 1) + 2) / 3;
    }
    uint32_t cycles = 0;
    while (clocks--) {
        uint32_t t = (uint32_t)(p + 2) / 3;
        if (t == 0) {
            t = 1;
        }
        cycles += t;
        p = p - 3 * (int32_t)t + 341;
    }
    return cycles;
}

static uint32_t mapper_cycles_to_fire();
// (the TC0690's IRQ delay, further down)
static void tc0690_cpu_run(uint32_t cycles);
static uint8_t tc0690_delay;

FRNES_HOT(timer_cpu_run) void mapper_cpu_run(uint32_t now) {
    uint32_t start = mapper_timer_cycle;
    uint32_t cycles = now - start;
    if (!cycles) {
        return;
    }
    // the IRQ a counter raises in these cycles goes up at the cycle it fires at (cpu_irq_raise_at), not at the
    // catch-up's end
    uint8_t had_irq = mapper_irq_flag & CPU_IRQ_MAPPER;
    uint32_t to_fire = (had_irq || board_own_irqs) ? 0xffffffff : mapper_cycles_to_fire();
    mapper_timer_cycle = now;
    // (the boards' counters first, as in mapper_cycles_to_event)
    if (board_timer) {
        board_cpu_run(cycles);
    } else if (mapper_number == 69) {
        fme7_cpu_run(cycles);
    } else if (vrc_irq_used) {
        vrc_irq_run(cycles);
    } else if (mapper_number == 73) {
        vrc3_run(cycles);
    } else if (mapper_number == 67) {
        sunsoft3_run(cycles);
    } else if (mapper_number == 64) {
        rambo_cpu_run(cycles);
    } else if (mapper_number == 19) {
        n163_cpu_run(cycles);
    } else if ((mapper_number == 16) || (mapper_number == 153) || (mapper_number == 159)) {
        bandai_cpu_run(cycles);
    } else if (mapper_number == 48) {
        tc0690_cpu_run(cycles);
    }
    if (!had_irq && (mapper_irq_flag & CPU_IRQ_MAPPER) && (to_fire <= cycles)) {
        mapper_irq_flag &= ~CPU_IRQ_MAPPER;
        cpu_irq_update_line();
        cpu_irq_raise_at(CPU_IRQ_MAPPER, start + to_fire + ((mapper_number == 64) ? RAMBO_IRQ_DELAY_CPU : 0));
    }
}

static uint32_t mmc3_clocks_to_irq();

// mapper clocks (scanlines) until an armed scanline IRQ can be raised
uint32_t mapper_clocks_to_irq() {
    if ((mapper_number == 4) || (mapper_number == 118) || (mapper_number == 119)) {
        return mmc3_clocks_to_irq();
    }
    return 1;
}

// CPU cycles from mapper_timer_cycle until a CPU cycle counter raises its IRQ (cpu_irq_raise_at gets the cycle that
// many after mapper_timer_cycle, as Mesen 2 times it), 0xffffffff if it won't
static uint32_t mapper_cycles_to_fire() {
    uint32_t cycles = 0xffffffff;
    if ((mapper_number == 69) && (fme7_irq_control & 1)) {
        cycles = (uint32_t)fme7_counter + 1;
    } else if (vrc_irq_used && (vrc_irq.control & 2)) {
        cycles = vrc_irq_cycles_to_event();
    } else if ((mapper_number == 73) && (vrc3_control & 2)) {
        cycles = vrc3_cycles_to_event();
    } else if ((mapper_number == 67) && vrc3_control) {
        cycles = (uint32_t)vrc3_counter + 1;
    } else if ((mapper_number == 19) && mapper_cpu_timer) {
        cycles = 0x7fff - (n163_counter & 0x7fff);
    } else if (mapper_number == 64) {
        cycles = rambo_cycles_to_event();
    } else if (((mapper_number == 16) || (mapper_number == 153) || (mapper_number == 159)) && bandai_irq_enabled) {
        cycles = (uint32_t)bandai_counter + 1;
    } else if ((mapper_number == 48) && mapper_cpu_timer) {
        cycles = tc0690_delay;
    } else if (board_timer) {
        cycles = board_cycles_to_fire();
    }
    return cycles;
}

FRNES_HOT(timer_cycles_to_event) uint32_t mapper_cycles_to_event() {
    // from the CPU's current time (writes happen mid batch)
    uint32_t elapsed = cpu_cycle_now - mapper_timer_cycle;
    uint32_t cycles = 0xffffffff;
    // (the boards' counters first: the FDS asks every batch while a disk loads; none of the mappers below has one)
    if (board_timer) {
        cycles = board_cycles_to_fire();
        if (cycles == 0xffffffff) {
            return cycles;
        }
    } else if ((mapper_number == 69) && (fme7_irq_control & 1)) {
        cycles = (uint32_t)fme7_counter + 1;
    } else if (vrc_irq_used && (vrc_irq.control & 2)) {
        cycles = vrc_irq_cycles_to_event();
    } else if ((mapper_number == 73) && (vrc3_control & 2)) {
        cycles = vrc3_cycles_to_event();
    } else if ((mapper_number == 67) && vrc3_control) {
        cycles = (uint32_t)vrc3_counter + 1;
    } else if ((mapper_number == 19) && mapper_cpu_timer) {
        cycles = 0x7fff - (n163_counter & 0x7fff);
    } else if (mapper_number == 64) {
        cycles = rambo_cycles_to_event();
        if (cycles == 0xffffffff) {
            return cycles;
        }
    } else if (((mapper_number == 16) || (mapper_number == 153) || (mapper_number == 159)) && bandai_irq_enabled) {
        cycles = (uint32_t)bandai_counter + 1;
    } else if ((mapper_number == 48) && mapper_cpu_timer) {
        cycles = tc0690_delay;
    } else {
        return cycles;
    }
    return (cycles > elapsed) ? cycles - elapsed : 1;
}

/**
 * Konami VRC2 / VRC4 (mappers 21, 22, 23, 25): the boards wire different CPU address lines to the chip's two
 * register select inputs, so each mapper number has its own pair (VRC2a / mapper 22 also drops the low bit of its CHR
 * banks). $8000 PRG bank at $8000 (or $C000 with VRC4's swap mode, $9002 bit 1), $A000 PRG bank at $A000, the second
 * last bank at $C000 (or $8000) and the last at $E000; $9000 mirroring; $B000-$E003 eight 1 kB CHR banks as low / high
 * nibbles; $F000-$F003 VRC4's IRQ (latch low / high nibble, control, acknowledge).
 */
static uint8_t vrc4_prg[2], vrc4_swap;
static uint16_t vrc4_chr[8];

static uint32_t vrc4_register(uint16_t address) {
    uint32_t a0, a1;
    switch (mapper_number) {
        case 21: a0 = (address >> 1) | (address >> 6); a1 = (address >> 2) | (address >> 7); break;
        case 22: a0 = address >> 1; a1 = address; break;
        case 23: a0 = address | (address >> 2); a1 = (address >> 1) | (address >> 3); break;
        default: a0 = (address >> 1) | (address >> 3); a1 = address | (address >> 2); break;
    }
    return (address & 0xf000) | ((a1 & 1) << 1) | (a0 & 1);
}

static void vrc4_apply_prg() {
    uint32_t second_last = num_8k_ROM_pages - 2;
    set_6502_bank_pointer(vrc4_swap ? 2 : 0, prg_page(vrc4_prg[0]));
    set_6502_bank_pointer(vrc4_swap ? 0 : 2, prg_page(second_last));
    set_6502_bank_pointer(1, prg_page(vrc4_prg[1]));
    set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
}

static void vrc4_init() {
    vrc2_latch_mode = (WRAM == NULL) && ((mapper_number == 22) || (mapper_number == 23) || (mapper_number == 25));
    vrc2_latch = 0;
    if (vrc2_latch_mode) {
        mapper_6000_mode = MAPPER_6000_REGISTERS;
        mapper_6000_reads = 1;
    }
    vrc4_prg[0] = 0;
    vrc4_prg[1] = 1;
    vrc4_swap = 0;
    memset(vrc4_chr, 0, sizeof(vrc4_chr));
    memset(&vrc_irq, 0, sizeof(vrc_irq));
    vrc_irq_used = 1;
    vrc4_apply_prg();
}

static __attribute__((noinline)) void vrc4_write(uint16_t address, uint8_t data) {
    uint32_t reg = vrc4_register(address);
    switch (reg & 0xf000) {
        case 0x8000:
            vrc4_prg[0] = data & 0x1f;
            vrc4_apply_prg();
            break;
        case 0x9000:
            if ((reg & 3) == 0 || (mapper_number == 22)) {
                static const uint8_t modes[4] = { NAMETABLE_MIRRORING_VERTICAL, NAMETABLE_MIRRORING_HORIZONTAL,
                                                  NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW, NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH };
                ppu_sync();
                set_ppu_nametable_mirroring(modes[data & ((mapper_number == 22) ? 1 : 3)]);
            } else if ((reg & 3) == 2) {
                vrc4_swap = (data >> 1) & 1;
                vrc4_apply_prg();
            }
            break;
        case 0xa000:
            vrc4_prg[1] = data & 0x1f;
            vrc4_apply_prg();
            break;
        case 0xb000: case 0xc000: case 0xd000: case 0xe000: {
            uint32_t bank = (((reg >> 12) - 0xb) << 1) | ((reg >> 1) & 1);
            ppu_sync();
            if (reg & 1) {
                vrc4_chr[bank] = (vrc4_chr[bank] & 0x0f) | ((data & 0x1f) << 4);
            } else {
                vrc4_chr[bank] = (vrc4_chr[bank] & 0x1f0) | (data & 0x0f);
            }
            set_ppu_pattern_table_bank_pointer(bank, chr_page((mapper_number == 22) ? (vrc4_chr[bank] >> 1) : vrc4_chr[bank]));
        } break;
        case 0xf000:
            switch (reg & 3) {
                case 0: vrc_irq.latch = (vrc_irq.latch & 0xf0) | (data & 0x0f); break;
                case 1: vrc_irq.latch = (vrc_irq.latch & 0x0f) | (data << 4); break;
                case 2: vrc_irq_write_control(data); break;
                case 3: vrc_irq_acknowledge(); break;
            }
            break;
    }
}

/**
 * Konami VRC6 (mappers 24, 26: 26 swaps A0 / A1): $8000 16 kB PRG at $8000, $C000 8 kB at $C000, the last 8 kB at
 * $E000; $B003 bits 2-3 mirroring; $D000-$E003 eight 1 kB CHR banks; $F000 IRQ latch, $F001 control, $F002
 * acknowledge. $9000-$B002 are the expansion sound channels (not emulated yet).
 */
static void vrc6_init() {
    apu_synth_set_expansion(APU_EXPANSION_VRC6);
    memset(&vrc_irq, 0, sizeof(vrc_irq));
    vrc_irq_used = 1;
    // (power on: banks 0-2 and the last, as Mesen 2 has them; Esper Dream 2 reads the ID byte of the $C000 bank before
    // writing $C000 and restores it later)
    prg_16k(0, 0);
    set_6502_bank_pointer(2, prg_page(2));
    set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
}

static __attribute__((noinline)) void vrc6_write(uint16_t address, uint8_t data) {
    uint32_t reg = (address & 0xf000) | ((mapper_number == 26) ? (((address & 1) << 1) | ((address >> 1) & 1)) : (address & 3));
    switch (reg) {
        case 0x9000: case 0x9001: case 0x9002: case 0x9003:
        case 0xa000: case 0xa001: case 0xa002:
        case 0xb000: case 0xb001: case 0xb002:
            apu_synth_log_write(reg, data, cpu_cycle_now);
            break;
        case 0x8000: case 0x8001: case 0x8002: case 0x8003:
            prg_16k(0, data & 0x0f);
            break;
        case 0xc000: case 0xc001: case 0xc002: case 0xc003:
            set_6502_bank_pointer(2, prg_page(data & 0x1f));
            break;
        case 0xb003: {
            static const uint8_t modes[4] = { NAMETABLE_MIRRORING_VERTICAL, NAMETABLE_MIRRORING_HORIZONTAL,
                                              NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW, NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH };
            ppu_sync();
            set_ppu_nametable_mirroring(modes[(data >> 2) & 3]);
        } break;
        case 0xd000: case 0xd001: case 0xd002: case 0xd003:
        case 0xe000: case 0xe001: case 0xe002: case 0xe003:
            ppu_sync();
            set_ppu_pattern_table_bank_pointer(((reg >> 12) - 0xd) * 4 + (reg & 3), chr_page(data));
            break;
        case 0xf000: vrc_irq.latch = data; break;
        case 0xf001: vrc_irq_write_control(data); break;
        case 0xf002: vrc_irq_acknowledge(); break;
    }
}

/**
 * Konami VRC7 (mapper 85, Lagrange Point): $8000 / $8010 (or $8008) / $9000 the PRG banks at $8000 / $A000 / $C000,
 * the last bank at $E000; $A000-$D010 eight 1 kB CHR banks; $E000 bits 0-1 mirroring; $E010 IRQ latch, $F000
 * control, $F010 acknowledge. The FM sound ($9010 / $9030) is not emulated yet.
 */
static void vrc7_init() {
    apu_synth_set_expansion(APU_EXPANSION_VRC7);
    memset(&vrc_irq, 0, sizeof(vrc_irq));
    vrc_irq_used = 1;
    set_6502_bank_pointer(0, prg_page(0));
    set_6502_bank_pointer(1, prg_page(1));
    set_6502_bank_pointer(2, prg_page(2));
    set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
}

static FRNES_COLD void vrc7_write(uint16_t address, uint8_t data) {
    // the boards use A4 or A3 for the second register of each pair
    uint32_t reg = (address & 0xf000) | ((address & 0x18) ? 0x10 : 0);
    switch (reg) {
        case 0x8000: set_6502_bank_pointer(0, prg_page(data & 0x3f)); break;
        case 0x8010: set_6502_bank_pointer(1, prg_page(data & 0x3f)); break;
        case 0x9000: set_6502_bank_pointer(2, prg_page(data & 0x3f)); break;
        case 0x9010:
            // sound: $9010 register select, $9030 data (A5 tells them apart)
            apu_synth_log_write((address & 0x20) ? 0x9030 : 0x9010, data, cpu_cycle_now);
            break;
        case 0xa000: case 0xa010: case 0xb000: case 0xb010: case 0xc000: case 0xc010: case 0xd000: case 0xd010:
            ppu_sync();
            set_ppu_pattern_table_bank_pointer((((reg >> 12) - 0xa) << 1) | ((reg >> 4) & 1), chr_page(data));
            break;
        case 0xe000: {
            static const uint8_t modes[4] = { NAMETABLE_MIRRORING_VERTICAL, NAMETABLE_MIRRORING_HORIZONTAL,
                                              NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW, NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH };
            ppu_sync();
            set_ppu_nametable_mirroring(modes[data & 3]);
        } break;
        case 0xe010: vrc_irq.latch = data; break;
        case 0xf000: vrc_irq_write_control(data); break;
        case 0xf010: vrc_irq_acknowledge(); break;
    }
}

/**
 * MMC3 (mapper 4)
 */
static uint8_t mmc3_bank_select;
static uint8_t mmc3_registers[8];
static uint8_t mmc3_irq_latch;
static uint8_t mmc3_irq_counter;
static uint8_t mmc3_irq_reload;
static uint8_t mmc3_irq_enabled;
static uint8_t mmc3_mirroring = 0xff;

// (MMC3's per-scanline path in the hot group: the most common mapper, its clock runs every rendered scanline)
FRNES_HOT(5_mmc3_apply_prg) static void mmc3_apply_prg() {
    uint32_t second_last = num_8k_ROM_pages - 2;
    if (mmc3_bank_select & 0x40) {
        set_6502_bank_pointer(0, prg_page(second_last));
        set_6502_bank_pointer(2, prg_page(mmc3_registers[6]));
    } else {
        set_6502_bank_pointer(0, prg_page(mmc3_registers[6]));
        set_6502_bank_pointer(2, prg_page(second_last));
    }
    set_6502_bank_pointer(1, prg_page(mmc3_registers[7]));
    set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
}

// TQROM (mapper 119): CHR bank values with bit 6 set select the 8 kB of CHR RAM instead of CHR ROM
static void mmc3_set_chr(uint32_t slot, uint32_t value) {
    if ((mapper_number == 119) && (value & 0x40)) {
        mapper_chr_ram_slots |= 1 << slot;
        set_ppu_pattern_table_bank_pointer(slot, VRAM_pages[value & 7]);
    } else {
        mapper_chr_ram_slots &= ~(1 << slot);
        set_ppu_pattern_table_bank_pointer(slot, chr_page(value));
    }
}

/**
 * TxSROM (mapper 118: Alien Syndrome, Armadillo, Pro Sport Hockey): an MMC3 whose CHR bank registers' bit 7 drives
 * CIRAM A10, so the banks covering $0000-$0FFF pick each nametable: R0 for $2000 / $2400 and R1 for $2800 / $2C00,
 * or R2-R5 one each with the CHR halves swapped. $A000 does nothing.
 */
__attribute__((noinline)) static void mmc3_txsrom_nametables() {
    const uint8_t* r = mmc3_registers;
    if (mmc3_bank_select & 0x80) {
        set_ppu_nametable_pages(r[2] >> 7, r[3] >> 7, r[4] >> 7, r[5] >> 7);
    } else {
        set_ppu_nametable_pages(r[0] >> 7, r[0] >> 7, r[1] >> 7, r[1] >> 7);
    }
}

FRNES_HOT(5_mmc3_apply_chr) static void mmc3_apply_chr() {
    // R0 / R1 are 2 kB banks, R2-R5 1 kB; with bit 7 of bank select the two halves swap
    uint32_t flip = (mmc3_bank_select & 0x80) ? 4 : 0;
    mmc3_set_chr(0 ^ flip, mmc3_registers[0] & 0xfe);
    mmc3_set_chr(1 ^ flip, mmc3_registers[0] | 0x01);
    mmc3_set_chr(2 ^ flip, mmc3_registers[1] & 0xfe);
    mmc3_set_chr(3 ^ flip, mmc3_registers[1] | 0x01);
    mmc3_set_chr(4 ^ flip, mmc3_registers[2]);
    mmc3_set_chr(5 ^ flip, mmc3_registers[3]);
    mmc3_set_chr(6 ^ flip, mmc3_registers[4]);
    mmc3_set_chr(7 ^ flip, mmc3_registers[5]);
    if (mapper_number == 118) {
        mmc3_txsrom_nametables();
    }
}

static uint32_t mmc3_clocks_to_irq();

static void mmc3_update_armed() {
    mapper_irq_armed = mmc3_irq_enabled;
    // a write mid batch (enable, latch, reload) can bring the IRQ before the end the batch was planned with
    if (mapper_irq_armed && (ppu_registers -> PPUMASK & (PPUMASK_ENABLE_BACKGROUND_RENDERING | PPUMASK_ENABLE_SPRITE_RENDERING))) {
        cpu_cut_batch_for_event(ppu_cycles_to_mapper_clock(mmc3_clocks_to_irq()));
    }
}

// mapper clocks until the scanline IRQ is raised: a counter at 0 (or told to reload) reloads from the latch and
// raises the IRQ when the latch is 0, otherwise it counts down to 0
static uint32_t mmc3_clocks_to_irq() {
    if ((mmc3_irq_counter == 0) || mmc3_irq_reload) {
        return mmc3_irq_latch ? (uint32_t)mmc3_irq_latch + 1 : 1;
    }
    return mmc3_irq_counter;
}

/**
 * MMC6 (mapper 4 submapper 1: StarTropics, Zoda's Revenge): an MMC3 with 1 kB of RAM inside the chip at $7000-$7FFF
 * (mirrored every 1 kB) instead of PRG RAM at $6000. $8000 bit 5 enables it; $A001 bits 4 / 5 enable writing /
 * reading its first 512 bytes, bits 6 / 7 the second. Per nesdev, with only one half readable the other reads as 0,
 * with neither $7000-$7FFF is open bus, as $6000-$6FFF always is. The CPU reads the window through the MMU, so the
 * window (WRAM, 8 kB) holds what each read sees: open bus values (the high byte of the address, the last value on the
 * bus for an absolute read) at $6000-$6FFF, and the four 1 kB mirrors of the RAM as the enables show it at
 * $7000-$7FFF. The RAM itself (mmc6_ram) is what the battery keeps; writes go through mapper_write_6000.
 */
static uint8_t mmc6 = 0;
static uint8_t mmc6_a001 = 0;
static uint8_t mmc6_ram[0x400];

static int mmc6_detect() {
    // NES 2.0 submapper 1, or the two games by CRC (their iNES 1 headers say mapper 4)
    if (cartridge_submapper == 1) {
        return 1;
    }
    return (currentCRC32 == 0x889129cb) || (currentCRC32 == 0xd054ffb0);
}

static uint8_t mmc6_view(uint32_t offset) {
    // offset: $7000-$7FFF relative
    uint32_t enabled = mmc3_bank_select & 0x20;
    uint32_t read_low = enabled && (mmc6_a001 & 0x20), read_high = enabled && (mmc6_a001 & 0x80);
    if (!read_low && !read_high) {
        return 0x70 | (offset >> 8);
    }
    uint32_t high = offset & 0x200;
    if (high ? read_high : read_low) {
        return mmc6_ram[offset & 0x3ff];
    }
    return 0;
}

static void mmc6_apply() {
    for (uint32_t offset = 0; offset < 0x1000; offset++) {
        NES_WINDOW_BYTE(0x7000 + offset) = mmc6_view(offset);
    }
}

static void mmc6_write(uint16_t address, uint8_t data) {
    if ((address < 0x7000) || !(mmc3_bank_select & 0x20)) {
        return;
    }
    if (!(mmc6_a001 & ((address & 0x200) ? 0x40 : 0x10))) {
        return;
    }
    mmc6_ram[address & 0x3ff] = data;
    if (mmc6_a001 & ((address & 0x200) ? 0x80 : 0x20)) {
        for (uint32_t mirror = 0; mirror < 0x1000; mirror += 0x400) {
            NES_WINDOW_BYTE(0x7000 + mirror + (address & 0x3ff)) = data;
        }
    }
}

static void mmc6_init() {
    resize_wram(0x2000);
    set_wram_bank_pointer(WRAM);
    mapper_6000_mode = MAPPER_6000_REGISTERS;
    mmc6_a001 = 0;
    for (uint32_t offset = 0; offset < 0x1000; offset++) {
        NES_WINDOW_BYTE(0x6000 + offset) = 0x60 | (offset >> 8);
    }
    mmc6_apply();
}

/**
 * A12 outside rendering: with rendering off the only addresses on the PPU bus are the CPU's $2006 / $2007 accesses
 * (ppu_bus_address_hook), and A12 rising there clocks the counter like the pattern fetches do, once A12 has been low
 * for a few CPU cycles (the MMC3 filters its input with M2). While rendering, the counter is clocked at the
 * scheduled dot of each line instead (mapper_clock_dot), so the hook ignores those accesses.
 */
#define MMC3_A12_LOW_CYCLES 3
static uint8_t mmc3_a12;
static uint32_t mmc3_a12_low_cycle;
static void mmc3_clock(uint32_t scanline, uint32_t dot);
static void mmc3_update_armed();

FRNES_HOT(5_mmc3_bus_address) static void mmc3_bus_address(uint16_t address) {
    uint32_t scanline = ppu_current_scanline();
    if ((ppu_registers -> PPUMASK & (PPUMASK_ENABLE_BACKGROUND_RENDERING | PPUMASK_ENABLE_SPRITE_RENDERING)) &&
        ((scanline < 240) || (scanline == 261))) {
        return;
    }
    uint8_t a12 = (address >> 12) & 1;
    if (a12 && !mmc3_a12) {
        if ((uint32_t)(cpu_cycle_now - mmc3_a12_low_cycle) >= MMC3_A12_LOW_CYCLES) {
            mmc3_clock(scanline, ppu_current_dot());
            if (mapper_irq_flag) {
                cpu_cut_batch_for_event(0);
            }
            // the counter moved outside the PPU's schedule: the batch may have been planned for a later IRQ
            mmc3_update_armed();
        }
    } else if (!a12 && mmc3_a12) {
        mmc3_a12_low_cycle = cpu_cycle_now;
    }
    mmc3_a12 = a12;
}

static void mmc3_init() {
    mmc3_bank_select = 0;
    static const uint8_t initial[8] = { 0, 2, 4, 5, 6, 7, 0, 1 };
    memcpy(mmc3_registers, initial, sizeof(mmc3_registers));
    mmc3_irq_latch = mmc3_irq_counter = 0;
    mmc3_irq_reload = 0;
    mmc3_irq_enabled = 0;
    mmc3_mirroring = 0xff;
    mapper_clock_dot = 260;
    mmc3_a12 = 0;
    mmc3_a12_low_cycle = 0;
    ppu_bus_address_hook = mmc3_bus_address;
    mmc3_apply_prg();
    mmc3_apply_chr();
    mmc3_update_armed();
}

// the MMC3 outer bank boards (mapper_boards.c: 47, 37) start as the MMC3 from board_init (out of the link time
// optimized code: a call from mmc3_init moved the hot mapper_ppu_clock's LTO register allocation)
FRNES_COLD_TEXT void mmc3_board_init() {
    mapper_number = 4;
    mmc3_init();
}

// the MMC3 outer bank boards (mapper_boards.c: 47, 37) changed the page tables: the banks again from them
FRNES_COLD_TEXT void mmc3_reapply() {
    ppu_sync();
    mmc3_apply_prg();
    mmc3_apply_chr();
}

static __attribute__((noinline)) void mmc3_write(uint16_t address, uint8_t data) {
    switch (address & 0xe001) {
        case 0x8000: {
            uint8_t changed = mmc3_bank_select ^ data;
            if (changed & 0x80) {
                // CHR mode change, the scanline being rendered has to be brought up to date first
                ppu_sync();
            }
            mmc3_bank_select = data;
            if (changed & 0x40) {
                mmc3_apply_prg();
            }
            if (mmc6 && (changed & 0x20)) {
                mmc6_apply();
            }
            if (changed & 0x80) {
                mmc3_apply_chr();
            }
        } break;
        case 0x8001: {
            uint32_t target = mmc3_bank_select & 7;
            if (mmc3_registers[target] == data) {
                break;
            }
            if (target >= 6) {
                // PRG banks don't affect rendering
                mmc3_registers[target] = data;
                mmc3_apply_prg();
            } else {
                ppu_sync();
                mmc3_registers[target] = data;
                mmc3_apply_chr();
            }
        } break;
        case 0xa000:
            if (!(NesHeader.byInfo1 & 8) && (mapper_number != 118) && ((data & 1) != mmc3_mirroring)) {
                ppu_sync();
                mmc3_mirroring = data & 1;
                set_ppu_nametable_mirroring((data & 1) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
            }
            break;
        case 0xa001:
            // PRG RAM enable / write protect, not emulated (MMC6: its RAM's enables)
            if (mmc6 && (data != mmc6_a001)) {
                mmc6_a001 = data;
                mmc6_apply();
            }
            break;
        case 0xc000:
            mmc3_irq_latch = data;
            mmc3_update_armed();
            break;
        case 0xc001:
            mmc3_irq_counter = 0;
            mmc3_irq_reload = 1;
            mmc3_update_armed();
            break;
        case 0xe000:
            mmc3_irq_enabled = 0;
            cpu_irq_clear(CPU_IRQ_MAPPER);
            mmc3_update_armed();
            break;
        case 0xe001:
            mmc3_irq_enabled = 1;
            mmc3_update_armed();
            break;
    }
}
// (mapper_mmc3_sync.c, built without LTO, reads the registers and calls mmc3_write by these names: aliases, so
// mapper.c's own code is compiled as before; the host, without aliases, compiles it here)
#ifdef FRNES_HOST
#define mmc3_sync_bank_select mmc3_bank_select
#define mmc3_sync_registers mmc3_registers
#define mmc3_sync_mirroring mmc3_mirroring
#define mmc3_sync_write mmc3_write
#include "mapper_mmc3_sync.c"
#else
extern uint8_t mmc3_sync_bank_select __attribute__((alias("mmc3_bank_select")));
extern uint8_t mmc3_sync_registers[8] __attribute__((alias("mmc3_registers")));
extern uint8_t mmc3_sync_mirroring __attribute__((alias("mmc3_mirroring")));
void mmc3_sync_write(uint16_t address, uint8_t data) __attribute__((alias("mmc3_write")));
#endif

// a clock from the PPU's fetches, at the dot A12 rose (ppu_mapper_clock_a12_dot: 261, 325, or 5 on the pre-render
// line when only the background uses $1000). That pre-render rise follows vblank, where the bus shows v: if a $2006 /
// $2007 access left A12 high there (mmc3_bus_address), it isn't a rise and doesn't clock the counter. The rendering
// fetches leave the bus on the nametables (A12 low) when vblank starts.
static void mmc3_clock(uint32_t scanline, uint32_t dot);
static void mmc3_ppu_clock() {
    uint32_t scanline = ppu_current_scanline();
    if ((ppu_mapper_clock_a12_dot == 5) && mmc3_a12) {
        return;
    }
    mmc3_clock(scanline, ppu_mapper_clock_a12_dot);
    if (ppu_mapper_clock_a12_dot != 5) {
        mmc3_a12 = 0;
    }
}

// a clock at that scanline / dot: the IRQ goes up during the CPU cycle the CPU sees that dot in
static void mmc3_clock(uint32_t scanline, uint32_t dot) {
    if ((mmc3_irq_counter == 0) || mmc3_irq_reload) {
        mmc3_irq_counter = mmc3_irq_latch;
        mmc3_irq_reload = 0;
    } else {
        mmc3_irq_counter--;
    }
    if ((mmc3_irq_counter == 0) && mmc3_irq_enabled) {
        cpu_irq_raise_at(CPU_IRQ_MAPPER, ppu_dot_cpu_cycle(scanline, dot));
    }
}

/**
 * The Taito TC0690's scanline IRQ (mapper 48, its banks are in mapper_boards.c): an MMC3-like counter clocked by A12
 * whose IRQ comes up 22 CPU cycles after the clock (6 on The Jetsons' board), as Mesen 2 times it. It stays in the
 * link time optimized unit because mapper_ppu_clock calls it: a call out of the unit there makes the compiler assume
 * every global may change, and the PPU code it is inlined into reloaded its state after each mapper clock (PinBot
 * and MM3 3% slower).
 */
static uint8_t tc0690_delay, tc0690_latch, tc0690_counter, tc0690_reload, tc0690_enabled, tc0690_a12, tc0690_jetsons;
static uint32_t tc0690_a12_low_cycle;
// (as the MMC3's: A12 has to have been low for a few CPU cycles)
#define TC0690_A12_LOW_CYCLES 3

// as mmc3_update_armed (mapper_clocks_to_irq gives it 1: a batch ends at each of its clocks while armed)
static void tc0690_update_armed() {
    mapper_irq_armed = tc0690_enabled;
    if (mapper_irq_armed && (ppu_registers -> PPUMASK & (PPUMASK_ENABLE_BACKGROUND_RENDERING | PPUMASK_ENABLE_SPRITE_RENDERING))) {
        cpu_cut_batch_for_event(ppu_cycles_to_mapper_clock(1));
    }
}

static void tc0690_cpu_run(uint32_t cycles);

// mapper_cpu_run for the TC0690 alone: nothing reached from mapper_ppu_clock may call out of the unit (see above),
// and mapper_cpu_run calls mapper_boards.c's counters
static void tc0690_catch_up(uint32_t now) {
    uint32_t start = mapper_timer_cycle;
    uint32_t cycles = now - start;
    if (!cycles) {
        return;
    }
    uint8_t had_irq = mapper_irq_flag & CPU_IRQ_MAPPER;
    uint32_t to_fire = had_irq ? 0xffffffff : tc0690_delay;
    mapper_timer_cycle = now;
    tc0690_cpu_run(cycles);
    if (!had_irq && (mapper_irq_flag & CPU_IRQ_MAPPER) && (to_fire <= cycles)) {
        mapper_irq_flag &= ~CPU_IRQ_MAPPER;
        cpu_irq_update_line();
        cpu_irq_raise_at(CPU_IRQ_MAPPER, start + to_fire);
    }
}

// the counter reached 0 at that CPU cycle: the IRQ comes up tc0690_delay cycles later (the counter timer's cycle
// being the clock's)
FRNES_BOARD_TEXT __attribute__((noinline)) static void tc0690_trigger(uint32_t cycle) {
    if (mapper_cpu_timer) {
        tc0690_catch_up(cpu_cycle_now);
    }
    tc0690_delay = tc0690_jetsons ? 6 : 22;
    mapper_timer_cycle = ((int32_t)(cpu_cycle_now - cycle) > 0) ? cycle : cpu_cycle_now;
    mapper_cpu_timer = 1;
    tc0690_catch_up(cpu_cycle_now);
    if (mapper_cpu_timer) {
        cpu_cut_batch_for_event(tc0690_delay);
    }
}

// the counter clock, A12 hook and PPU clock, as mmc3_clock, mmc3_bus_address and mmc3_ppu_clock, with the late IRQ
FRNES_BOARD_TEXT __attribute__((noinline)) static void tc0690_clock(uint32_t scanline, uint32_t dot) {
    if ((tc0690_counter == 0) || tc0690_reload) {
        tc0690_counter = tc0690_latch;
        tc0690_reload = 0;
    } else {
        tc0690_counter--;
    }
    if ((tc0690_counter == 0) && tc0690_enabled) {
        tc0690_trigger(ppu_dot_cpu_cycle(scanline, dot));
    }
}

static FRNES_BOARD_TEXT void tc0690_bus_address(uint16_t address) {
    uint32_t scanline = ppu_current_scanline();
    if ((ppu_registers -> PPUMASK & (PPUMASK_ENABLE_BACKGROUND_RENDERING | PPUMASK_ENABLE_SPRITE_RENDERING)) &&
        ((scanline < 240) || (scanline == 261))) {
        return;
    }
    uint8_t a12 = (address >> 12) & 1;
    if (a12 && !tc0690_a12) {
        if ((uint32_t)(cpu_cycle_now - tc0690_a12_low_cycle) >= TC0690_A12_LOW_CYCLES) {
            tc0690_clock(scanline, ppu_current_dot());
            tc0690_update_armed();
        }
    } else if (!a12 && tc0690_a12) {
        tc0690_a12_low_cycle = cpu_cycle_now;
    }
    tc0690_a12 = a12;
}

static void tc0690_ppu_clock() {
    if ((ppu_mapper_clock_a12_dot == 5) && tc0690_a12) {
        return;
    }
    tc0690_clock(ppu_current_scanline(), ppu_mapper_clock_a12_dot);
    if (ppu_mapper_clock_a12_dot != 5) {
        tc0690_a12 = 0;
    }
}

static void tc0690_cpu_run(uint32_t cycles) {
    if (cycles >= tc0690_delay) {
        tc0690_delay = 0;
        mapper_cpu_timer = 0;
        mapper_irq_flag |= CPU_IRQ_MAPPER;
        cpu_irq_update_line();
    } else {
        tc0690_delay -= cycles;
    }
}

// mapper_boards.c's register writes ($C000-$C003) and init
FRNES_BOARD_TEXT void tc0690_irq_write(uint32_t reg, uint8_t data) {
    switch (reg) {
        case 0:
            cpu_irq_clear(CPU_IRQ_MAPPER);
            tc0690_latch = (data ^ 0xff) + tc0690_jetsons;
            break;
        case 1:
            cpu_irq_clear(CPU_IRQ_MAPPER);
            tc0690_counter = 0;
            tc0690_reload = 1;
            break;
        case 2:
            tc0690_enabled = 1;
            break;
        case 3:
            tc0690_enabled = 0;
            cpu_irq_clear(CPU_IRQ_MAPPER);
            break;
    }
    tc0690_update_armed();
}

FRNES_BOARD_TEXT void tc0690_irq_init(uint8_t jetsons) {
    // (A12 clocks from the PPU at the MMC3's dot, and from $2006 / $2007 accesses outside rendering)
    tc0690_latch = tc0690_counter = tc0690_reload = tc0690_enabled = tc0690_a12 = tc0690_delay = 0;
    tc0690_jetsons = jetsons;
    tc0690_a12_low_cycle = 0;
    mapper_clock_dot = 260;
    ppu_bus_address_hook = tc0690_bus_address;
    tc0690_update_armed();
}

/**
 * Tengen RAMBO-1 (mapper 64: Klax, Shinobi, Skull & Crossbones): MMC3-like. $8000 bank select (bits 0-3 the register:
 * 0-5 and 8-9 CHR, 6-7 and F PRG; bit 5 1 kB CHR mode for R0 / R1 with R8 / R9; bit 6 PRG mode; bit 7 CHR A12
 * inversion), $8001 bank data, $A000 mirroring, $C000 IRQ latch, $C001 IRQ mode (bit 0: count CPU cycles / 4 instead of
 * scanlines) and reload, $E000 IRQ off and acknowledge, $E001 IRQ on. The counter reloads with latch + 1 (+ 2 after a
 * $C001 write when the latch is over 1) and raises the IRQ when it reaches 0.
 */
static uint8_t rambo_select, rambo_registers[16], rambo_latch, rambo_counter, rambo_reload, rambo_cycle_mode, rambo_enabled;
static uint8_t rambo_prescaler;

/*
 * Tengen 800037 (mapper 158: Alien Syndrome, unlicensed): a RAMBO-1 whose CHR bank registers' bit 7 drives CIRAM A10,
 * as TxSROM's (mmc3_txsrom_nametables): R0 for $2000 / $2400 and R1 for $2800 / $2C00, or R2-R5 one each with the CHR
 * halves swapped, and $A000 does nothing (Mesen 2's Rambo1_158). It runs as mapper 64 (board_override sets
 * cartridge_mapper_number 158 picks its write handler), so the hot dispatch is the RAMBO-1's
 */

static __attribute__((cold, noinline)) void rambo_800037_nametables();

// the CHR banks (R0-R5, R8 / R9 in 1 kB mode) and the PRG banks (R6, R7, RF) apart, as the MMC3's: a write maps
// again only what it changed (Alien Syndrome writes them ~36 times a frame)
static FRNES_BOARD_TEXT void rambo_apply_chr() {
    uint32_t flip = (rambo_select & 0x80) ? 4 : 0;
    const uint8_t* r = rambo_registers;
    if (rambo_select & 0x20) {
        set_ppu_pattern_table_bank_pointer(0 ^ flip, chr_page(r[0]));
        set_ppu_pattern_table_bank_pointer(1 ^ flip, chr_page(r[8]));
        set_ppu_pattern_table_bank_pointer(2 ^ flip, chr_page(r[1]));
        set_ppu_pattern_table_bank_pointer(3 ^ flip, chr_page(r[9]));
    } else {
        set_ppu_pattern_table_bank_pointer(0 ^ flip, chr_page(r[0] & 0xfe));
        set_ppu_pattern_table_bank_pointer(1 ^ flip, chr_page(r[0] | 1));
        set_ppu_pattern_table_bank_pointer(2 ^ flip, chr_page(r[1] & 0xfe));
        set_ppu_pattern_table_bank_pointer(3 ^ flip, chr_page(r[1] | 1));
    }
    for (uint32_t i = 0; i < 4; i++) {
        set_ppu_pattern_table_bank_pointer((4 + i) ^ flip, chr_page(r[2 + i]));
    }
}

static FRNES_BOARD_TEXT void rambo_apply_prg() {
    const uint8_t* r = rambo_registers;
    if (rambo_select & 0x40) {
        set_6502_bank_pointer(0, prg_page(r[15]));
        set_6502_bank_pointer(1, prg_page(r[6]));
        set_6502_bank_pointer(2, prg_page(r[7]));
    } else {
        set_6502_bank_pointer(0, prg_page(r[6]));
        set_6502_bank_pointer(1, prg_page(r[7]));
        set_6502_bank_pointer(2, prg_page(r[15]));
    }
    set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
}

static FRNES_BOARD_TEXT void rambo_apply() {
    rambo_apply_chr();
    rambo_apply_prg();
}

static inline __attribute__((always_inline)) void rambo_clock_counter_body() {
    if (rambo_reload) {
        rambo_counter = (rambo_latch <= 1) ? rambo_latch + 1 : rambo_latch + 2;
        rambo_reload = 0;
    } else if (rambo_counter == 0) {
        rambo_counter = rambo_latch + 1;
    }
    rambo_counter--;
    if ((rambo_counter == 0) && rambo_enabled) {
        mapper_irq_flag = 1;
        update_irq_line();
    }
}

static void rambo_clock_counter() {
    rambo_clock_counter_body();
}

// (a copy of its own for rambo_bus_address and rambo_write_c001: as rambo_clock_counter's third caller it changed how LTO inlined it into
// the hot mapper_ppu_clock and mapper_cpu_run, and MM3 ran ~1.5% slower in the 6 layout A/B)
static FRNES_BOARD_TEXT __attribute__((cold, noinline)) void rambo_clock_counter_bus() {
    rambo_clock_counter_body();
}

static void rambo_update_timer() {
    if (rambo_cycle_mode) {
        if (!mapper_cpu_timer) {
            mapper_timer_cycle = cpu_cycle_now;
        }
        mapper_cpu_timer = 1;
        mapper_irq_armed = 0;
    } else {
        mapper_cpu_timer = 0;
        mapper_irq_armed = rambo_enabled;
    }
}

/*
 * A12 outside rendering (as mmc3_bus_address): the CPU's $2006 / $2007 accesses put their address on the PPU bus, and
 * A12 rising there after more than 30 dots low (Mesen 2's A12 watcher for RAMBO-1) clocks the counter like a scanline:
 * Xybots reloads in vblank then points $2006 at $3F00, a clock Mesen counts (its IRQs came a line late without it)
 */
#define RAMBO_A12_LOW_CYCLES 10
// (Mesen 2's 2 CPU clocks after the rise, counted from the access's own cycle)
#define RAMBO_IRQ_DELAY_CPU_ACCESS 2
static uint8_t rambo_a12;
static uint32_t rambo_a12_low_cycle;

static FRNES_BOARD_TEXT void rambo_bus_address(uint16_t address) {
    uint32_t scanline = ppu_current_scanline();
    if ((ppu_registers -> PPUMASK & (PPUMASK_ENABLE_BACKGROUND_RENDERING | PPUMASK_ENABLE_SPRITE_RENDERING)) &&
        ((scanline < 240) || (scanline == 261))) {
        return;
    }
    uint8_t a12 = (address >> 12) & 1;
    if (a12 && !rambo_a12) {
        if (!rambo_cycle_mode && ((uint32_t)(cpu_cycle_now - rambo_a12_low_cycle) > RAMBO_A12_LOW_CYCLES)) {
            uint8_t had_irq = mapper_irq_flag & CPU_IRQ_MAPPER;
            rambo_clock_counter_bus();
            if (!had_irq && (mapper_irq_flag & CPU_IRQ_MAPPER)) {
                mapper_irq_flag &= ~CPU_IRQ_MAPPER;
                cpu_irq_update_line();
                cpu_irq_raise_at(CPU_IRQ_MAPPER, cpu_cycle_now + RAMBO_IRQ_DELAY_CPU_ACCESS);
                cpu_cut_batch_for_event(0);
            }
        }
    } else if (!a12 && rambo_a12) {
        rambo_a12_low_cycle = cpu_cycle_now;
    }
    rambo_a12 = a12;
}

static void rambo_init() {
    rambo_select = 0;
    rambo_a12 = 0;
    rambo_a12_low_cycle = 0;
    ppu_bus_address_hook = rambo_bus_address;
    static const uint8_t initial[16] = { 0, 2, 4, 5, 6, 7, 0, 1, 1, 3, 0, 0, 0, 0, 0, 0xfe };
    memcpy(rambo_registers, initial, sizeof(rambo_registers));
    rambo_latch = rambo_counter = rambo_reload = rambo_cycle_mode = rambo_enabled = rambo_prescaler = 0;
    // (5 dots after the MMC3's 260: the RAMBO-1's IRQ "is delayed until M2 falls twice after the PPU A12 rise that
    // would have triggered the MMC3 interrupt", nesdev's RAMBO-1 page. At 260 Klax's space station, a $2006 split
    // every 8 lines from its IRQ, jittered a line: the IRQ taken an instruction early or late on some lines, Matt; at
    // 265 every IRQ of a frame lands on the instruction Mesen 2 takes it on)
    mapper_clock_dot = 265;
    // (the mirroring last written to $A000: the MMC3's cache, unused while a RAMBO-1 runs, so no new variable)
    mmc3_mirroring = 0xff;
    rambo_apply();
    rambo_update_timer();
}

static FRNES_BOARD_TEXT __attribute__((cold, noinline)) void rambo_800037_nametables() {
    const uint8_t* r = rambo_registers;
    if (rambo_select & 0x80) {
        set_ppu_nametable_pages(r[2] >> 7, r[3] >> 7, r[4] >> 7, r[5] >> 7);
    } else {
        set_ppu_nametable_pages(r[0] >> 7, r[0] >> 7, r[1] >> 7, r[1] >> 7);
    }
}

/*
 * $C001: cycle mode (bit 0) or the A12 scanline counter, and a reload. The prescaler restarts when cycle mode is
 * selected; leaving cycle mode gives the counter one more clock (Mesen 2's _forceClock, after FHorse: the switch takes
 * more than 4 CPU cycles, so the reload still gets a clock: Skull & Crossbones), here at the write rather than at the
 * prescaler's next wrap (at most 3 CPU cycles early). Cold and out of line, with rambo_clock_counter_bus and
 * rambo_update_timer written out: RAMBO-1 is rare, and code or callers added to the shared hot paths (the mapper write,
 * the CPU timer, mapper_ppu_clock's inlining) cost every game
 */
static FRNES_BOARD_TEXT __attribute__((cold, noinline)) void rambo_write_c001(uint8_t data) {
    uint8_t leaving = rambo_cycle_mode && !(data & 1);
    rambo_cycle_mode = data & 1;
    rambo_prescaler = 0;
    rambo_reload = 1;
    if (leaving) {
        rambo_clock_counter_bus();
    }
    if (rambo_cycle_mode) {
        if (!mapper_cpu_timer) {
            mapper_timer_cycle = cpu_cycle_now;
        }
        mapper_cpu_timer = 1;
        mapper_irq_armed = 0;
    } else {
        mapper_cpu_timer = 0;
        mapper_irq_armed = rambo_enabled;
    }
}

static FRNES_BOARD_TEXT __attribute__((noinline)) void rambo_write(uint16_t address, uint8_t data) {
    if (mapper_cpu_timer) {
        mapper_cpu_run(cpu_cycle_now);
    }
    switch (address & 0xe001) {
        case 0x8000: {
            // (bit 7 the CHR halves, bit 5 1 kB mode: CHR; bit 6 the PRG mode)
            uint8_t changed = rambo_select ^ data;
            if (changed & 0xa0) {
                ppu_sync();
            }
            rambo_select = data;
            if (changed & 0xa0) {
                rambo_apply_chr();
            }
            if (changed & 0x40) {
                rambo_apply_prg();
            }
        } break;
        case 0x8001: {
            uint32_t target = rambo_select & 0x0f;
            if (rambo_registers[target] == data) {
                break;
            }
            if ((target == 6) || (target == 7) || (target == 15)) {
                // PRG banks don't affect rendering
                rambo_registers[target] = data;
                rambo_apply_prg();
            } else if ((target < 6) || ((target < 10) && (rambo_select & 0x20))) {
                ppu_sync();
                rambo_registers[target] = data;
                rambo_apply_chr();
            } else {
                // (R8 / R9 outside 1 kB mode, RA-RE: kept, nothing mapped from them now)
                rambo_registers[target] = data;
            }
        } break;
        case 0xa000:
            // (Tengen 800037's $A000 does nothing: its nametables follow the CHR banks)
            if (!(NesHeader.byInfo1 & 8) && (cartridge_mapper_number != 158) && ((data & 1) != mmc3_mirroring)) {
                ppu_sync();
                mmc3_mirroring = data & 1;
                set_ppu_nametable_mirroring((data & 1) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
            }
            break;
        case 0xc000:
            rambo_latch = data;
            break;
        case 0xc001:
            rambo_write_c001(data);
            break;
        case 0xe000:
            rambo_enabled = 0;
            mapper_irq_flag = 0;
            update_irq_line();
            rambo_update_timer();
            break;
        case 0xe001:
            rambo_enabled = 1;
            rambo_update_timer();
            break;
    }
    cpu_cut_batch_for_event(mapper_cycles_to_event());
}

// Tengen 800037's board handler (mapper_write_select picks it for mapper 158): the RAMBO-1's write, then the
// nametables from the CHR banks again when it changed what they follow: the select's bit 7 (which registers) or
// bit 7 of a CHR bank register R0-R5. Cold: only that board runs it
FRNES_COLD_TEXT void rambo_800037_write(uint16_t address, uint8_t data) {
    uint8_t select = rambo_select;
    uint32_t target = select & 0x0f;
    uint8_t value = rambo_registers[target];
    rambo_write(address, data);
    uint32_t reg = address & 0xe001;
    if (((reg == 0x8000) && ((select ^ rambo_select) & 0x80)) ||
        ((reg == 0x8001) && (target < 6) && ((value ^ rambo_registers[target]) & 0x80))) {
        rambo_800037_nametables();
    }
}

static void rambo_cpu_run(uint32_t cycles) {
    // the counter is clocked every 4 CPU cycles: the first clock as rambo_clock_counter (it may reload), then it counts
    // down to 0 and goes on from latch + 1 (tests/closed_form_test.c checks this against the clock by clock loop)
    uint32_t total = rambo_prescaler + cycles;
    rambo_prescaler = total & 3;
    uint32_t clocks = total >> 2;
    if (!clocks) {
        return;
    }
    rambo_clock_counter();
    clocks--;
    uint32_t counter = rambo_counter;
    if (clocks < counter) {
        rambo_counter = counter - clocks;
        return;
    }
    clocks -= counter;
    uint32_t period = (uint32_t)rambo_latch + 1, rest = clocks % period;
    rambo_counter = rest ? period - rest : 0;
    if ((counter || (clocks >= period)) && rambo_enabled) {
        mapper_irq_flag = 1;
        update_irq_line();
    }
}

static uint32_t rambo_cycles_to_event() {
    if (!rambo_enabled) {
        return 0xffffffff;
    }
    // clocks until the counter reaches 0
    uint32_t clocks = rambo_reload ? ((rambo_latch <= 1) ? rambo_latch + 1 : rambo_latch + 2) : (rambo_counter ? rambo_counter : rambo_latch + 1);
    return clocks * 4 - rambo_prescaler;
}

/**
 * Sunsoft-4 (mapper 68: After Burner, Maharaja): $8000 / $9000 / $A000 / $B000 2 kB CHR banks, $C000 / $D000 the two
 * 1 kB CHR ROM pages (bank | $80) the nametables can come from, $E000 mirroring (bits 0-1) and bit 4: nametables from
 * CHR ROM, $F000 16 kB PRG at $8000 (bit 4: PRG RAM enabled).
 */
static uint8_t sunsoft4_nt[2], sunsoft4_control;

static FRNES_BOARD_TEXT void sunsoft4_apply_nametables() {
    static const uint8_t layouts[4][4] = { { 0, 1, 0, 1 }, { 0, 0, 1, 1 }, { 0, 0, 0, 0 }, { 1, 1, 1, 1 } };
    const uint8_t* layout = layouts[sunsoft4_control & 3];
    if (sunsoft4_control & 0x10) {
        uint8_t* pages[4];
        for (uint32_t slot = 0; slot < 4; slot++) {
            pages[slot] = VROM_pages[((sunsoft4_nt[layout[slot]] | 0x80) % num_1k_VROM_pages)];
        }
        set_ppu_nametable_rom(pages, 0x0f, layout);
    } else {
        set_ppu_nametable_pages(layout[0], layout[1], layout[2], layout[3]);
    }
}

static void sunsoft4_init() {
    sunsoft4_nt[0] = sunsoft4_nt[1] = 0;
    sunsoft4_control = 0;
    prg_16k(0, 0);
    prg_16k(2, num_8k_ROM_pages / 2 - 1);
}

static FRNES_BOARD_TEXT __attribute__((noinline)) void sunsoft4_write(uint16_t address, uint8_t data) {
    switch (address & 0xf000) {
        case 0x8000: case 0x9000: case 0xa000: case 0xb000: {
            uint32_t slot = ((address >> 12) - 8) * 2;
            ppu_sync();
            set_ppu_pattern_table_bank_pointer(slot, chr_page(data * 2));
            set_ppu_pattern_table_bank_pointer(slot + 1, chr_page(data * 2 + 1));
        } break;
        case 0xc000: case 0xd000:
            ppu_sync();
            sunsoft4_nt[(address >> 12) & 1] = data & 0x7f;
            sunsoft4_apply_nametables();
            break;
        case 0xe000:
            ppu_sync();
            sunsoft4_control = data;
            sunsoft4_apply_nametables();
            break;
        case 0xf000:
            prg_16k(0, data & 0x0f);
            break;
    }
}

/**
 * Namco 163 (mapper 19, Megami Tensei II, King of Kings, Rolling Thunder): $E000 / $E800 / $F000 8 kB PRG banks at
 * $8000-$DFFF ($E000 is the last bank); $8000-$B800 eight 1 kB CHR banks; $C000-$D800 the four nametable slots, a
 * CHR ROM page or, for values $E0 and up, CIRAM page (value & 1). $E000 bit 6 disables the sound, $E800 bits 6 / 7
 * stop CHR banks $E0+ from selecting CIRAM for $0000-$0FFF / $1000-$1FFF (CIRAM as pattern tables is not emulated:
 * those banks select CHR ROM). The 15 bit IRQ counter ($5000 low, $5800 bits 0-6 high, bit 7 enabled; both
 * readable, writing either acknowledges the IRQ) counts CPU cycles up while enabled and stops at $7FFF, raising the
 * IRQ there. $F800 bits 0-6 address the chip's 128 bytes of sound RAM, bit 7 auto-increments the address after each
 * $4800 access (the data port, readable); its bit 6 enables PRG RAM writes ($6000-$7FFF), bits 0-3 protect each
 * 2 kB quarter.
 */
static uint8_t n163_ram[128];
static uint8_t n163_address;
static uint8_t n163_nametable[4];

static void n163_update_timer() {
    if (!mapper_cpu_timer) {
        mapper_timer_cycle = cpu_cycle_now;
    }
    mapper_cpu_timer = (n163_counter & 0x8000) && ((n163_counter & 0x7fff) != 0x7fff);
}

static FRNES_BOARD_TEXT void n163_apply_nametables() {
    uint8_t* pages[4];
    uint8_t vram[4];
    uint32_t rom_slots = 0;
    for (uint32_t slot = 0; slot < 4; slot++) {
        uint8_t value = n163_nametable[slot];
        vram[slot] = value & 1;
        pages[slot] = chr_page(value);
        if (value < 0xe0) {
            rom_slots |= 1 << slot;
        }
    }
    if (rom_slots) {
        set_ppu_nametable_rom(pages, rom_slots, vram);
    } else {
        set_ppu_nametable_pages(vram[0], vram[1], vram[2], vram[3]);
    }
}

static void n163_init() {
    apu_synth_set_expansion(APU_EXPANSION_N163);
    resize_wram(0x2000);
    mapper_6000_mode = MAPPER_6000_REGISTERS;
    n163_write_protect = 0;
    memset(n163_ram, 0, sizeof(n163_ram));
    board_n163_reset();
    n163_address = 0;
    n163_counter = 0;
    n163_update_timer();
    for (uint32_t i = 0; i < 4; i++) {
        set_6502_bank_pointer(i, prg_page((i < 3) ? i : num_8k_ROM_pages - 1));
        n163_nametable[i] = 0xe0 | (i >> 1);
    }
    n163_apply_nametables();
}

static void n163_cpu_run(uint32_t cycles) {
    uint32_t count = (n163_counter & 0x7fff) + cycles;
    if (count >= 0x7fff) {
        count = 0x7fff;
        mapper_irq_flag = 1;
        update_irq_line();
        mapper_cpu_timer = 0;
    }
    n163_counter = (n163_counter & 0x8000) | count;
}

static FRNES_BOARD_TEXT FRNES_COLD uint8_t n163_read_low(uint16_t address, uint8_t open_bus) {
    switch (address & 0xf800) {
        case 0x4800: {
            // (the channels' phases move as the chip plays: mapper_boards.c runs them up to now)
            board_n163_sync(n163_ram, cpu_cycle_now);
            uint8_t value = n163_ram[n163_address & 0x7f];
            if (n163_address & 0x80) {
                n163_address = (n163_address & 0x80) | ((n163_address + 1) & 0x7f);
            }
            return value;
        }
        case 0x5000:
        case 0x5800:
            if (mapper_cpu_timer) {
                mapper_cpu_run(cpu_cycle_now);
            }
            return (address & 0x0800) ? (n163_counter >> 8) : (n163_counter & 0xff);
    }
    return open_bus;
}

static void n163_write_low(uint16_t address, uint8_t data) {
    switch (address & 0xf800) {
        case 0x4800:
            board_n163_sync(n163_ram, cpu_cycle_now);
            n163_ram[n163_address & 0x7f] = data;
            apu_synth_log_write(0x4800 | (n163_address & 0x7f), data, cpu_cycle_now);
            if (n163_address & 0x80) {
                n163_address = (n163_address & 0x80) | ((n163_address + 1) & 0x7f);
            }
            break;
        case 0x5000:
        case 0x5800:
            cpu_sync_devices();
            if (mapper_cpu_timer) {
                mapper_cpu_run(cpu_cycle_now);
            }
            n163_counter = (address & 0x0800) ? ((n163_counter & 0x00ff) | (data << 8)) : ((n163_counter & 0xff00) | data);
            mapper_irq_flag = 0;
            update_irq_line();
            n163_update_timer();
            cpu_cut_batch_for_event(mapper_cycles_to_event());
            break;
    }
}

static FRNES_BOARD_TEXT FRNES_COLD void n163_write(uint16_t address, uint8_t data) {
    switch (address & 0xf800) {
        case 0x8000: case 0x8800: case 0x9000: case 0x9800:
        case 0xa000: case 0xa800: case 0xb000: case 0xb800:
            ppu_sync();
            set_ppu_pattern_table_bank_pointer((address - 0x8000) >> 11, chr_page(data));
            break;
        case 0xc000: case 0xc800: case 0xd000: case 0xd800:
            ppu_sync();
            n163_nametable[(address - 0xc000) >> 11] = data;
            n163_apply_nametables();
            break;
        case 0xe000:
            set_6502_bank_pointer(0, prg_page(data & 0x3f));
            // (bit 6 halts the sound; the synthesizer takes nothing else from $E000, so only a change of it is a sound
            // write: Mappy Kids' title switches this bank 535 times a frame, each a synthesizer span and a sync before)
            if (((data >> 6) & 1) != board_n163_halted()) {
                apu_synth_log_write(0xe000, data, cpu_cycle_now);
                board_n163_sync(n163_ram, cpu_cycle_now);
                board_n163_halt((data >> 6) & 1);
            }
            break;
        case 0xe800:
            set_6502_bank_pointer(1, prg_page(data & 0x3f));
            break;
        case 0xf000:
            set_6502_bank_pointer(2, prg_page(data & 0x3f));
            break;
        case 0xf800:
            n163_address = data;
            n163_write_protect = data;
            break;
    }
}

/**
 * Namco 108 family (Namco 118 / Tengen MIMIC-1 boards): the MMC3's bank registers without its IRQ, mirroring or modes.
 * $8000 (even addresses, $8000-$9FFF) selects R0-R7, odd addresses write it (writes to $A000-$FFFF land on the
 * same two registers); R6 / R7 are 8 kB PRG banks at $8000 / $A000, the last two banks are fixed; CHR:
 *   206: R0 / R1 2 kB at $0000 / $0800 (in 1 kB units, low bit ignored), R2-R5 1 kB at $1000-$1C00
 *   76 (Megami Tensei): R2-R5 2 kB banks at $0000-$1800
 *   88 (Dragon Spirit, Quinty): as 206, with R0 / R1 in the first 64 kB of CHR and R2-R5 in the second
 *   154 (Devil Man): 88, and every write's bit 6 picks the one-screen nametable
 *   95 (Dragon Buster): 206, with bit 5 of R0 / R1 picking the nametable for $2000-$27FF / $2800-$2FFF
 */
static uint8_t namco108_select, namco108_registers[8];

static FRNES_BOARD_TEXT void namco108_apply() {
    const uint8_t* r = namco108_registers;
    set_6502_bank_pointer(0, prg_page(r[6] & 0x3f));
    set_6502_bank_pointer(1, prg_page(r[7] & 0x3f));
    if (mapper_number == 76) {
        for (uint32_t i = 0; i < 4; i++) {
            set_ppu_pattern_table_bank_pointer(i * 2, chr_page((r[2 + i] & 0x3f) * 2));
            set_ppu_pattern_table_bank_pointer(i * 2 + 1, chr_page((r[2 + i] & 0x3f) * 2 + 1));
        }
        return;
    }
    uint32_t low_mask = 0x3f, high_set = 0;
    if ((mapper_number == 88) || (mapper_number == 154)) {
        low_mask = 0x3f;
        high_set = 0x40;
    }
    set_ppu_pattern_table_bank_pointer(0, chr_page((r[0] & low_mask) & 0xfe));
    set_ppu_pattern_table_bank_pointer(1, chr_page((r[0] & low_mask) | 1));
    set_ppu_pattern_table_bank_pointer(2, chr_page((r[1] & low_mask) & 0xfe));
    set_ppu_pattern_table_bank_pointer(3, chr_page((r[1] & low_mask) | 1));
    for (uint32_t i = 0; i < 4; i++) {
        set_ppu_pattern_table_bank_pointer(4 + i, chr_page(high_set ? ((r[2 + i] & 0x3f) | high_set) : r[2 + i]));
    }
    if (mapper_number == 95) {
        uint8_t a = (r[0] >> 5) & 1, b = (r[1] >> 5) & 1;
        set_ppu_nametable_pages(a, a, b, b);
    }
}

FRNES_BOARD_TEXT __attribute__((cold, noinline)) static void namco108_init() {
    static const uint8_t initial[8] = { 0, 2, 4, 5, 6, 7, 0, 1 };
    memcpy(namco108_registers, initial, sizeof(namco108_registers));
    namco108_select = 0;
    // (the Vs. System's protected games' $4020-$5FFF reads: mapper_boards.c's vs_read_low)
    board_low = vs_protection_reads();
    set_6502_bank_pointer(2, prg_page(num_8k_ROM_pages - 2));
    set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
    namco108_apply();
}

static FRNES_BOARD_TEXT __attribute__((noinline)) void namco108_write(uint16_t address, uint8_t data) {
    // (only a write that changes the one-screen page catches the PPU up: Devil Man writes the bank registers mid frame,
    // each sync replaying its scanline so far dot by dot)
    uint8_t page = (data >> 6) & 1;
    if ((mapper_number == 154) && ((ppu_nametable_slot_page(0) != page) || (ppu_nametable_slot_page(1) != page) ||
                                   (ppu_nametable_slot_page(2) != page) || (ppu_nametable_slot_page(3) != page))) {
        ppu_sync();
        set_ppu_nametable_mirroring(page ? NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH : NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW);
    }
    if (!(address & 1)) {
        namco108_select = data & 7;
        return;
    }
    if (namco108_registers[namco108_select] == data) {
        return;
    }
    if (namco108_select < 6) {
        ppu_sync();
    }
    namco108_registers[namco108_select] = data;
    namco108_apply();
}

/**
 * Camerica / Codemasters:
 *   71 (BF909x: Micro Machines, Bee 52, Fire Hawk): $C000-$FFFF selects the 16 kB bank at $8000, the last bank is
 *      fixed at $C000; on Fire Hawk's BF9097, $9000-$9FFF bit 4 picks the one-screen nametable (taken up at the
 *      first write there, other boards never write it)
 *   232 (BF9096, Quattro carts): $8000-$BFFF bits 3-4 pick the 64 kB block, $C000-$FFFF bits 0-1 the 16 kB bank
 *      in it at $8000; $C000 shows the block's last bank
 */
static uint8_t camerica_block, camerica_bank;

static void camerica_apply() {
    if (mapper_number == 232) {
        prg_16k(0, camerica_block * 4 + camerica_bank);
        prg_16k(2, camerica_block * 4 + 3);
    } else {
        prg_16k(0, camerica_bank);
    }
}

FRNES_BOARD_TEXT __attribute__((cold, noinline)) static void camerica_init() {
    camerica_block = camerica_bank = 0;
    prg_16k(2, num_8k_ROM_pages / 2 - 1);
    camerica_apply();
}

static FRNES_BOARD_TEXT __attribute__((noinline)) void camerica_write(uint16_t address, uint8_t data) {
    if (mapper_number == 232) {
        if (address < 0xc000) {
            camerica_block = (data >> 3) & 3;
        } else {
            camerica_bank = data & 3;
        }
        camerica_apply();
        return;
    }
    if (address >= 0xc000) {
        camerica_bank = data & 0x0f;
        camerica_apply();
    } else if ((address & 0xf000) == 0x9000) {
        ppu_sync();
        set_ppu_nametable_mirroring((data & 0x10) ? NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH : NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW);
    }
}

/**
 * Bandai FCG-1 / FCG-2 / LZ93D50 (mappers 16, 153, 159: Dragon Ball, SD Gundam Gaiden, Famicom Jump II, Magical
 * Taruruuto-kun). Registers at address & $F: 0-7 1 kB CHR banks (153: their bit 0 is the 256 kB PRG half), 8 the
 * 16 kB PRG bank at $8000 (the last bank of the half is fixed at $C000), 9 mirroring, A IRQ enable (on the LZ93D50
 * writing it also loads the counter from the latch; it acknowledges the IRQ), B / C the IRQ latch, D the serial
 * EEPROM's SCL (bit 5) / SDA (bit 6) (153: bit 5 enables its PRG RAM). The 16 bit counter counts CPU cycles down
 * while enabled and raises the IRQ when it is 0 (and goes on to $FFFF). iNES mapper 16 decodes the registers at
 * $6000-$7FFF and $8000-$FFFF, 153 / 159 at $8000-$FFFF. $6000-$7FFF reads give the EEPROM's SDA out in bit 4
 * (mapper_read_6000, from read_byte: the CPU cores read everything below $8000 through it).
 * Mapper 16 has a 24C02 (256 bytes, 8 bit word address after a device address), 159 a 24C01 (128 bytes, the 7
 * bit address and read / write bit right after START, bits sent low first); its contents are the battery save.
 */
#define EEPROM_IDLE 0
#define EEPROM_CHIP_ADDRESS 1
#define EEPROM_ADDRESS 2
#define EEPROM_READ 3
#define EEPROM_WRITE 4
#define EEPROM_SEND_ACK 5
#define EEPROM_WAIT_ACK 6

static struct {
    uint8_t size;           // 0 none, 128 (24C01), 255 (256 bytes, 24C02)
    uint8_t mode, next_mode, chip_address, address, data, counter, output, scl, sda;
    uint8_t memory[256];
} eeprom;

static uint8_t bandai_chr[8], bandai_prg, bandai_outer;
// (bandai_submapper: 4 FCG-1 / FCG-2: registers at $6000-$7FFF, B / C write the counter itself, no EEPROM; 5 LZ93D50:
// registers at $8000-$FFFF; 0 iNES mapper 16, which could be either: both ranges, the latch, a 24C02)

// The boards of the known Bandai games by PRG + CHR CRC32, as iNES headers mostly just say 16: mapper, submapper,
// EEPROM (0 none, 128 24C01, 255 24C02)
static const struct { uint32_t crc; uint8_t mapper, submapper, eeprom; } bandai_boards[] = {
    { 0x0cf42e69, 159, 0, 128 }, { 0x183859d2, 159, 0, 128 }, { 0x276ac722, 159, 0, 128 }, { 0xb7f28915, 159, 0, 128 },
    { 0xdcb972ce, 159, 0, 128 }, { 0xe170404c, 159, 0, 128 }, { 0x2194bb29, 159, 0, 128 },
    { 0x136ca449, 16, 5, 255 }, { 0x99240573, 16, 5, 255 }, { 0xa262a81f, 16, 5, 255 }, { 0xa9541452, 16, 5, 255 },
    { 0xb049a8c4, 16, 5, 255 }, { 0xc2840372, 16, 5, 255 }, { 0xdc52bf0c, 16, 5, 255 }, { 0xdb05106e, 16, 5, 0 },
    { 0x2c4421b2, 16, 4, 0 }, { 0x33b899c9, 16, 4, 0 }, { 0x6e68e31a, 16, 4, 0 }, { 0x7b5206af, 16, 4, 0 },
    { 0x9c04c8d5, 16, 4, 0 }, { 0xa851cae9, 16, 4, 0 }, { 0xbfc7a2e9, 16, 4, 0 }, { 0xd343c66a, 16, 4, 0 },
    { 0x3f15d20d, 153, 0, 0 },
};

// a Bandai game's board from the table (or the header's NES 2.0 submapper): its mapper number
FRNES_BOARD_TEXT __attribute__((cold, noinline)) static uint32_t bandai_board(uint32_t number) {
    bandai_submapper = 0;
    eeprom.size = (number == 159) ? 128 : ((number == 16) ? 255 : 0);
    if (cartridge_submapper && (number == 16)) {
        bandai_submapper = cartridge_submapper;
        if (bandai_submapper == 4) {
            eeprom.size = 0;
        }
    }
    for (uint32_t i = 0; i < sizeof(bandai_boards) / sizeof(bandai_boards[0]); i++) {
        if (bandai_boards[i].crc == currentCRC32) {
            bandai_submapper = bandai_boards[i].submapper;
            eeprom.size = bandai_boards[i].eeprom;
            printf(" ...Bandai board: mapper %u submapper %u, EEPROM %u bytes\n", bandai_boards[i].mapper, bandai_submapper,
                eeprom.size == 255 ? 256 : eeprom.size);
            return bandai_boards[i].mapper;
        }
    }
    return number;
}
static uint16_t bandai_latch;

static void eeprom_bit_in(uint8_t* dest, uint8_t value) {
    if (eeprom.counter < 8) {
        uint32_t bit = (eeprom.size == 128) ? eeprom.counter : 7 - eeprom.counter;
        *dest = (*dest & ~(1 << bit)) | (value << bit);
        eeprom.counter++;
    }
}

static void eeprom_bit_out() {
    if (eeprom.counter < 8) {
        uint32_t bit = (eeprom.size == 128) ? eeprom.counter : 7 - eeprom.counter;
        eeprom.output = (eeprom.data >> bit) & 1;
        eeprom.counter++;
    }
}

static void eeprom_write(uint8_t scl, uint8_t sda) {
    uint8_t mask = (eeprom.size == 128) ? 0x7f : 0xff;
    if (eeprom.scl && scl && (sda < eeprom.sda)) {
        // START: SDA falls while SCL is high
        eeprom.mode = (eeprom.size == 128) ? EEPROM_ADDRESS : EEPROM_CHIP_ADDRESS;
        if (eeprom.size == 128) {
            eeprom.address = 0;
        }
        eeprom.counter = 0;
        eeprom.output = 1;
    } else if (eeprom.scl && scl && (sda > eeprom.sda)) {
        // STOP: SDA rises while SCL is high
        eeprom.mode = EEPROM_IDLE;
        eeprom.output = 1;
    } else if (scl > eeprom.scl) {
        // SCL rises: bits are taken in / sent out
        switch (eeprom.mode) {
            case EEPROM_CHIP_ADDRESS: eeprom_bit_in(&eeprom.chip_address, sda); break;
            case EEPROM_ADDRESS:
                if (eeprom.size != 128) {
                    eeprom_bit_in(&eeprom.address, sda);
                } else if (eeprom.counter < 7) {
                    eeprom_bit_in(&eeprom.address, sda);
                } else if (eeprom.counter == 7) {
                    // the 24C01's 8th bit: read or write
                    eeprom.counter = 8;
                    if (sda) {
                        eeprom.next_mode = EEPROM_READ;
                        eeprom.data = eeprom.memory[eeprom.address & mask];
                    } else {
                        eeprom.next_mode = EEPROM_WRITE;
                    }
                }
                break;
            case EEPROM_READ: eeprom_bit_out(); break;
            case EEPROM_WRITE: eeprom_bit_in(&eeprom.data, sda); break;
            case EEPROM_SEND_ACK: eeprom.output = 0; break;
            case EEPROM_WAIT_ACK:
                if (eeprom.size == 128) {
                    if (!sda) {
                        eeprom.next_mode = EEPROM_IDLE;
                    }
                } else if (!sda) {
                    eeprom.next_mode = EEPROM_READ;
                    eeprom.data = eeprom.memory[eeprom.address];
                }
                break;
        }
    } else if (scl < eeprom.scl) {
        // SCL falls: the phases change
        switch (eeprom.mode) {
            case EEPROM_CHIP_ADDRESS:
                if (eeprom.counter == 8) {
                    eeprom.counter = 0;
                    eeprom.output = 1;
                    if ((eeprom.chip_address & 0xa0) == 0xa0) {
                        eeprom.mode = EEPROM_SEND_ACK;
                        if (eeprom.chip_address & 1) {
                            eeprom.next_mode = EEPROM_READ;
                            eeprom.data = eeprom.memory[eeprom.address];
                        } else {
                            eeprom.next_mode = EEPROM_ADDRESS;
                        }
                    } else {
                        eeprom.mode = EEPROM_IDLE;
                    }
                }
                break;
            case EEPROM_ADDRESS:
                if (eeprom.counter == 8) {
                    eeprom.mode = EEPROM_SEND_ACK;
                    eeprom.output = 1;
                    if (eeprom.size != 128) {
                        eeprom.counter = 0;
                        eeprom.next_mode = EEPROM_WRITE;
                    }
                }
                break;
            case EEPROM_READ:
                if (eeprom.counter == 8) {
                    eeprom.mode = EEPROM_WAIT_ACK;
                    eeprom.address = (eeprom.address + 1) & mask;
                }
                break;
            case EEPROM_WRITE:
                if (eeprom.counter == 8) {
                    eeprom.counter = 0;
                    eeprom.mode = EEPROM_SEND_ACK;
                    eeprom.next_mode = (eeprom.size == 128) ? EEPROM_IDLE : EEPROM_WRITE;
                    eeprom.memory[eeprom.address & mask] = eeprom.data;
                    eeprom.address = (eeprom.address + 1) & mask;
                    prg_ram_written = 1;
                }
                break;
            case EEPROM_SEND_ACK:
            case EEPROM_WAIT_ACK:
                if ((eeprom.mode == EEPROM_WAIT_ACK) && (eeprom.size == 128)) {
                    break;
                }
                eeprom.mode = eeprom.next_mode;
                eeprom.counter = 0;
                eeprom.output = 1;
                break;
        }
    }
    eeprom.scl = scl;
    eeprom.sda = sda;
}

uint8_t mapper_6000_reads = 0;

// $6000-$7FFF reads on the EEPROM boards: SDA in bit 4, open bus in the rest
uint8_t mapper_read_6000(uint16_t address, uint8_t open_bus) {
    if (prg_ram_disabled) {
        return open_bus;
    }
    if (__builtin_expect(board_6000, 0)) {
        return board_read_6000(address, open_bus);
    }
    if (vrc2_latch_mode) {
        return (open_bus & 0xfe) | vrc2_latch;
    }
    (void)address;
    return (open_bus & 0xe7) | (eeprom.output << 4);
}

static void bandai_apply_prg() {
    prg_16k(0, bandai_outer | bandai_prg);
    prg_16k(2, bandai_outer | 0x0f);
}

FRNES_BOARD_TEXT __attribute__((cold, noinline)) static void bandai_init() {
    memset(bandai_chr, 0, sizeof(bandai_chr));
    bandai_prg = bandai_outer = bandai_irq_enabled = 0;
    bandai_counter = bandai_latch = 0;
    eeprom.mode = eeprom.next_mode = EEPROM_IDLE;
    eeprom.chip_address = eeprom.address = eeprom.data = eeprom.counter = eeprom.scl = eeprom.sda = 0;
    eeprom.output = 1;
    resize_wram(0x2000);
    set_wram_bank_pointer(WRAM);
    if (mapper_number == 153) {
        // $6000-$7FFF is its battery PRG RAM; registers only at $8000
        mapper_6000_mode = 0;
    } else {
        mapper_6000_mode = MAPPER_6000_REGISTERS;
        mapper_6000_reads = 1;
    }
    bandai_apply_prg();
    mapper_cpu_timer = 0;
}

static FRNES_BOARD_TEXT __attribute__((noinline)) void bandai_write(uint16_t address, uint8_t data) {
    if (mapper_cpu_timer) {
        mapper_cpu_run(cpu_cycle_now);
    }
    uint32_t reg = address & 0x0f;
    switch (reg) {
        case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
            bandai_chr[reg] = data;
            if ((mapper_number == 153) || (num_8k_ROM_pages >= 0x40)) {
                uint32_t outer = 0;
                for (uint32_t i = 0; i < 8; i++) {
                    outer |= (bandai_chr[i] & 1) << 4;
                }
                bandai_outer = outer;
                bandai_apply_prg();
            } else if (num_1k_VROM_pages) {
                ppu_sync();
                set_ppu_pattern_table_bank_pointer(reg, chr_page(data));
            }
            break;
        case 8:
            bandai_prg = data & 0x0f;
            bandai_apply_prg();
            break;
        case 9: {
            static const uint8_t modes[4] = { NAMETABLE_MIRRORING_VERTICAL, NAMETABLE_MIRRORING_HORIZONTAL,
                                              NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW, NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH };
            ppu_sync();
            set_ppu_nametable_mirroring(modes[data & 3]);
        } break;
        case 10:
            bandai_irq_enabled = data & 1;
            if (bandai_submapper != 4) {
                bandai_counter = bandai_latch;
            }
            cpu_irq_clear(CPU_IRQ_MAPPER);
            if (!mapper_cpu_timer) {
                mapper_timer_cycle = cpu_cycle_now;
            }
            mapper_cpu_timer = bandai_irq_enabled;
            cpu_cut_batch_for_event(mapper_cycles_to_event());
            break;
        case 11:
            if (bandai_submapper == 4) {
                bandai_counter = (bandai_counter & 0xff00) | data;
            } else {
                bandai_latch = (bandai_latch & 0xff00) | data;
            }
            break;
        case 12:
            if (bandai_submapper == 4) {
                bandai_counter = (bandai_counter & 0x00ff) | (data << 8);
            } else {
                bandai_latch = (bandai_latch & 0x00ff) | (data << 8);
            }
            break;
        case 13:
            if (mapper_number == 153) {
                // (PRG RAM enable, not emulated: always on)
            } else if (eeprom.size) {
                eeprom_write((data >> 5) & 1, (data >> 6) & 1);
            }
            break;
    }
}

static void bandai_cpu_run(uint32_t cycles) {
    // the IRQ goes up at each cycle the counter is 0 at (and keeps counting through $FFFF)
    if (bandai_irq_enabled && (cycles > bandai_counter)) {
        mapper_irq_flag |= CPU_IRQ_MAPPER;
        update_irq_line();
    }
    bandai_counter = (uint16_t)(bandai_counter - cycles);
}

/**
 * Mapper dispatch
 */
FRNES_BOARD_TEXT __attribute__((noinline)) int mapper_init(uint32_t number) {
    printf("mapper - initializing mapper %lu\n", number);
    if ((number == 16) || (number == 153) || (number == 159)) {
        number = bandai_board(number);
    } else {
        number = board_override(number);
    }
    // (the boards frnes_rom_support says are supported, the same list: mapper_boards.c)
    if (!board_supported(number)) {
        printf(" ...ERROR: mapper %lu is not supported\n", number);
        return -1;
    }
    mapper_number = number;
    mapper_write_select();
    mapper_irq_flag = 0;
    mapper_clock_dot = 0;
    mapper_irq_armed = 0;
    mapper_chr_latch = 0;
    mapper_ext_attributes = 0;
    mapper_sprite_chr_split = 0;
    mapper_cpu_timer = 0;
    vrc_irq_used = 0;
    mapper_6000_mode = 0;
    mapper_chr_ram_slots = 0;
    mmc6 = 0;
    mapper_6000_reads = 0;
    prg_ram_disabled = 0;
    vrc2_latch_mode = 0;
    board_6000 = 0;
    board_timer = 0;
    board_low = 0;
    board_low_unsynced = 0;
    board_own_irqs = 0;
    board_saves = 0;
    mapper_save_written = 0;
    ppu_bus_address_hook = NULL;
    apu_synth_set_expansion(APU_EXPANSION_NONE);
    switch (number) {
        case 0:
        case 3:
            break;
        case 1:
            mmc1_init();
            break;
        case 2:
            uxrom_init();
            break;
        case 118:
            mmc3_init();
            break;
        case 4:
            mmc3_init();
            if (mmc6_detect()) {
                printf(" ...MMC6\n");
                mmc6 = 1;
                mmc6_init();
            }
            break;
        case 7:
            axrom_init();
            break;
        case 9:
        case 10:
            mmc2_init();
            break;
        case 69:
            fme7_init();
            break;
        case 21: case 22: case 23: case 25:
            vrc4_init();
            break;
        case 11: case 30: case 34: case 66: case 75: case 111:
            discrete_init();
            break;
        case 73: case 67:
            vrc3_latch = vrc3_counter = 0;
            vrc3_control = sunsoft3_toggle = 0;
            prg_16k(0, 0);
            prg_16k(2, num_8k_ROM_pages / 2 - 1);
            break;
        case 119:
            mmc3_init();
            break;
        case 64:
            rambo_init();
            break;
        case 68:
            sunsoft4_init();
            break;
        case 206: case 76: case 88: case 95: case 154:
            namco108_init();
            break;
        case 71: case 232:
            camerica_init();
            break;
        case 16: case 153: case 159:
            bandai_init();
            break;
        case 19:
            n163_init();
            break;
        case 24: case 26:
            vrc6_init();
            break;
        case 85:
            vrc7_init();
            break;
        case 5:
            if (mmc5_init() != 0) {
                return -1;
            }
            break;
        default:
            if (board_init(number) == 0) {
                break;
            }
            printf(" ...ERROR: mapper %lu is not supported\n", number);
            return -1;
    }
    // four-screen boards (iNES flag): 4 kB of nametable RAM, VRAM pages 0-3, which mappers that switch mirroring leave
    // alone when the flag is set
    if ((NesHeader.byInfo1 & 8) && (number != 111)) {
        set_ppu_nametable_pages(0, 1, 2, 3);
    }
    update_fetch_hooks();
    printf(" ...done\n");
    return 0;
}

/*
 * $8000-$FFFF writes go to the board's handler through mapper_write_fn (in .hotdata), chosen when the mapper is set up
 * (mapper_write_select), instead of a switch on mapper_number in write_byte_6502_c: the handlers are noinline, so no
 * board's code is inlined into the hot write function, and adding or changing a board (or a variant of one: RAMBO-1
 * and Tengen 800037, MMC3 and TxSROM) can't change that function's size or its slot's layout. One indirect call on a
 * mapper register write, where the switch took a jump table. Same handlers as the switch had, case for case.
 */
static FRNES_BOARD_TEXT __attribute__((noinline)) void flash_write_ignored(uint16_t address, uint8_t data) {
    // (mapper 111: writes to $8000-$FFFF program the flash, not emulated)
    (void)address;
    (void)data;
}

static FRNES_BOARD_TEXT __attribute__((noinline)) void bandai_mapper_write(uint16_t address, uint8_t data) {
    if ((mapper_number != 16) || (bandai_submapper != 4)) {
        bandai_write(address, data);
    }
}

void (*mapper_write_fn)(uint16_t address, uint8_t data) = board_write;
// the board's handler behind mapper_synced_write (every board but MMC1)
static void (*mapper_board_write_fn)(uint16_t address, uint8_t data) FRNES_COLD_BSS;

// a ROM space write: the PPU and APU caught up to the write first (the scanline counter must have seen every clock
// before it), then the board's handler; MMC1's catches up itself, only for the writes that change something. (In the
// "c" slot, after the bank switching: nothing in the hot slots moves)
FRNES_HOT(c_write_sync) void mapper_synced_write(uint16_t address, uint8_t data) {
    cpu_sync_devices();
    mapper_board_write_fn(address, data);
}

static void mapper_write_select() {
    switch (mapper_number) {
        case 1:
            mapper_write_fn = mmc1_write;
            break;
        case 2:
            mapper_write_fn = uxrom_write;
            break;
        case 3:
            mapper_write_fn = cnrom_write;
            break;
        case 7:
            mapper_write_fn = axrom_write;
            break;
        case 9:
        case 10:
            mapper_write_fn = mmc2_write;
            break;
        case 5:
            mapper_write_fn = mmc5_write;
            break;
        case 69:
            mapper_write_fn = fme7_write;
            break;
        case 21: case 22: case 23: case 25:
            mapper_write_fn = vrc4_write;
            break;
        case 11: case 30: case 34: case 66: case 75:
            mapper_write_fn = discrete_write;
            break;
        case 111:
            mapper_write_fn = flash_write_ignored;
            break;
        case 73:
            mapper_write_fn = vrc3_write;
            break;
        case 67:
            mapper_write_fn = sunsoft3_write;
            break;
        case 119:
            mapper_write_fn = mmc3_write;
            break;
        case 64:
            // (Tengen 800037, mapper 158, runs as the RAMBO-1 with its own handler: board_override)
            mapper_write_fn = (cartridge_mapper_number == 158) ? rambo_800037_write : rambo_write;
            break;
        case 68:
            mapper_write_fn = sunsoft4_write;
            break;
        case 206: case 76: case 88: case 95: case 154:
            mapper_write_fn = namco108_write;
            break;
        case 71: case 232:
            mapper_write_fn = camerica_write;
            break;
        case 16: case 153: case 159:
            mapper_write_fn = bandai_mapper_write;
            break;
        case 19:
            mapper_write_fn = n163_write;
            break;
        case 24: case 26:
            mapper_write_fn = vrc6_write;
            break;
        case 85:
            mapper_write_fn = vrc7_write;
            break;
        case 4: case 118:
            mapper_write_fn = mmc3_write;
            break;
        default:
            mapper_write_fn = board_write;
            break;
    }
    if (mapper_write_fn != mmc1_write) {
        extern void mapper_mmc3_synced_write(uint16_t address, uint8_t data);
        mapper_board_write_fn = mapper_write_fn;
        mapper_write_fn = (mapper_write_fn == mmc3_write) ? mapper_mmc3_synced_write : mapper_synced_write;
    }
}

// battery backed RAM that isn't the $6000 window's WRAM (MMC6's 1 kB inside the chip), or NULL
uint8_t* mapper_battery_data(uint32_t* size) {
    if (mmc6) {
        *size = sizeof(mmc6_ram);
        return mmc6_ram;
    }
    if (((mapper_number == 16) || (mapper_number == 159)) && eeprom.size) {
        *size = (eeprom.size == 128) ? 128 : 256;
        return eeprom.memory;
    }
    if (board_saves) {
        return board_battery_data(size);
    }
    return NULL;
}

void mapper_battery_prepare() {
    if (board_saves) {
        board_battery_prepare();
    }
}

void mapper_battery_reset() {
    if (board_saves) {
        board_battery_reset();
    }
}

// the CPU reading an interrupt vector (perf's hook, cpu.c / cpu_exact.c): the MMC5 watches for $FFFA / $FFFB, the NMI
// vector, and clears its IRQ (pending flag and line) and its scanline counter there (Mesen 2): Castlevania III (U)
// reads $5204 in its NMI handler and sees no IRQ pending
FRNES_BOARD_TEXT void mapper_vector_fetch(uint16_t vector) {
    if ((mapper_number == 5) && (vector == VECTOR_NMI)) {
        mmc5_vector_nmi();
    }
}

// after a save was loaded into mapper_battery_data()
void mapper_battery_loaded() {
    if (mmc6) {
        mmc6_apply();
    }
    if (board_saves) {
        board_battery_loaded();
    }
}

uint8_t mapper_read_low(uint16_t address, uint8_t open_bus) {
    if (mapper_number == 5) {
        return mmc5_read_low(address, open_bus);
    }
    if (mapper_number == 19) {
        return n163_read_low(address, open_bus);
    }
    if (board_low) {
        return board_read_low(address, open_bus);
    }
    return open_bus;
}

void mapper_write_low(uint16_t address, uint8_t data) {
    if (mapper_number == 5) {
        mmc5_write_low(address, data);
    } else if (mapper_number == 19) {
        n163_write_low(address, data);
    } else if ((mapper_number == 111) && (address >= 0x5000)) {
        gtrom_write(data);
    } else if (board_low) {
        board_write_low(address, data);
    }
}

// a PPU clocked scanline counter's IRQ goes up during the CPU cycle the CPU sees the clock's dot in
static void mapper_ppu_irq_at(uint8_t had_irq, uint32_t dot, uint32_t delay) {
    if (!had_irq && (mapper_irq_flag & CPU_IRQ_MAPPER)) {
        mapper_irq_flag &= ~CPU_IRQ_MAPPER;
        cpu_irq_update_line();
        cpu_irq_raise_at(CPU_IRQ_MAPPER, ppu_dot_cpu_cycle(ppu_current_scanline(), dot) + delay);
    }
}

FRNES_HOT(5_mmc3_ppu_clock) void mapper_ppu_clock() {
    uint8_t had_irq = mapper_irq_flag & CPU_IRQ_MAPPER;
    switch (mapper_number) {
        case 5:
            mmc5_clock();
            mapper_ppu_irq_at(had_irq, ppu_mapper_clock_a12_dot - 2, 0);
            break;
        case 4:
        case 118:
        case 119:
            mmc3_ppu_clock();
            break;
        case 64:
            if (!rambo_cycle_mode) {
                rambo_clock_counter();
                mapper_ppu_irq_at(had_irq, ppu_mapper_clock_a12_dot - 1 + RAMBO_IRQ_DELAY_SCANLINE_DOTS, 0);
            }
            break;
        case 48:
            tc0690_ppu_clock();
            break;
    }
}

/*
 * Save states: mapper_boards.c keeps the board hooks (mapper_board_state, out of the link time optimized code, where
 * ~10 kB of hooks moved the partitions and with them ordinary .text and .bss). Here only this one cold function: the
 * variables of every board in this file copied by value into or out of a buffer (no variable's address escapes, which
 * would make the hot code reload them), what the hooks need from here, and the boards' own remapping after a load
 */
// the IRQ line, the CPU timer, the $6000-$7FFF mode, then each board's registers and counters (all of them, whichever
// board is loaded: one list, one copy loop)
#define MAPPER_STATE(X, A) X(mapper_irq_flag) X(mapper_clock_dot) X(mapper_irq_armed) X(mapper_chr_latch) \
    X(mapper_cpu_timer) X(mapper_timer_cycle) X(mapper_chr_ram_slots) X(mapper_6000_mode) X(prg_ram_disabled) \
    X(mapper_6000_reads) \
    X(mmc1_shift) X(mmc1_count) X(mmc1_control) X(mmc1_chr0) X(mmc1_chr1) X(mmc1_prg) X(mmc1_last_write_cycle) \
    X(axrom_bank) A(vrc1_chr) \
    X(mmc3_bank_select) A(mmc3_registers) X(mmc3_irq_latch) X(mmc3_irq_counter) X(mmc3_irq_reload) X(mmc3_irq_enabled) \
    X(mmc3_mirroring) X(mmc3_a12) X(mmc3_a12_low_cycle) X(mmc6_a001) A(mmc6_ram) \
    A(mmc2_chr) A(mmc2_latch) \
    X(fme7_command) X(fme7_irq_control) X(fme7_counter) \
    X(vrc_irq) X(vrc_irq_used) A(vrc4_prg) X(vrc4_swap) A(vrc4_chr) X(vrc2_latch_mode) X(vrc2_latch) \
    X(rambo_select) A(rambo_registers) X(rambo_latch) X(rambo_counter) X(rambo_reload) X(rambo_cycle_mode) \
    X(rambo_enabled) X(rambo_prescaler) X(rambo_a12) X(rambo_a12_low_cycle) \
    X(mmc5_prg_mode) X(mmc5_chr_mode) X(mmc5_exram_mode) X(mmc5_nametables) X(mmc5_fill_tile) X(mmc5_fill_attribute) \
    A(mmc5_prg) A(mmc5_chr) X(mmc5_chr_upper) X(mmc5_chr_last_b) X(mmc5_irq_compare) X(mmc5_irq_enabled) \
    X(mmc5_irq_pending) X(mmc5_in_frame) X(mmc5_scanline) X(mmc5_multiplicand) X(mmc5_multiplier) \
    X(mapper_ext_attributes) X(mapper_sprite_chr_split) \
    X(n163_write_protect) A(n163_ram) X(n163_address) A(n163_nametable) X(n163_counter) \
    A(bandai_chr) X(bandai_prg) X(bandai_outer) X(bandai_irq_enabled) X(bandai_counter) X(bandai_latch) \
    X(bandai_submapper) X(eeprom) \
    X(namco108_select) A(namco108_registers) \
    X(camerica_block) X(camerica_bank) \
    A(sunsoft4_nt) X(sunsoft4_control) \
    X(vrc3_latch) X(vrc3_counter) X(vrc3_control) X(sunsoft3_toggle) \
    X(tc0690_delay) X(tc0690_latch) X(tc0690_counter) X(tc0690_reload) X(tc0690_enabled) X(tc0690_a12) \
    X(tc0690_jetsons) X(tc0690_a12_low_cycle)
#define MAPPER_STATE_X_SIZE(var) + (uint32_t)sizeof(var)
#define MAPPER_STATE_X_SAVE(var) { __typeof__(var) value = (var); memcpy(at, &value, sizeof(value)); at += sizeof(value); }
#define MAPPER_STATE_X_LOAD(var) { __typeof__(var) value; memcpy(&value, at, sizeof(value)); (var) = value; at += sizeof(value); }
#define MAPPER_STATE_A_SAVE(array) for (uint32_t i = 0; i < sizeof(array) / sizeof((array)[0]); i++) MAPPER_STATE_X_SAVE((array)[i])
#define MAPPER_STATE_A_LOAD(array) for (uint32_t i = 0; i < sizeof(array) / sizeof((array)[0]); i++) MAPPER_STATE_X_LOAD((array)[i])

// (mapper.h) MAPPER_STATE_SIZE: the list's size; SAVE / LOAD copy it to / from 'buffer'; INFO fills 'memory'; AFTER maps
// again the pages the boards derive from their registers (MMC5's, the Namco 163's and Sunsoft-4's nametables)
FRNES_BOARD_TEXT uint32_t mapper_state(uint32_t mode, uint8_t* buffer, board_memory_t* memory) {
    uint8_t* at = buffer;
    switch (mode) {
        case MAPPER_STATE_SAVE:
            MAPPER_STATE(MAPPER_STATE_X_SAVE, MAPPER_STATE_A_SAVE)
            break;
        case MAPPER_STATE_LOAD:
            MAPPER_STATE(MAPPER_STATE_X_LOAD, MAPPER_STATE_A_LOAD)
            break;
        case MAPPER_STATE_INFO:
            memory -> vram = VRAM;
            memory -> wram_size = WRAM_size;
            memory -> mapper = mapper_number;
            break;
        case MAPPER_STATE_AFTER:
            switch (mapper_number) {
                case 5: mmc5_apply_prg(); mmc5_apply_chr(); break;
                case 19: n163_apply_nametables(); break;
                case 68: sunsoft4_apply_nametables(); break;
            }
            break;
    }
    return 0 MAPPER_STATE(MAPPER_STATE_X_SIZE, MAPPER_STATE_X_SIZE);
}
