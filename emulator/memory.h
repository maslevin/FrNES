/**
 * FrNES 1.0.0 - memory.h 
 * 
 * 05/17/2025
 * By Matt Slevinsky (maslevin)
 * 
 * Summary:
 * Memory Managment Routines for FrNES
 * FrNES 1.0.0 uses the Dreamcast MMU for memory mapping which requires some conventions around allocation of
 * buffers so they can be successfully mapped using the MMU.  This means that the ROM Loader obeys the following
 * conventions:
 * 
 * ROM Pages must be loaded on a 0x1000 (4kB) boundary
 * VROM / VRAM Pages must be loaded on a 0x400 (1kB) boundary
 * 
 * This file provides functions to initialize the NES memory map and configure it for any mapper configuration
 * 
 */

#ifndef __6502_MEMORY_H
#define __6502_MEMORY_H

#include <kos.h>

#include "6502_registers.h"

/**
 * Code that runs every instruction / scanline goes into numbered .text.hot.N sections, which the linker script
 * places in fixed slots (tools/hot_slots.py), each at a fixed position in the 8 KB direct mapped instruction cache.
 * A group of several functions gives each its own name (FRNES_HOT(5_mmc3_apply_prg)...): its slot sorts them by
 * name, so their order doesn't depend on LTO's partitions. noclone keeps LTO from making copies (.isra /
 * .constprop) that would land outside the section.
 */
#ifdef FRNES_HOST
#define FRNES_HOT(n) __attribute__((noinline))
#else
#define FRNES_HOT(n) __attribute__((section(".text.hot." #n), noclone, noinline))
#endif

/**
 * A rarely run function that LTO would otherwise inline into a hot one (a mapper's register handler into write_byte,
 * the per dot line output into the PPU catch-up): kept out of line and in the cold code, so the hot function's code
 * stays dense in the instruction cache (step 5 of tests/perf/consolidation-plan.md)
 */
#define FRNES_COLD __attribute__((noinline, cold))
/**
 * Code only run at load time or on rare events, placed last in .text (tools/shlelf_hot.xc), after everything else: adding
 * some moves no other code (the C core's read_word_6502_c and others are in the ordinary .text, outside the hot slots)
 */
#ifdef FRNES_HOST
#define FRNES_COLD_TEXT __attribute__((noinline, cold))
#else
#define FRNES_COLD_TEXT __attribute__((noinline, cold, section(".text.~frnes_cold")))
#endif

/**
 * A board's code (its register handlers, bank switching, init) that isn't in a hot slot or tools/hot_kos.txt: in
 * .text.~frnes_board, which tools/shlelf_hot.xc places after all the library code with mapper_boards.c's, fds.c's and
 * the cold code, so a board growing or a new one moves nothing that runs every frame (land9b's 0xc4 bytes of board code
 * in mapper.c had moved KOS's interrupt, scheduler and G2 code, LTO's partitions mixing the two). Only the section:
 * GCC still inlines it where it would, so the code is what it was
 */
#ifdef FRNES_HOST
#define FRNES_BOARD_TEXT
#else
#define FRNES_BOARD_TEXT __attribute__((section(".text.~frnes_board")))
#endif

/**
 * Const tables read every instruction / sample go in .frnes_hot_rodata, inside .hotdata (tools/shlelf_hot.xc): in
 * .rodata their operand cache sets would move with the size of unrelated code
 */
#ifdef FRNES_HOST
#define FRNES_HOT_RODATA __attribute__((aligned(32)))
#else
#define FRNES_HOT_RODATA __attribute__((section(".frnes_hot_rodata"), aligned(32)))
#endif

/**
 * Large const tables only read at ROM load (the game database: mapper_table.h, prg_ram_table.h) go at the end of
 * .rodata (tools/shlelf_hot.xc), after every other constant table: their size moves nothing hot (.rodata's start is
 * pinned, .hotdata follows at a 16 kB boundary and the .bss buffers are pinned), and they stay out of .data
 */
/**
 * Writable state only touched at load time or on rare events (a board's registers or flags, harness buffers): last in
 * .bss (tools/shlelf_hot.xc), after every other .bss symbol, so adding some moves nothing already there onto other
 * operand cache sets. Zero-initialized only (the section is NOBITS)
 */
