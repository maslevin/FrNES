#include "Mapper_11.h"

void Mapper_11_Init() {
    BankTable[4] = ROM_pages[0];
	BankTable[5] = ROM_pages[1];
	BankTable[6] = ROM_pages[2];
	BankTable[7] = ROM_pages[3];

	/* Set PPU Banks */
	for ( int nPage = 0; nPage < 8; ++nPage )
		PPUBANK[ nPage ] = VROM_pages [nPage];

    pNesX_Mirroring(MIRRORING_VERTICAL);
}

void Mapper_11_Write( uint16 wAddr, unsigned char byData ) {
    uint8 prg_bank = (byData & 0x01) << 2;
    uint8 chr_bank = ((byData & 0x70) >> 4) << 3;

    BankTable[4] = ROM_pages[prg_bank % num_8k_ROM_pages];
    BankTable[5] = ROM_pages[(prg_bank + 1) % num_8k_ROM_pages];
    BankTable[6] = ROM_pages[(prg_bank + 2) % num_8k_ROM_pages];
    BankTable[7] = ROM_pages[(prg_bank + 3) % num_8k_ROM_pages];

	for ( int nPage = 0; nPage < 8; ++nPage ) {
		PPUBANK[ nPage ] = VROM_pages[ (chr_bank + nPage) % num_1k_VROM_pages ];
    }
}
