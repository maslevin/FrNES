#include "Mapper_22.h"

void Mapper_22_Init() {
    ROMBANK0 = ROMPAGE(0);
    ROMBANK1 = ROMPAGE(1);
    ROMBANK2 = ROMLASTPAGE(1);
    ROMBANK3 = ROMLASTPAGE(0);

    for ( int nPage = 0; nPage < 8; ++nPage )
        PPUBANK[ nPage ] = &VROM[ nPage * 0x400 ];
}

void Mapper_22_Write(uint16 addr, uint8 data) {
    switch (addr) {
        case 0x8000: {
            uint32 num_8k_ROM_banks = NesHeader.byRomSize * 2;
            ROMBANK0 = ROMPAGE(data % num_8k_ROM_banks);
        } break;

        case 0x9000: {
            data &= 0x03;
            if (data == 0) {
                pNesX_Mirroring(MIRRORING_VERTICAL);
            } else if(data == 1) {
                pNesX_Mirroring(MIRRORING_HORIZONTAL);
            } else if(data == 2) {
                pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_HIGH);
            } else {
                pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_LOW);
            }
        } break;

        case 0xA000: {
            uint32 num_8k_ROM_banks = NesHeader.byRomSize * 2;             
            ROMBANK1 = ROMPAGE(data % num_8k_ROM_banks);
        } break;

        case 0xB000: {
            PPUBANK[0] = &VROM[(data >> 1) * 0x400];
        } break;

        case 0xB001: {
            PPUBANK[1] = &VROM[(data >> 1) * 0x400];
        } break;

        case 0xC000: {
            PPUBANK[2] = &VROM[(data >> 1) * 0x400];
        } break;

        case 0xC001: {
            PPUBANK[3] = &VROM[(data >> 1) * 0x400];
        } break;

        case 0xD000: {
            PPUBANK[4] = &VROM[(data >> 1) * 0x400];
        } break;

        case 0xD001: {
            PPUBANK[5] = &VROM[(data >> 1) * 0x400];
        } break;

        case 0xE000: {
            PPUBANK[6] = &VROM[(data >> 1) * 0x400];
        } break;

        case 0xE001: {
            PPUBANK[7] = &VROM[(data >> 1) * 0x400];
        } break;
    }
}