#ifdef FRNES_HOST
#define FRNES_COLD_BSS
#else
#define FRNES_COLD_BSS __attribute__((section(".bss.~frnes_cold")))
#endif

#ifdef FRNES_HOST
#define FRNES_COLD_RODATA
#else
#define FRNES_COLD_RODATA __attribute__((section(".frnes_cold_rodata")))
#endif

/**
 * Store queue mapping for the frame output (instead of KOS's sq_lock, see memory.c)
 */
uint32_t* frnes_sq_map(void* dest);
#ifndef FRNES_HOST
// mmu_page_map_static, returning the UTLB entry it loaded (kos_mainline.c; KOS's page_size_t / page_prot_t, which the
// host's kos.h doesn't have)
unsigned int frnes_kos_map_static(uintptr_t virt, uintptr_t phys, page_size_t page_size, page_prot_t page_prot, bool cached);
#endif

#define read_byte read_byte_6502_c
#define read_word read_word_6502_c
#define write_byte write_byte_6502_c

uint8_t read_byte(uint16_t address);
void write_byte(uint16_t address, uint8_t value);
uint16_t read_word(uint16_t address);

/** 
 * Virtual Addresses in SH-4 Address Space for 6502 RAM, WRAM, ROMBANKs 0-3, REGISTER/STATE BUFFER
 */
#define RAM_ADDR            (uintptr_t)0x02000000
#define WRAM_ADDR           (uintptr_t)0x02006000
#define ROMBANK0_ADDR       (uintptr_t)0x02008000
#define ROMBANK1_ADDR       (uintptr_t)0x0200a000
#define ROMBANK2_ADDR       (uintptr_t)0x0200c000
#define ROMBANK3_ADDR       (uintptr_t)0x0200e000

/*
 * The PPU's windows (pattern tables, nametables, OAM / palette)
 */
#define PPU_WINDOW_BASE            (uintptr_t)0x02010000
#define PPU_MEMORY_ADDR            (PPU_WINDOW_BASE)
#define PATTERN_TABLES_ADDR        (PPU_WINDOW_BASE)
#define NAMETABLE0_ADDR            (PPU_WINDOW_BASE + 0x2000)
#define NAMETABLE1_ADDR            (PPU_WINDOW_BASE + 0x2400)
#define NAMETABLE2_ADDR            (PPU_WINDOW_BASE + 0x2800)
#define NAMETABLE3_ADDR            (PPU_WINDOW_BASE + 0x2c00)
#define NAMETABLE0_ADDR_MIRROR     (PPU_WINDOW_BASE + 0x3000)
#define NAMETABLE1_ADDR_MIRROR     (PPU_WINDOW_BASE + 0x3400)
#define NAMETABLE2_ADDR_MIRROR     (PPU_WINDOW_BASE + 0x3800)
#define NAMETABLE3_ADDR_MIRROR     (PPU_WINDOW_BASE + 0x3c00)
#define SPRRAM_ADDR                (PPU_WINDOW_BASE + 0x4000)
#define PALETTE_RAM_ADDR           (PPU_WINDOW_BASE + 0x4100)
#define REGISTERS_ADDR             (uintptr_t)0x02014400

#define NAMETABLE_MIRRORING_HORIZONTAL          0
#define NAMETABLE_MIRRORING_VERTICAL            1
#define NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW   2
#define NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH  3

/**
 * Pointers to buffers for RAM or WRAM
 * RAM must be allocated on a 0x400 (1kB) boundary
 * WRAM must be allocated on a 0x1000 (4kB) boundary
 * WRAM may not be present dependent on cartridge configuration in which case this variable will be NULL
 * VRAM must be allocated on a 0x400 (1kB) boundary
 */
extern uint8_t* RAM;
extern uint8_t* WRAM;
extern uint32_t WRAM_size;
extern uint8_t* VRAM;
// (memory.c: the register page's and OAM / palette buffers, the nametables' generation)
extern uint8_t* register_buffer;
extern uint8_t* ppu_memory_buffer;
extern uint32_t ppu_nametable_generation;
extern uint8_t* SPRRAM;
extern uint8_t* PALETTE_RAM;

