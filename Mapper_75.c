#include "Mapper_75.h"

uint8 Mapper_75_regs[2];

void Mapper_75_Init() {
	BankTable[4] = ROM_pages[0];
	BankTable[5] = ROM_pages[1];
    BankTable[6] = ROM_pages[ num_8k_ROM_pages - 2];
    BankTable[7] = ROM_pages[ num_8k_ROM_pages - 1];

    for ( int nPage = 0; nPage < 8; ++nPage )
        PPUBANK[ nPage ] = VROM_pages[ nPage ];

    Mapper_75_regs[0] = 0;
    Mapper_75_regs[1] = 1;
}

void Mapper_75_Write( uint16 wAddr, unsigned char byData ) {
    switch(wAddr & 0xF000) {
        case 0x8000: {
            BankTable[4] = ROM_pages[byData];
        } break;

        case 0x9000: {
            if (byData & 0x01) {
                pNesX_Mirroring(MIRRORING_HORIZONTAL);
            } else {
                pNesX_Mirroring(MIRRORING_VERTICAL);
            }
            Mapper_75_regs[0] = (Mapper_75_regs[0] & 0x0F) | ((byData & 0x02) << 3);
            uint8 base = Mapper_75_regs[0] << 2;
            PPUBANK[0] = VROM_pages[ base ];
            PPUBANK[1] = VROM_pages[ base + 1];
            PPUBANK[2] = VROM_pages[ base + 2];
            PPUBANK[3] = VROM_pages[ base + 3];
            Mapper_75_regs[1] = (Mapper_75_regs[1] & 0x0F) | ((byData & 0x04) << 2);
            base = Mapper_75_regs[1] << 2;            
            PPUBANK[4] = VROM_pages[ base ];
            PPUBANK[5] = VROM_pages[ base + 1];
            PPUBANK[6] = VROM_pages[ base + 2];
            PPUBANK[7] = VROM_pages[ base + 3];
        } break;

        case 0xA000: {
            BankTable[5] = ROM_pages[ byData ];
        } break;

        case 0xC000: {
            BankTable[6] = ROM_pages[ byData ];
        } break;

        case 0xE000: {
            Mapper_75_regs[0] = (Mapper_75_regs[0] & 0x10) | (byData & 0x0F);
            uint8 base = Mapper_75_regs[0] << 2;
            PPUBANK[0] = VROM_pages[ base ];
            PPUBANK[1] = VROM_pages[ base + 1];
            PPUBANK[2] = VROM_pages[ base + 2];
            PPUBANK[3] = VROM_pages[ base + 3];
        } break;

        case 0xF000: {
            Mapper_75_regs[1] = (Mapper_75_regs[1] & 0x10) | (byData & 0x0F);
            uint8 base = Mapper_75_regs[1] << 2;   
            PPUBANK[4] = VROM_pages[ base ];
            PPUBANK[5] = VROM_pages[ base + 1];
            PPUBANK[6] = VROM_pages[ base + 2];
            PPUBANK[7] = VROM_pages[ base + 3];
        } break;
    }
}
