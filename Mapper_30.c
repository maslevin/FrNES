#include "Mapper_30.h"

void Mapper_30_Init() {
	BankTable[4] = ROM_pages[0];
	BankTable[5] = ROM_pages[1];
    BankTable[6] = ROM_pages[ num_8k_ROM_pages - 2];
    BankTable[7] = ROM_pages[ num_8k_ROM_pages - 1];

    for ( int nPage = 0; nPage < 8; ++nPage )
        PPUBANK[ nPage ] = VROM_pages[nPage];
}

void Mapper_30_Write( uint16 wAddr, unsigned char byData ) {
	if ((wAddr >= 0x8000) && (wAddr <= 0xFFFF)) {
		unsigned char prg = (byData & 0x1f) << 1;
		unsigned char chr = ((byData & 0x60) >> 5) << 3;

		BankTable[4] = ROM_pages[prg % num_8k_ROM_pages];
		BankTable[5] = ROM_pages[(prg + 1) % num_8k_ROM_pages];

		for ( int nPage = 0; nPage < 8; nPage++)
			PPUBANK[ nPage ] = VROM_pages[(nPage + chr)];
	}
}