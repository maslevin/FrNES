#include "Mapper_22.h"

void Mapper_22_Init() {
    BankTable[4] = ROM_pages[0];
    BankTable[5] = ROM_pages[1];
    BankTable[6] = ROM_pages[ num_8k_ROM_pages - 2];
    BankTable[7] = ROM_pages[ num_8k_ROM_pages - 1];

    for ( int nPage = 0; nPage < 8; ++nPage )
        PPUBANK[ nPage ] = VROM_pages[nPage];
}

void Mapper_22_Write(uint16 addr, uint8 data) {
    switch (addr) {
        case 0x8000: {
            BankTable[4] = ROM_pages[data % num_8k_ROM_pages];
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
            BankTable[5] = ROM_pages[data % num_8k_ROM_pages];
        } break;

        case 0xB000: {
            PPUBANK[0] = VROM_pages[data >> 1];
        } break;

        case 0xB001: {
            PPUBANK[1] = VROM_pages[data >> 1];
        } break;

        case 0xC000: {
            PPUBANK[2] = VROM_pages[data >> 1];
        } break;

        case 0xC001: {
            PPUBANK[3] = VROM_pages[data >> 1];
        } break;

        case 0xD000: {
            PPUBANK[4] = VROM_pages[data >> 1];
        } break;

        case 0xD001: {
            PPUBANK[5] = VROM_pages[data >> 1];
        } break;

        case 0xE000: {
            PPUBANK[6] = VROM_pages[data >> 1];
        } break;

        case 0xE001: {
            PPUBANK[7] = VROM_pages[data >> 1];
        } break;
    }
}