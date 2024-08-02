#include "Mapper_30.h"

void Mapper_30_Init() {
	ROMBANK0 = ROMPAGE( 0 );
	ROMBANK1 = ROMPAGE( 1 );
	ROMBANK2 = ROMLASTPAGE(1);
	ROMBANK3 = ROMLASTPAGE(0);
}

void Mapper_30_Write( uint16 wAddr, unsigned char byData ) {
//	printf("Map30_Write: $%04X, %02X\n", wAddr, byData);
    unsigned char prg = byData & 0x1f;
    unsigned char chr = (byData & 0x60) >> 5;

	uint32 num_8k_ROM_banks = NesHeader.byRomSize * 2; 
    prg &= (num_8k_ROM_banks - 1);
    ROMBANK0 = ROMPAGE( prg * 2 );
    ROMBANK1 = ROMPAGE( (prg * 2) + 1 );

	unsigned char c = chr * 8;
	for ( int nPage = 0; nPage < 8; ++nPage )
		PPUBANK[ nPage ] = &PPURAM[(nPage + c) * 0x400 ];
}