/*
 * A direct access to the CPU window at RAM_ADDR. The Dreamcast build maps only $0000-$1FFF (RAM and its mirrors),
 * $6000-$7FFF while there is WRAM, and $8000-$FFFF: anything else ($2000-$5FFF, $6000-$7FFF without WRAM) is a TLB
 * miss on the console, which the host build (the whole window is memory there) would never show. Host builds with
 * FRNES_WINDOW_CHECK (host/Makefile's default) check every such access against that map and abort with the file and
 * line (host/host_memory.c); player builds read the window as before.
 */
#if defined(FRNES_HOST) && defined(FRNES_WINDOW_CHECK)
uint32_t nes_window_check(uint32_t address, const char* file, int line);
#define NES_WINDOW(address) nes_window_check((address), __FILE__, __LINE__)
#else
#define NES_WINDOW(address) (address)
#endif
#define NES_WINDOW_BYTE(address) (((uint8_t*)RAM_ADDR)[NES_WINDOW(address)])

/**
 * Writes a nametable byte ($2000-$3EFF) through the MMU nametable mapping, keeping every alias of the VRAM page
 * coherent in the SH4 operand cache. Reads can use PPU_MEMORY_ADDR / NAMETABLE0_ADDR directly.
 */
void ppu_nametable_write(uint16_t address, uint8_t data);

/**
 * Pointers to cpu_registers and ppu_registers structs
 */
extern _6502_registers_t* cpu_registers;
extern _PPU_registers_t* ppu_registers;

/**
 * Size of main 6502 memory
 */
#define RAM_SIZE 0x800

/**
 * Initializes NES memory system
 *   - returns -1 if an error is encountered
 */
int initialize_memory();

/**
 * Initializes registers buffer
 *   - returns -1 if an error is encountered
 */
int initialize_registers();

/**
 *
 */
int initialize_rom(uint8_t* rombank0data, uint8_t* rombank1data, uint8_t* rombank2data, uint8_t* rombank3data);

/**
 * configure_wram
 * @param size - the size of WRAM to configure in bytes
 * @param data - optional, for SRAM saved content is intended to be passed here
 *
 * WRAM can have multiple configurations, 4kB/8kB/and greater will be implemented as 
 * 4kB pages.  Smaller WRAM can be implemented as a number of smaller 1kB pages.
 */
int initialize_wram(uint32_t size, uint8_t* data);

/**
 * @brief Initializes PPU pattern tables, ppu address space 0x0000 - 0x1fff in 8 x 1kB pages
 * 
 * @return int 
 */
int initialize_ppu_pattern_tables(uint8_t* ppubank0data, uint8_t* ppubank1data, uint8_t* ppubank2data, uint8_t* ppubank3data, uint8_t* ppubank4data, uint8_t* ppubank5data, uint8_t* ppubank6data, uint8_t* ppubank7data);

/**
 * @brief Initializes PPU name tables, ppu address space 0x2000 - 0x2fff in 4 x 1kB pages
 * defaults to horizonal mirroring, set subsequent mirroring configuraiton after inialization with 
 * set_ppu_nametable_mirroring
 *
 * @return int
 */
int initialize_ppu_memory();

//(uint8_t* ppunametable0data, uint8_t* ppunametable1data, uint8_t* ppunametable2data, uint8_t* ppunametable3data);

/**
 * Shuts down NES memory system
 */
void shutdown_memory();

/**
 * Sets a single bank to point to a new data location in memory.  Data must be 0x1000 (4kB) aligned.
 * @param bank - a valid bank index between 0-3 (ROMBANKs 0 through 3)
 * @param data - a valid 0x1000 (4kB) aligned page
 */
void set_6502_bank_pointer(uint32_t bank, uint8_t* data);

/** 
 * Sets all 4 ROM bank pointers at once
 * @param rombank0data - a valid 0x1000 (4kB) aligned page
 * @param rombank1data - a valid 0x1000 (4kB) aligned page
 * @param rombank2data - a valid 0x1000 (4kB) aligned page
 * @param rombank3data - a valid 0x1000 (4kB) aligned page
 */
void set_6502_rom_bank_pointers(uint8_t* rombank0data, uint8_t* rombank1data, uint8_t* rombank2data, uint8_t* rombank3data);

/**
 * Sets a single PPU pattern table bank to point to a new data location in memory.  Data must be 0x400 (1kB) aligned.
 * @param bank - a valid bank index between 0 and 7
 * @param data - a valid 0x400 (1kB) aligned page
 */
