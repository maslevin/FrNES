#include "Mapper_11.h"

void Mapper_11_Init() {
    ROMBANK0 = ROM_pages[0];
	ROMBANK1 = ROM_pages[1];
	ROMBANK2 = ROM_pages[2];
	ROMBANK3 = ROM_pages[3];

	/* Set PPU Banks */
	for ( int nPage = 0; nPage < 8; ++nPage )
		PPUBANK[ nPage ] = VROM_pages [nPage];

    pNesX_Mirroring(MIRRORING_VERTICAL);
}

void Mapper_11_Write( uint16 wAddr, unsigned char byData ) {
    uint8 prg_bank = (byData & 0x01) << 2;
    uint8 chr_bank = ((byData & 0x70) >> 4) << 3;

    ROMBANK0 = ROM_pages[prg_bank % num_8k_ROM_pages];
    ROMBANK1 = ROM_pages[(prg_bank + 1) % num_8k_ROM_pages];
    ROMBANK2 = ROM_pages[(prg_bank + 2) % num_8k_ROM_pages];
    ROMBANK3 = ROM_pages[(prg_bank + 3) % num_8k_ROM_pages];

	for ( int nPage = 0; nPage < 8; ++nPage ) {
		PPUBANK[ nPage ] = VROM_pages[ (chr_bank + nPage) % num_1k_VROM_pages ];
    }
}
