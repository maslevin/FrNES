#include "Mapper_7.h"

/*-------------------------------------------------------------------*/
/*  Mapper 7 Init Function                                           */
/*-------------------------------------------------------------------*/
void Mapper_7_Init() {
	ROMBANK0 = ROM_pages[0];
	ROMBANK1 = ROM_pages[1];
	ROMBANK2 = ROM_pages[2];
	ROMBANK3 = ROM_pages[3];

	for (uint8 nPage = 0; nPage < 8; ++nPage )
		PPUBANK[ nPage ] = VROM_pages[nPage];
}

/*-------------------------------------------------------------------*/
/*  Mapper 7 Write Function                                          */
/*-------------------------------------------------------------------*/
void Mapper_7_Write(uint16 wAddr, unsigned char byData) {
//	printf("Map7_Write: $%04X, %02X\n", wAddr, byData);
	unsigned char bank;

	bank = ((byData & 0x07) << 2) % num_8k_ROM_pages;

	ROMBANK0 = ROM_pages[bank];
	ROMBANK1 = ROM_pages[bank + 1];
	ROMBANK2 = ROM_pages[bank + 2];
	ROMBANK3 = ROM_pages[bank + 3];

	if (byData & 0x10) {
		pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_HIGH);
	} else {
		pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_LOW);
	}
}