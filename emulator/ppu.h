#ifndef __PPU_H
#define __PPU_H

#include <kos.h>

#include "memory.h"
#include "6502_registers.h"

int initialize_ppu();
void ppu_set_power_on_dot(uint32_t dot, uint32_t odd);
void shutdown_ppu();

#define PPU_FRAMEBUFFER_WIDTH  256
#define PPU_FRAMEBUFFER_HEIGHT 240

/**
 * The last rendered picture as 6 bit NES color indexes (palette RAM values), 256 x 240, one byte per pixel
 */
extern uint8_t ppu_framebuffer[PPU_FRAMEBUFFER_WIDTH * PPU_FRAMEBUFFER_HEIGHT];

/**
 * Sets where finished scanlines are written: line y goes to output + y * 256 as 32 bit words. With store_queues set,
 * output is a store queue area address and each 32 byte block is flushed as soon as it is written. Defaults to
 * ppu_framebuffer.
 */
void ppu_set_output(uint8_t* output, int store_queues);

/**
 * Number of PPU frames completed since power on
 */
extern uint32_t ppu_frame_count;

/**
 * Set when palette RAM or PPUMASK's greyscale bit changes, so the renderer rebuilds its color tables:
 */
// palettes whose colors changed since the renderer last built its color tables: bit n for background palette n,
// PPU_PALETTE_DIRTY_SPRITES for the sprite palettes
#define PPU_PALETTE_DIRTY_SPRITES 0x10
#define PPU_PALETTE_DIRTY_ALL     0x1f
extern uint8_t ppu_palette_dirty;

/**
 * Bumped whenever OAM is written ($2004, OAM DMA), the renderer keeps per scanline sprite lists built from it
 */
extern uint32_t ppu_oam_generation;

/**
 * Handles a $2002 read racing the VBlank flag at the start of scanline 241, call after ppu_sync_status()
 */
void ppu_status_race();

/**
 * $2000 writes racing VBlank: the VBlank flag as an NMI enable sees it, and cancelling an NMI raised a dot ago
 */
int ppu_vblank_flag_for_nmi();
void ppu_nmi_disable_race();

/**
 * Set when a catch-up raises the VBlank NMI, with how many dots it ran past it
 */
extern uint8_t ppu_nmi_raised;
uint32_t ppu_oam_busy_dot();
void ppu_data_increment();
// a $2007 write is about to be made: what a scanline with rendering off shows for the dots it takes to land
void ppu_blank_data_write(uint8_t data);
// the same for a second $2006 write, before v changes
void ppu_blank_address_write();

/**
 * Addresses the PPU puts on its bus outside the rendering fetches (for mappers that watch it, e.g. the MMC3's A12):
 * v after a second $2006 write, a $2007 read or write's address (while rendering, the fetch address it sees) and v
 * after its increment. Called with the PPU brought up to the access (ppu_current_scanline / ppu_current_dot give
 * where); NULL unless a mapper sets it, and only reached from the $2006 / $2007 paths.
 */
extern void (*ppu_bus_address_hook)(uint16_t address);
static inline void ppu_bus_address(uint16_t address) {
    if (__builtin_expect(ppu_bus_address_hook != NULL, 0)) {
        ppu_bus_address_hook(address & 0x3fff);
    }
}
// the PPU bus address at a dot of a rendering scanline (what a $2007 read then reloads its buffer from)
uint16_t ppu_rendering_bus_address(uint32_t dot);
uint8_t ppu_oam_read_while_rendering(uint32_t dot);
// a $2001 write changing rendering on / off (OAM row corruption)
void ppu_rendering_switched(uint8_t old_mask, uint8_t new_mask);

/*
 * The PPU I/O latch (open bus of its registers) decays: a bit that hasn't been driven for a while reads as 0.
 * Each bit remembers the frame it was last driven, and reads clear the ones older than PPU_OPEN_BUS_DECAY_FRAMES
 * (frames counted at the start of scanline 240, as Mesen 2 does).
 */
