#include "Mapper_3.h"

/*===================================================================*/
/*                                                                   */
/*                     Mapper 3 (VROM Switch)                        */
/*                                                                   */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 3                                              */
/*-------------------------------------------------------------------*/
void Mapper_3_Init() {
	int nPage;

	/* Set ROM Banks */
	if (num_8k_ROM_pages > 2) {
		ROMBANK0 = ROM_pages[0];
		ROMBANK1 = ROM_pages[1];
		ROMBANK2 = ROM_pages[2];
		ROMBANK3 = ROM_pages[3];
	} else {
		ROMBANK0 = ROM_pages[0];
		ROMBANK1 = ROM_pages[1];
		ROMBANK2 = ROM_pages[0];
		ROMBANK3 = ROM_pages[1];
	}

	/* Set PPU Banks */
	for ( nPage = 0; nPage < 8; ++nPage )
		PPUBANK[ nPage ] = VROM_pages[nPage];
}

/*-------------------------------------------------------------------*/
/*  Mapper 3 Write Function                                          */
/*-------------------------------------------------------------------*/
void Mapper_3_Write( uint16 wAddr, unsigned char byData ) {
	if (wAddr & 0x8000) {
		//printf("Map3: Setting PPU banks to page [%u]\n", byData);
		uint16 base = (byData % NesHeader.byVRomSize) << 3;

		PPUBANK[0] = VROM_pages[base];
		PPUBANK[1] = VROM_pages[base + 1];
		PPUBANK[2] = VROM_pages[base + 2];
		PPUBANK[3] = VROM_pages[base + 3];
		PPUBANK[4] = VROM_pages[base + 4];
		PPUBANK[5] = VROM_pages[base + 5];
		PPUBANK[6] = VROM_pages[base + 6];
		PPUBANK[7] = VROM_pages[base + 7];
	}
}