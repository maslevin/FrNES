#include "Mapper_68.h"

uint8 Mapper_68_regs[4];

void Mapper_68_Init() {

    ROMBANK0 = ROMPAGE( 0 );
	ROMBANK1 = ROMPAGE( 1 );
	ROMBANK2 = ROMLASTPAGE( 1 );
	ROMBANK3 = ROMLASTPAGE( 0 );

    Mapper_68_regs[0] = 0;
    Mapper_68_regs[1] = 0;
    Mapper_68_regs[2] = 0;
    Mapper_68_regs[3] = 0;
}

void SyncMirror() {
    if (Mapper_68_regs[0]) {
        if (Mapper_68_regs[1] == 0) {
            PPUBANK[8] = &VROM[ (Mapper_68_regs[2] + 0x80) * 0x400 ];
            PPUBANK[9] = &VROM[ (Mapper_68_regs[3] + 0x80) * 0x400 ];
            PPUBANK[10] = &VROM[ (Mapper_68_regs[2] + 0x80) * 0x400 ];
            PPUBANK[11] = &VROM[ (Mapper_68_regs[3] + 0x80) * 0x400 ];
        } else if (Mapper_68_regs[1] == 1) {
            PPUBANK[8] = &VROM[ (Mapper_68_regs[2] + 0x80) * 0x400 ];
            PPUBANK[9] = &VROM[ (Mapper_68_regs[2] + 0x80) * 0x400 ];
            PPUBANK[10] = &VROM[ (Mapper_68_regs[3] + 0x80) * 0x400 ];
            PPUBANK[11] = &VROM[ (Mapper_68_regs[3] + 0x80) * 0x400 ];
        } else if (Mapper_68_regs[1] == 2) {
            PPUBANK[8] = &VROM[ (Mapper_68_regs[2] + 0x80) * 0x400 ];
            PPUBANK[9] = &VROM[ (Mapper_68_regs[2] + 0x80) * 0x400 ];
            PPUBANK[10] = &VROM[ (Mapper_68_regs[2] + 0x80) * 0x400 ];
            PPUBANK[11] = &VROM[ (Mapper_68_regs[2] + 0x80) * 0x400 ];
        } else if (Mapper_68_regs[1] == 3) {
            PPUBANK[8] = &VROM[ (Mapper_68_regs[3] + 0x80) * 0x400 ];
            PPUBANK[9] = &VROM[ (Mapper_68_regs[3] + 0x80) * 0x400 ];
            PPUBANK[10] = &VROM[ (Mapper_68_regs[3] + 0x80) * 0x400 ];
            PPUBANK[11] = &VROM[ (Mapper_68_regs[3] + 0x80) * 0x400 ];
        }
    } else {
        if (Mapper_68_regs[1] == 0) {
            pNesX_Mirroring(MIRRORING_VERTICAL);
        } else if (Mapper_68_regs[1] == 1) {
            pNesX_Mirroring(MIRRORING_HORIZONTAL);
        } else if (Mapper_68_regs[1] == 2) {
            pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_LOW);
        } else if (Mapper_68_regs[1] == 3) {
            pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_HIGH);
        }
    }    
}

void Mapper_68_Write( uint16 wAddr, unsigned char byData ) {
    switch(wAddr & 0xF000) {
        case 0x8000: {
            PPUBANK[0] = &VROM[ (byData * 2) * 0x400 ];
            PPUBANK[1] = &VROM[ ((byData * 2) + 1) * 0x400 ];
        } break;

        case 0x9000: {
            PPUBANK[2] = &VROM[ (byData * 2) * 0x400 ];
            PPUBANK[3] = &VROM[ ((byData * 2) + 1) * 0x400 ];
        } break;

        case 0xA000: {
            PPUBANK[4] = &VROM[ (byData * 2) * 0x400 ];
            PPUBANK[5] = &VROM[ ((byData * 2) + 1) * 0x400 ];
        } break;

        case 0xB000: {
            PPUBANK[6] = &VROM[ (byData * 2) * 0x400 ];
            PPUBANK[7] = &VROM[ ((byData * 2) + 1) * 0x400 ];
        } break;

        case 0xC000: {
            Mapper_68_regs[2] = byData;
            SyncMirror();
        } break;

        case 0xD000: {
            Mapper_68_regs[3] = byData;
            SyncMirror();
        } break;

        case 0xE000: {
            Mapper_68_regs[0] = (byData & 0x10) >> 4;
            Mapper_68_regs[1] = byData & 0x03;
            SyncMirror();
        } break;

        case 0xF000: {
            ROMBANK0 = ROMPAGE((byData * 2));
            ROMBANK1 = ROMPAGE((byData * 2) + 1);
        } break;
    }
}