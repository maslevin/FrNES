#include "Mapper_66.h"

void Mapper_66_Init() {
    ROMBANK0 = ROMPAGE( 0 );
	ROMBANK1 = ROMPAGE( 1 );
	ROMBANK2 = ROMPAGE( 2 );
	ROMBANK3 = ROMPAGE( 3 );

	/* Set PPU Banks */
	for ( int nPage = 0; nPage < 8; ++nPage )
		PPUBANK[ nPage ] = &VROM[ nPage * 0x400 ];

    pNesX_Mirroring(MIRRORING_VERTICAL);
}

void Mapper_66_Write( uint16 wAddr, unsigned char byData ) {
    uint8 prg_bank = (byData & 0xF0) >> 4;
    uint8 chr_bank = byData & 0x0F;
    uint32 num_8k_ROM_banks = NesHeader.byRomSize * 2;   
    uint32 num_1k_VROM_banks = NesHeader.byVRomSize * 8;

    ROMBANK0 = ROMPAGE((prg_bank * 4) % num_8k_ROM_banks);
    ROMBANK1 = ROMPAGE(((prg_bank * 4) + 1) % num_8k_ROM_banks);
    ROMBANK2 = ROMPAGE(((prg_bank * 4) + 2) % num_8k_ROM_banks);
    ROMBANK3 = ROMPAGE(((prg_bank * 4) + 3) % num_8k_ROM_banks);

	for ( int nPage = 0; nPage < 8; ++nPage ) {
		PPUBANK[ nPage ] = &VROM[ (((chr_bank * 8) + nPage) % num_1k_VROM_banks) * 0x400 ];
    }
}
