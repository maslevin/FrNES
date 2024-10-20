#include "Mapper_34.h"

void Mapper_34_Init() {
    BankTable[4] = ROM_pages[0];
    BankTable[5] = ROM_pages[1];
	BankTable[6] = ROM_pages[2];
	BankTable[7] = ROM_pages[3];          

    for ( int nPage = 0; nPage < 8; ++nPage )
        PPUBANK[ nPage ] = VROM_pages[nPage];
}

void Mapper_34_Write( uint16 wAddr, unsigned char byData ) {
    switch(wAddr) {
        case 0x7FFE: {
            uint8 basePage = byData << 2;
            PPUBANK[0] = VROM_pages[ basePage ];
            PPUBANK[1] = VROM_pages[ basePage + 1 ];
            PPUBANK[2] = VROM_pages[ basePage + 2 ]; 
            PPUBANK[3] = VROM_pages[ basePage + 3 ];
        } break;

        case 0x7FFF: {
            uint8 basePage = byData << 2;            
            PPUBANK[4] = VROM_pages[ basePage ];
            PPUBANK[5] = VROM_pages[ basePage + 1];
            PPUBANK[6] = VROM_pages[ basePage + 2];
            PPUBANK[7] = VROM_pages[ basePage + 3];
        } break;

        case 0x7FFD:
        default: {
            uint8 basePage = ((byData << 2) % num_8k_ROM_pages);
            BankTable[4] = ROM_pages[ basePage ];
            BankTable[5] = ROM_pages[ basePage + 1];
            BankTable[6] = ROM_pages[ basePage + 2];
            BankTable[7] = ROM_pages[ basePage + 3];          
        } break;
    }
}