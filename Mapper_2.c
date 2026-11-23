#include "Mapper.h"
#include "Mapper_2.h"

/*===================================================================*/
/*                                                                   */
/*                        Mapper 2 (UNROM)                           */
/*                                                                   */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 2                                              */
/*-------------------------------------------------------------------*/
void Mapper_2_Init() {
	/* Set ROM Banks */
	ROMBANK0 = ROM_pages[0];
	ROMBANK1 = ROM_pages[1];
	ROMBANK2 = ROM_pages[num_8k_ROM_pages - 2];
	ROMBANK3 = ROM_pages[num_8k_ROM_pages - 1];
}

/*-------------------------------------------------------------------*/
/*  Mapper 2 Write Function                                          */
/*-------------------------------------------------------------------*/
void Mapper_2_Write( uint16 wAddr, unsigned char byData ) {
	if (wAddr & 0x8000) {
		/* Set ROM Banks */
		byData %= NesHeader.byRomSize;
		byData <<= 1;
		// printf("Write [$%04X]: Setting ROMBANK0 to [%u] and ROMBANK1 to [%u]\n", wAddr, byData, byData + 1);

		ROMBANK0 = ROM_pages[byData];
		ROMBANK1 = ROM_pages[byData + 1];
	}
}