#define PPU_OPEN_BUS_DECAY_FRAMES 3
extern uint32_t ppu_open_bus_frame[8];
extern uint32_t ppu_bus_frame_count;
static inline void ppu_open_bus_drive(uint8_t value, uint8_t mask) {
    ppu_registers -> PPUOPENBUS = (ppu_registers -> PPUOPENBUS & ~mask) | (value & mask);
    for (uint32_t bit = 0; bit < 8; bit++) {
        if (mask & (1 << bit)) {
            ppu_open_bus_frame[bit] = ppu_bus_frame_count;
        }
    }
}
// a $2002 read at cpu_cycle_now that can be answered from the status as it is: before STATUS_STABLE_UNTIL (at most a
// frame ahead: an older value is stale; the window never reaches past this frame's vblank), with the rendering state
// and OAM it was worked out from unchanged, and before the APU's next event, which a catch-up would have run
#define PPU_STATUS_STABLE_SPAN 30000
static inline int ppu_status_stable() {
    extern int32_t apu_cycles_to_event;
    extern uint32_t cpu_cycle_now, cpu_devices_synced_cycle, ppu_oam_generation;
    return ((uint32_t)(ppu_registers -> STATUS_STABLE_UNTIL - cpu_cycle_now - 1) < PPU_STATUS_STABLE_SPAN) &&
        (ppu_registers -> STATUS_STABLE_OAM == ppu_oam_generation) &&
        (ppu_registers -> STATUS_STABLE_MASK == ppu_registers -> PPUMASK) &&
        (ppu_registers -> STATUS_STABLE_CTRL == (ppu_registers -> PPUCTRL & PPUCTRL_SPRITE_SIZE)) &&
        (ppu_registers -> STATUS_STABLE_OAMADDR == ppu_registers -> OAMADDR) &&
        ((int32_t)(cpu_cycle_now - cpu_devices_synced_cycle) < apu_cycles_to_event);
}
void ppu_status_stable_update(void);
// a PPU register read's catch-up, and $2002's status work after it (both skipped by a read ppu_status_stable allows)
// (inline in read_byte_6502_c)
static inline void ppu_read_sync(uint32_t address) {
    extern void cpu_sync_devices();
    if (((address & 0x0007) == 0x2) && ppu_status_stable()) {
        ppu_registers -> STATUS_SKIP = 1;
        return;
    }
    ppu_registers -> STATUS_SKIP = 0;
    cpu_sync_devices();
}
void ppu_status_read_sync(void);
static inline uint8_t ppu_open_bus_read() {
    uint8_t value = ppu_registers -> PPUOPENBUS;
    for (uint32_t bit = 0; bit < 8; bit++) {
        if ((value & (1 << bit)) && ((ppu_bus_frame_count - ppu_open_bus_frame[bit]) > PPU_OPEN_BUS_DECAY_FRAMES)) {
            value &= ~(1 << bit);
        }
    }
    ppu_registers -> PPUOPENBUS = value;
    return value;
}
// CPU cycles from the point the PPU was last brought up to until the VBlank NMI could next come up
uint32_t ppu_cycles_to_vblank_nmi();
extern uint32_t ppu_nmi_dots_after;

/**
 * Advances the PPU by a number of CPU cycles (3 PPU dots each). Raises the vblank flag and NMI at
 * scanline 241 dot 1 and clears the status flags at scanline 261 dot 1. With rendering enabled the
 * visible and pre-render scanlines are emulated dot by dot into ppu_framebuffer (background, sprites,
 * sprite 0 hit, sprite overflow), otherwise whole scanlines are skipped at once.
 *
 * @return 1 if a frame completed (the PPU wrapped back to scanline 0), otherwise 0
 */
int ppu_run(uint32 cpu_cycles);

/**
 * ppu_run() stops at the end of a frame (carrying any remaining dots into the next call). Returns 1 once per
 * finished frame.
 */
int ppu_take_frame();

/**
 * CPU cycles until the next point the CPU has to stop at for the PPU: vblank / NMI at scanline 241 dot 1, or the
 * end of the frame.
 */
uint32_t ppu_cycles_to_next_event();

/**
 * CPU cycles until the PPU next clocks the mapper (for scanline IRQs)
 */
uint32_t ppu_cycles_to_mapper_clock(uint32_t clocks);
// while mapper_ppu_clock() runs: the dot of the current scanline A12 rises at for that clock (MMC3: 261, or 325 with
// only the background at $1000, which also gives the pre-render line a clock for its first background fetch at 5)
extern uint32_t ppu_mapper_clock_a12_dot;

/**
 * The CPU cycle (cpu_cycle_now's time base, for cpu_irq_raise_at) during which the CPU sees a change the PPU makes as
 * it emulates dot 'dot' of scanline 'scanline': the dot being emulated now when called from the PPU (a mapper clock)
 * or a register access (ppu_bus_address_hook), otherwise the next time the PPU gets there. A dot in the last two
 * thirds of a CPU cycle is seen during the next one (Mesen 2's alignment, as for the VBlank NMI).
 */
uint32_t ppu_dot_cpu_cycle(uint32_t scanline, uint32_t dot);

/**
 * Must be called before anything that changes rendering state or reads it mid-scanline: PPU register writes,
 * $2004 / $2007 reads and OAM DMA. Brings a lazily rendered scanline up to the current dot.
 */
void ppu_sync();
// the per dot pass's line buffer: pixel x of the scanline being drawn, after ppu_sync() past it (the Zapper's light)
const uint8_t* ppu_line_buffer_address(void);
// how the current scanline is being rendered: PPU_LINE_LAZY when ppu_sync() has something to do
#define PPU_LINE_PERDOT 0
#define PPU_LINE_LAZY   1
extern uint8_t ppu_line_mode;
// before a PPU register write (register 0-7, a $2000 write changing more than the NMI enable): 1 when the scanline
// being rendered lazily can take it without ppu_sync() ($2000 nametable bits, $2001 sprite switch, $2005)
int ppu_sync_lazy_write(uint32_t reg, uint8_t data);

/**
 * Must be called before a $2002 read, applies the sprite 0 hit / overflow flags of a lazily rendered scanline
 */
void ppu_sync_status();

uint32_t ppu_current_scanline();
uint32_t ppu_current_dot();

#endif