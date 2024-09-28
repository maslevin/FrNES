#include "Mapper_9.h"

/*-------------------------------------------------------------------*/
/*  Mapper 9 Variables                                               */
/*-------------------------------------------------------------------*/
unsigned char Map9_Regs[6];
unsigned char Map9_Latch_0000;
unsigned char Map9_Latch_1000;
unsigned char Map9_bank1swap;
unsigned char Map9_bank2swap;

void Mapper_9_set_VROM_0000() {
	unsigned char bank_num;

	if (Map9_Latch_0000 == 0xFD)
		bank_num = Map9_Regs[1];
	else
		bank_num = Map9_Regs[2];

	bank_num <<= 2;
	pNesX_DebugPrint("Map9: Setting Sprite PPU Banks to %u, %u, %u, %u\n", bank_num, bank_num + 1, bank_num + 2, bank_num + 3);

	PPUBANK[0] = VROM_pages[bank_num];
	PPUBANK[1] = VROM_pages[bank_num + 1];
	PPUBANK[2] = VROM_pages[bank_num + 2];
	PPUBANK[3] = VROM_pages[bank_num + 3];
}

void Mapper_9_set_VROM_1000() {
	unsigned char bank_num;

	if (Map9_Latch_1000 == 0xFD)
		bank_num = Map9_Regs[3];
	else
		bank_num = Map9_Regs[4];

	bank_num <<= 2;
	pNesX_DebugPrint("Map9: Setting Character PPU Banks to %u, %u, %u, %u\n", bank_num, bank_num + 1, bank_num + 2, bank_num + 3);	

	PPUBANK[4] = VROM_pages[bank_num];
	PPUBANK[5] = VROM_pages[bank_num + 1];
	PPUBANK[6] = VROM_pages[bank_num + 2];
	PPUBANK[7] = VROM_pages[bank_num + 3];
}

void Mapper_9_PPU_Latch_FDFE(uint16 wAddr) {
	unsigned char latch = (wAddr & 0x0FF0) >> 4;
	if (wAddr & 0x1000) {
		if (latch != Map9_Latch_1000) {
			pNesX_DebugPrint("Map9: Latching BG Addr [0x%04x] on Scanline [%li]\n", wAddr, ppuinfo.PPU_Scanline);			
			Map9_Latch_1000 = latch;
			Mapper_9_set_VROM_1000();
		}
	} else {
		if (latch != Map9_Latch_0000) {
			pNesX_DebugPrint("Map9: Latching Spr Addr [0x%04x] on Scanline [%li]\n", wAddr, ppuinfo.PPU_Scanline);		
			Map9_Latch_0000 = (wAddr & 0x0FF0) >> 4;
			Mapper_9_set_VROM_0000();
		}
	}
}

/*-------------------------------------------------------------------*/
/*  Mapper 9 Init Function                                           */
/*-------------------------------------------------------------------*/
void Mapper_9_Init() {
	// set ROM bank pointers
	ROMBANK0 = ROM_pages[0];
	ROMBANK1 = ROM_pages[ num_8k_ROM_pages - 3];
	ROMBANK2 = ROM_pages[ num_8k_ROM_pages - 2];
	ROMBANK3 = ROM_pages[ num_8k_ROM_pages - 1];

	// clean out the registers
	Map9_Regs[0] = 0;
	Map9_Regs[1] = 0;
	Map9_Regs[2] = 4;
	Map9_Regs[3] = 0;
	Map9_Regs[4] = 0;
	Map9_Regs[5] = 0;

	Map9_Latch_0000 = 0xFE;
	Map9_Latch_1000 = 0xFE;

	Mapper_9_set_VROM_0000();
	Mapper_9_set_VROM_1000();
}

void Mapper_9_Write(uint16 wAddr, unsigned char byData) {
	switch(wAddr & 0xF000) {
		case 0xA000: {
			// 8K ROM bank at $8000
			Map9_Regs[0] = byData & 0x0F;
			ROMBANK0 = ROM_pages[Map9_Regs[0]];
			pNesX_DebugPrint("Map9: Setting ROMBANK0 to Page [%u]\n", Map9_Regs[0]);
		} break;

		case 0xB000: {
			// B000-BFFF: select 4k VROM for (0000) $FD latch
			Map9_Regs[1] = byData & 0x1F;
			pNesX_DebugPrint("Map9: Setting VROM for Sprite $FD Latch to Page [%u]\n", Map9_Regs[1]);
			Mapper_9_set_VROM_0000();
		} break;

		case 0xC000: {
			// C000-CFFF: select 4k VROM for (0000) $FE latch
			Map9_Regs[2] = byData & 0x1F;
			pNesX_DebugPrint("Map9: Setting VROM for Sprite $FE Latch to Page [%u]\n", Map9_Regs[2]);
			Mapper_9_set_VROM_0000();
		} break;

		case 0xD000: {
			// D000-DFFF: select 4k VROM for (1000) $FD latch
			Map9_Regs[3] = byData & 0x1F;
			pNesX_DebugPrint("Map9: Setting VROM for Character $FD Latch to Page [%u]\n", Map9_Regs[3]);
			Mapper_9_set_VROM_1000();
		} break;

		case 0xE000: {
			// E000-EFFF: select 4k VROM for (1000) $FE latch
			Map9_Regs[4] = byData & 0x1F;
			pNesX_DebugPrint("Map9: Setting VROM for Character $FE Latch to Page [%u]\n", Map9_Regs[4]);
			Mapper_9_set_VROM_1000();
		} break;

		case 0xF000: {
			Map9_Regs[5] = byData;
			if (Map9_Regs[5] & 0x01) {
				//Horizontal Mirror
				pNesX_DebugPrint("Map9: Setting Mirroring to Horizontal\n");
				pNesX_Mirroring(MIRRORING_HORIZONTAL);
			} else {
				//Vertical Mirror
				pNesX_DebugPrint("Map9: Setting Mirroring to Vertical\n");				
				pNesX_Mirroring(MIRRORING_VERTICAL);
			}
		} break;
	}
}