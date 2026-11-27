/**
 * FrNES 1.0.0 - memory.h 
 * 
 * 05/18/2025
 * By Matt Slevinsky (maslevin)
 * 
 * Summary:
 * Cartridge Management Routines for FrNES
 * The ROM Loader obeys the following conventions:
 * 
 * ROM Pages must be loaded on a 0x1000 (4kB) boundary
 * VROM / VRAM Pages must be loaded on a 0x400 (1kB) boundary
 * 
 * This file provides functions to initialize the NES memory map and configure it for any mapper configuration
 * 
 */

#ifndef __CARTRIDGE_H
#define __CARTRIDGE_H

#include <kos.h>

// (2 MB each: all but 163 of Matt's 16383 dumps, the big MMC3 multicarts (45, 176) among them; ROM_pages / VROM_pages
// are in FRNES_COLD_BSS, at the end of .bss, so their size moves nothing else. cartridge_convert refuses larger files)
#define MAXIMUM_ROM_SIZE 2097152
#define MAXIMUM_VROM_SIZE 2097152
#define MAXIMUM_VRAM_SIZE 32768
#define MAXIMUM_WRAM_SIZE 131072

extern uint8_t* ROM;
extern uint8_t* VROM;
extern uint8_t* cartridge_VRAM;
extern uint8_t* WRAM;

extern uint32_t num_ROM_bytes_allocated;
extern uint32_t num_VROM_bytes_allocated;
extern uint32_t num_VRAM_bytes_allocated;
extern uint32_t num_WRAM_bytes_allocated;

extern uint32_t num_8k_ROM_pages;
extern uint32_t num_8k_WRAM_pages;
extern uint32_t num_1k_VROM_pages;
extern uint32_t num_8k_NVRAM_pages;
extern uint32_t num_1k_VRAM_pages;

#define MAXIMUM_ROM_PAGES (MAXIMUM_ROM_SIZE / 8192)
#define MAXIMUM_VROM_PAGES (MAXIMUM_VROM_SIZE / 1024)
#define MAXIMUM_VRAM_PAGES (MAXIMUM_VRAM_SIZE / 1024)
#define MAXIMUM_WRAM_PAGES (MAXIMUM_WRAM_SIZE / 8192)

extern uint8_t* ROM_pages[MAXIMUM_ROM_PAGES];
extern uint8_t* VROM_pages[MAXIMUM_VROM_PAGES];
extern uint8_t* VRAM_pages[MAXIMUM_VRAM_PAGES];
extern uint8_t* WRAM_pages[MAXIMUM_WRAM_PAGES];

/* .nes File Header */
typedef struct __attribute__ ((packed, aligned(4))) NesHeader_s {
  uint8_t byID[ 4 ];
  uint8_t byRomSize;
  uint8_t byVRomSize;
  uint8_t byInfo1;
  uint8_t byInfo2;
  uint8_t byReserve[ 8 ];
} NesHeader_t;

/* .nes File Header */
extern NesHeader_t NesHeader;

/**
 * iNES / NES 2.0 mapper number of the loaded cartridge
 */
extern uint32_t cartridge_mapper_number;
// the NES 2.0 header's submapper, or the game database's for an iNES 1.0 ROM it knows (0 otherwise)
extern uint32_t cartridge_submapper;
// from the game database, for an iNES 1.0 ROM it knows: one screen mirroring at power on (1 screen A, 2 screen B; 0 the
// header's), and a validated entry saying no battery where the header says one (the player then keeps saving only a
// save that's already there, see main.c)
extern uint8_t cartridge_single_screen, cartridge_db_no_battery;

extern uint32_t SRAM_Enabled;

//The crc32 of the currently selected rom
extern uint32_t currentCRC32;

/* Other */
extern uint8_t ROM_Mirroring;
extern uint8_t ROM_SRAM;
extern uint8_t ROM_Trainer;
extern uint8_t ROM_FourScr;

int load_nes_file(const char* filepath);
int unload_nes_file();

#endif