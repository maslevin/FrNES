#include "Mapper_68.h"

uint8 Mapper_68_regs[4];

void Mapper_68_Init() {
	ROMBANK0 = ROM_pages[0];
	ROMBANK1 = ROM_pages[1];
    ROMBANK2 = ROM_pages[ num_8k_ROM_pages - 2];
    ROMBANK3 = ROM_pages[ num_8k_ROM_pages - 1];

    Mapper_68_regs[0] = 0;
    Mapper_68_regs[1] = 0;
    Mapper_68_regs[2] = 0;
    Mapper_68_regs[3] = 0;
}

void SyncMirror() {
    if (Mapper_68_regs[0]) {
        if (Mapper_68_regs[1] == 0) {
            PPUBANK[8] = VROM_pages[(Mapper_68_regs[2] + 0x80)];
            PPUBANK[9] = VROM_pages[(Mapper_68_regs[3] + 0x80)];
            PPUBANK[10] = VROM_pages[(Mapper_68_regs[2] + 0x80)];
            PPUBANK[11] = VROM_pages[(Mapper_68_regs[3] + 0x80)];
        } else if (Mapper_68_regs[1] == 1) {
            PPUBANK[8] = VROM_pages[(Mapper_68_regs[2] + 0x80)];
            PPUBANK[9] = VROM_pages[(Mapper_68_regs[2] + 0x80)];
            PPUBANK[10] = VROM_pages[(Mapper_68_regs[3] + 0x80)];
            PPUBANK[11] = VROM_pages[(Mapper_68_regs[3] + 0x80)];
        } else if (Mapper_68_regs[1] == 2) {
            PPUBANK[8] = VROM_pages[(Mapper_68_regs[2] + 0x80)];
            PPUBANK[9] = VROM_pages[(Mapper_68_regs[2] + 0x80)];
            PPUBANK[10] = VROM_pages[(Mapper_68_regs[2] + 0x80)];
            PPUBANK[11] = VROM_pages[(Mapper_68_regs[2] + 0x80)];
        } else if (Mapper_68_regs[1] == 3) {
            PPUBANK[8] = VROM_pages[(Mapper_68_regs[3] + 0x80)];
            PPUBANK[9] = VROM_pages[(Mapper_68_regs[3] + 0x80)];
            PPUBANK[10] = VROM_pages[(Mapper_68_regs[3] + 0x80)];
            PPUBANK[11] = VROM_pages[(Mapper_68_regs[3] + 0x80)];
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
            PPUBANK[0] = VROM_pages[ (byData << 1) ];
            PPUBANK[1] = VROM_pages[ (byData << 1) + 1];
        } break;

        case 0x9000: {
            PPUBANK[2] = VROM_pages[ (byData << 1) ];
            PPUBANK[3] = VROM_pages[ (byData << 1) + 1 ];
        } break;

        case 0xA000: {
            PPUBANK[4] = VROM_pages[ (byData << 1) ];
            PPUBANK[5] = VROM_pages[ (byData << 1) + 1 ];
        } break;

        case 0xB000: {
            PPUBANK[6] = VROM_pages[ (byData << 1) ];
            PPUBANK[7] = VROM_pages[ (byData << 1) + 1 ];
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
            ROMBANK0 = ROM_pages[ (byData << 1) ];
            ROMBANK1 = ROM_pages[ (byData << 1) + 1 ];
        } break;
    }
}