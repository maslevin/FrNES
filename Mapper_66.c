#include "Mapper_66.h"

void Mapper_66_Init() {
    BankTable[4] = ROM_pages[0];
    BankTable[5] = ROM_pages[1];
	BankTable[6] = ROM_pages[2];
	BankTable[7] = ROM_pages[3]; 

    for ( int nPage = 0; nPage < 8; ++nPage )
        PPUBANK[ nPage ] = VROM_pages[nPage];

    pNesX_Mirroring(MIRRORING_VERTICAL);
}

void Mapper_66_Write( uint16 wAddr, unsigned char byData ) {
    if (wAddr & 0x8000) {
        uint8 prg_bank = ((byData & 0xF0) >> 4) << 2;
        uint8 chr_bank = (byData & 0x0F) << 3;

        BankTable[4] = ROM_pages[ prg_bank % num_8k_ROM_pages ];
        BankTable[5] = ROM_pages[ (prg_bank + 1) % num_8k_ROM_pages ];
        BankTable[6] = ROM_pages[ (prg_bank + 2) % num_8k_ROM_pages ];
        BankTable[7] = ROM_pages[ (prg_bank + 3) % num_8k_ROM_pages ];

        for ( int nPage = 0; nPage < 8; ++nPage ) {
            PPUBANK[ nPage ] = VROM_pages[ (chr_bank + nPage) % num_1k_VROM_pages ];
        }
    }
}
