#include "Mapper_66.h"

void Mapper_66_Init() {
    ROMBANK0 = ROM_pages[0];
    ROMBANK1 = ROM_pages[1];
	ROMBANK2 = ROM_pages[2];
	ROMBANK3 = ROM_pages[3]; 

    for ( int nPage = 0; nPage < 8; ++nPage )
        PPUBANK[ nPage ] = VROM_pages[nPage];

    pNesX_Mirroring(MIRRORING_VERTICAL);
}

void Mapper_66_Write( uint16 wAddr, unsigned char byData ) {
    if (wAddr & 0x8000) {
        uint8 prg_bank = ((byData & 0xF0) >> 4) << 2;
        uint8 chr_bank = (byData & 0x0F) << 3;

        ROMBANK0 = ROM_pages[ prg_bank % num_8k_ROM_pages ];
        ROMBANK1 = ROM_pages[ (prg_bank + 1) % num_8k_ROM_pages ];
        ROMBANK2 = ROM_pages[ (prg_bank + 2) % num_8k_ROM_pages ];
        ROMBANK3 = ROM_pages[ (prg_bank + 3) % num_8k_ROM_pages ];

        for ( int nPage = 0; nPage < 8; ++nPage ) {
            PPUBANK[ nPage ] = VROM_pages[ (chr_bank + nPage) % num_1k_VROM_pages ];
        }
    }
}
