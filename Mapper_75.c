#include "Mapper_75.h"

uint8 Mapper_75_regs[2];

void Mapper_75_Init() {
    ROMBANK0 = ROMPAGE( 0 );
	ROMBANK1 = ROMPAGE( 1 );
	ROMBANK2 = ROMLASTPAGE( 1 );
	ROMBANK3 = ROMLASTPAGE( 0 );

    if ( NesHeader.byVRomSize > 0 ) {
        for ( int nPage = 0; nPage < 8; ++nPage )
            PPUBANK[ nPage ] = &VROM[ nPage * 0x400 ];
    }

    Mapper_75_regs[0] = 0;
    Mapper_75_regs[1] = 1;
}

void Mapper_75_Write( uint16 wAddr, unsigned char byData ) {
    switch(wAddr & 0xF000) {
        case 0x8000: {
            ROMBANK0 = ROMPAGE(byData);
        } break;

        case 0x9000: {
            if (byData & 0x01) {
                pNesX_Mirroring(MIRRORING_HORIZONTAL);
            } else {
                pNesX_Mirroring(MIRRORING_VERTICAL);
            }
            Mapper_75_regs[0] = (Mapper_75_regs[0] & 0x0F) | ((byData & 0x02) << 3);
            PPUBANK[0] = &VROM[ (Mapper_75_regs[0] * 4) * 0x400 ];
            PPUBANK[1] = &VROM[ ((Mapper_75_regs[0] * 4) + 1) * 0x400 ];
            PPUBANK[2] = &VROM[ ((Mapper_75_regs[0] * 4) + 2) * 0x400 ];
            PPUBANK[3] = &VROM[ ((Mapper_75_regs[0] * 4) + 3) * 0x400 ];
            Mapper_75_regs[1] = (Mapper_75_regs[1] & 0x0F) | ((byData & 0x04) << 2);
            PPUBANK[4] = &VROM[ (Mapper_75_regs[1] * 4) * 0x400 ];
            PPUBANK[5] = &VROM[ ((Mapper_75_regs[1] * 4) + 1) * 0x400 ];
            PPUBANK[6] = &VROM[ ((Mapper_75_regs[1] * 4) + 2) * 0x400 ];
            PPUBANK[7] = &VROM[ ((Mapper_75_regs[1] * 4) + 3) * 0x400 ];
        } break;

        case 0xA000: {
            ROMBANK1 = ROMPAGE(byData);
        } break;

        case 0xC000: {
            ROMBANK2 = ROMPAGE(byData);
        } break;

        case 0xE000: {
            Mapper_75_regs[0] = (Mapper_75_regs[0] & 0x10) | (byData & 0x0F);
            PPUBANK[0] = &VROM[ (Mapper_75_regs[0] * 4) * 0x400 ];
            PPUBANK[1] = &VROM[ ((Mapper_75_regs[0] * 4) + 1) * 0x400 ];
            PPUBANK[2] = &VROM[ ((Mapper_75_regs[0] * 4) + 2) * 0x400 ];
            PPUBANK[3] = &VROM[ ((Mapper_75_regs[0] * 4) + 3) * 0x400 ];
        } break;

        case 0xF000: {
            Mapper_75_regs[1] = (Mapper_75_regs[1] & 0x10) | (byData & 0x0F);
            PPUBANK[4] = &VROM[ (Mapper_75_regs[1] * 4) * 0x400 ];
            PPUBANK[5] = &VROM[ ((Mapper_75_regs[1] * 4) + 1) * 0x400 ];
            PPUBANK[6] = &VROM[ ((Mapper_75_regs[1] * 4) + 2) * 0x400 ];
            PPUBANK[7] = &VROM[ ((Mapper_75_regs[1] * 4) + 3) * 0x400 ];
        } break;
    }
}
