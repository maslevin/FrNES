/*===================================================================*/
/*                                                                   */
/*  Mapper.h : Mapper Function                                       */
/*                                                                   */
/*  1999/11/03  Racoon     New preparation                           */
/*  2001/12/22  maslevin   0.60 Final Release                        */
/*                         Mapper fixes + 7 + 9 initial drivers      */
/*  2024/07/24  maslevin   Fully rework mapper framework             */
/*                                                                   */
/*===================================================================*/

#ifndef _MAPPER_H
#define _MAPPER_H

/*-------------------------------------------------------------------*/
/*  Include files                                                    */
/*-------------------------------------------------------------------*/
#include <kos.h>

#include "cartridge.h"
#include "pNesX.h"
#include "pNesX_System_DC.h"

/*-------------------------------------------------------------------*/
/*  Mapper Configuration                                             */
/*-------------------------------------------------------------------*/
typedef struct __attribute__ ((packed, aligned(4))) Mapper_s {
	void (*init)();
	void (*write)( uint16 wAddr, unsigned char byData );
	unsigned char (*read)( uint16 wAddr );
	void (*vsync)();
	void (*hsync)();
} Mapper_t;

typedef struct __attribute__ ((packed, aligned(4))) MapperTable_s {
  int nMapperNo;
  Mapper_t * mapper;
} MapperTable_t;

extern MapperTable_t Mappers[];
extern Mapper_t* mapper;
extern uint8 MapperNo;
extern uint8 SubmapperNo;

// void debugRomPages();

// Cartridge mapper init is responsible for setting PPU bank pointers for reading and writing
// Setting bank pointers to NULL will disable / read or write to that region
// PPU Bank pointers for indexes 8-15 are currently unsupported but would map to CIRAM nametables / attribute tables
// CPU Bank pointers for indexes 0-2 are currently unsupported but would map to system RAM, PPU/APU/Joypad register region
// Don't set them and expect the system to do anything with that ;)

// PPU Bank Pointers 0-7 should be set with 1kB VROM/VRAM page pointers from cartridge and map into PPU memory space 0x0000 ... 0x1FFF
extern uint8* PPU_BANK_WRITE[16];
extern uint8* PPU_BANK_READ[16];

// CPU Bank Pointers 3-7 should be set with 8kB ROM/WRAM page pointers from cartridge and map into CPU memory space 0x6000 ... 0xFFFF
extern uint8* CPU_BANK_WRITE[8];
extern uint8* CPU_BANK_READ[8];

void set_cpu_banks_to_rom_pages(uint32 page4, uint32 page5, uint32 page6, uint32 page7);
void set_ppu_banks_low_to_vrom_pages(uint32 page0, uint32 page1, uint32 page2, uint32 page3);
void set_ppu_banks_high_to_vrom_pages(uint32 page4, uint32 page5, uint32 page6, uint32 page7);
void set_ppu_banks_low_to_vram_pages(uint32 page0, uint32 page1, uint32 page2, uint32 page3);
void set_ppu_banks_high_to_vram_pages(uint32 page4, uint32 page5, uint32 page6, uint32 page7);

#endif