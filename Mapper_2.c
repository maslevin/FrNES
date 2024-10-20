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
	BankTable[4] = ROM_pages[0];
	BankTable[5] = ROM_pages[1];
	BankTable[6] = ROM_pages[num_8k_ROM_pages - 2];
	BankTable[7] = ROM_pages[num_8k_ROM_pages - 1];
}

/*-------------------------------------------------------------------*/
/*  Mapper 2 Write Function                                          */
/*-------------------------------------------------------------------*/
void Mapper_2_Write( uint16 wAddr, unsigned char byData ) {
	if (wAddr & 0x8000) {
		/* Set ROM Banks */
		byData %= NesHeader.byRomSize;
		byData <<= 1;
		// printf("Write [$%04X]: Setting BankTable[4] to [%u] and BankTable[5] to [%u]\n", wAddr, byData, byData + 1);

		BankTable[4] = ROM_pages[byData];
		BankTable[5] = ROM_pages[byData + 1];
	}
}