void set_ppu_pattern_table_bank_pointer(uint32_t bank, uint8_t* data);
const uint8_t* ppu_pattern_table_bank_pointer(uint32_t bank);
// the VRAM page a nametable slot (0-3) maps
uint8_t ppu_nametable_slot_page(uint32_t slot);
// a $2007 write to CHR RAM (pattern table address): a page in two slots at once (CPROM's bank 0 at $0000 and $1000)
// has a cache line per slot, so the write invalidates the others' (memory.c; the CHR RAM slots are write-through)
extern uint8_t ppu_chr_alias_slots;
void ppu_chr_ram_alias_write(uint16_t address);
static inline void ppu_chr_ram_write(uint16_t address, uint8_t data) {
    ((uint8_t*)PATTERN_TABLES_ADDR)[address] = data;
    if (__builtin_expect((ppu_chr_alias_slots >> (address >> 10)) & 1, 0)) {
        ppu_chr_ram_alias_write(address);
    }
}

/**
 * Sets all 8 PPU pattern table banks at once to new data locations.  Data must be 0x400 (1kB) aligned.
 * @param ppubank0data - a valid 0x400 (1kB) aligned page
 * @param ppubank1data - a valid 0x400 (1kB) aligned page
 * @param ppubank2data - a valid 0x400 (1kB) aligned page
 * @param ppubank3data - a valid 0x400 (1kB) aligned page 
 * @param ppubank4data - a valid 0x400 (1kB) aligned page
 * @param ppubank5data - a valid 0x400 (1kB) aligned page
 * @param ppubank6data - a valid 0x400 (1kB) aligned page
 * @param ppubank7data - a valid 0x400 (1kB) aligned page 
 */
void set_ppu_pattern_table_bank_pointers(uint8_t* ppubank0data, uint8_t* ppubank1data, uint8_t* ppubank2data, uint8_t* ppubank3data, uint8_t* ppubank4data, uint8_t* ppubank5data, uint8_t* ppubank6data, uint8_t* ppubank7data);

/**
 * @brief Sets nametable mirroring configuration for the PPU
 *   this method is only expected to be used in cartridges which use only the default 2kB internal VRAM
 *   if your cartridge uses extended VRAM (ie 3 or 4-screen modes), the mapper should use set_ppu_nametable_bank_pointer 
 *   instead to bankswitch in external VRAM pages individually
 * 
 * @param mirroring_mode - a valid mirroring constant 
 *   0 = horizontal
 *   1 = vertical
 *   2 = single screen low
 *   3 = single screen hight
 */
void set_ppu_nametable_mirroring(uint32_t mirroring_mode);
// nametable slots $2000 / $2400 / $2800 / $2C00 to 1 kB VRAM pages: 0 / 1 are CIRAM, 2 / 3 extra mapper pages
void set_ppu_nametable_pages(uint8_t page0, uint8_t page1, uint8_t page2, uint8_t page3);
void ppu_vram_page_write(uint32_t page, uint32_t offset, uint8_t data);
uint8_t ppu_vram_page_read(uint32_t page, uint32_t offset);
void ppu_vram_page_fill(uint32_t page, uint8_t tile, uint8_t attribute);
// set by every CPU write to $6000-$7FFF (the player's battery autosave watches it)
extern volatile uint8_t prg_ram_written;
// writes back the $6000 window's lines from the operand cache
void wram_window_writeback();
void set_wram_bank_pointer(uint8_t* data);
// the page a window slot shows, as the bank functions above were given it (save states): 0-3 the 8 kB PRG slots at
// $8000-$FFFF, 4 the 8 kB at $6000 (NULL without WRAM), 5-12 the 1 kB pattern table slots, 13-16 the nametable slots
#define MEMORY_SLOT_PRG 0
#define MEMORY_SLOT_WRAM 4
#define MEMORY_SLOT_CHR 5
#define MEMORY_SLOT_NAMETABLE 13
#define MEMORY_SLOTS 17
uint8_t* memory_slot_page(uint32_t slot);
void set_ppu_nametable_rom(uint8_t* const* pages, uint32_t rom_slots, const uint8_t* vram_pages);
int resize_wram(uint32_t size);

#endif
