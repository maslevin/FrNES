#include "Mapper_30.h"

void Mapper_30_Init() {
	ROMBANK0 = ROMPAGE( 0 );
	ROMBANK1 = ROMPAGE( 1 );
	ROMBANK2 = ROMLASTPAGE(1);
	ROMBANK3 = ROMLASTPAGE(0);
}

void Mapper_30_Write( uint16 wAddr, unsigned char byData ) {
//	printf("Map30_Write: $%04X, %02X\n", wAddr, byData);
	if ((wAddr >= 0x8000) && (wAddr <= 0xFFFF)) {
		unsigned char prg = byData & 0x1f;
		unsigned char chr = (byData & 0x60) >> 5;

		uint32 num_8k_ROM_banks = NesHeader.byRomSize * 2; 
		uint32 page0 = (prg * 2);
		uint32 page1 = (prg * 2) + 1;
	//	printf("Setting ROMBANK0 to page [%lu] and setting ROMBANK1 to page [%lu]\n", page0, page1);
		ROMBANK0 = ROMPAGE( page0 % num_8k_ROM_banks );
		ROMBANK1 = ROMPAGE( page1 % num_8k_ROM_banks );

		unsigned char c = chr * 8;
		for ( int nPage = 0; nPage < 8; nPage++)
			PPUBANK[ nPage ] = &VROM[(nPage + c) * 0x400 ];
	}
}