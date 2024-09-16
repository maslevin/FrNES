#ifndef __CARTRIDGE_H
#define __CARTRIDGE_H

#include <kos.h>

#include "Mapper.h"

#define MAXIMUM_ROM_SIZE 524288
#define MAXIMUM_VROM_SIZE 524288
#define MAXIMUM_VRAM_SIZE 32768
#define MAXIMUM_WRAM_SIZE 131072

extern uint8 ROM[MAXIMUM_ROM_SIZE];
extern uint8 VROM[MAXIMUM_VROM_SIZE];
extern uint8 VRAM[MAXIMUM_VRAM_SIZE];
extern uint8 WRAM[MAXIMUM_WRAM_SIZE];

extern uint32 num_ROM_bytes_allocated;
extern uint32 num_VROM_bytes_allocated;
extern uint32 num_WRAM_bytes_allocated;

extern uint32 num_8k_ROM_pages;
extern uint32 num_1k_VROM_pages;

#define MAXIMUM_ROM_PAGES (MAXIMUM_ROM_SIZE / 8192)
#define MAXIMUM_VROM_PAGES (MAXIMUM_VROM_SIZE / 1024)

extern uint8* ROM_pages[MAXIMUM_ROM_PAGES];
extern uint8* VROM_pages[MAXIMUM_VROM_PAGES];

/* .nes File Header */
typedef struct __attribute__ ((packed, aligned(4))) NesHeader_s {
  uint8 byID[ 4 ];
  uint8 byRomSize;
  uint8 byVRomSize;
  uint8 byInfo1;
  uint8 byInfo2;
  uint8 byReserve[ 8 ];
} NesHeader_t;

/* .nes File Header */
extern NesHeader_t NesHeader;

extern uint32 SRAM_Enabled;

//The crc32 of the currently selected rom
extern uint32 currentCRC32;

/* Other */
extern uint8 ROM_Mirroring;
extern uint8 ROM_SRAM;
extern uint8 ROM_Trainer;
extern uint8 ROM_FourScr;

bool ReadRom(const char* filepath, uint32 filesize);

#endif