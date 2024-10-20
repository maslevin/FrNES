#include "Mapper_26.h"

#include "nes_apu.h"

unsigned char Mapper_26_irq_enabled;
unsigned char Mapper_26_irq_counter;
unsigned char Mapper_26_irq_latch;

void Mapper_26_Init() {
    apu_set_exsound(NES_APU_EXSOUND_VRC6);

    BankTable[4] = ROM_pages[0];
    BankTable[5] = ROM_pages[1];
    BankTable[6] = ROM_pages[ num_8k_ROM_pages - 2];
    BankTable[7] = ROM_pages[ num_8k_ROM_pages - 1];

    for ( int nPage = 0; nPage < 8; ++nPage )
        PPUBANK[ nPage ] = VROM_pages[ nPage ];

    Mapper_26_irq_enabled = 0;
    Mapper_26_irq_counter = 0;
}

void Mapper_26_Write(uint16 addr, unsigned char data) {
    switch(addr) {
        case 0x8000: {
            BankTable[4] = ROM_pages[data << 1];
            BankTable[5] = ROM_pages[(data << 1) + 1];
        } break;

        case 0xB003: {
            data = data & 0x7F;
            if (data == 0x08 || data == 0x2C) {
                pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_HIGH);
            } else if(data == 0x20) {
                pNesX_Mirroring(MIRRORING_VERTICAL);
            } else if(data == 0x24) {
                pNesX_Mirroring(MIRRORING_HORIZONTAL);
            } else if(data == 0x28) {
                pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_LOW);
            }
        } break;

        case 0xC000: {
            BankTable[6] = ROM_pages[data];
        } break;

        case 0xD000: {
            PPUBANK[0] = VROM_pages[data];
        } break;

        case 0xD001: {
            PPUBANK[2] = VROM_pages[data];
        } break;

        case 0xD002: {
            PPUBANK[1] = VROM_pages[data];
        } break;

        case 0xD003: {
            PPUBANK[3] = VROM_pages[data];
        } break;

        case 0xE000: {
            PPUBANK[4] = VROM_pages[data];
        } break;

        case 0xE001: {
            PPUBANK[6] = VROM_pages[data];
        } break;

        case 0xE002: {
            PPUBANK[5] = VROM_pages[data];           
        } break;

        case 0xE003: {
            PPUBANK[7] = VROM_pages[data];           
        } break;

        case 0xF000: {
            Mapper_26_irq_latch = data;
        } break;

        case 0xF001: {
            Mapper_26_irq_enabled = data & 0x01;
        } break;

        case 0xF002: {
            Mapper_26_irq_enabled = data & 0x03;
            if (Mapper_26_irq_enabled & 0x02) {
                Mapper_26_irq_counter = Mapper_26_irq_latch;
            }
        } break;
    }

    addr = (addr & 0xfffc) | ((addr & 1) << 1) | ((addr & 2) >> 1);
    ex_write(addr, data);
}

void Mapper_26_HSync() {
    if (Mapper_26_irq_enabled & 0x03) {
        if(Mapper_26_irq_counter >= 0xFE) {
            set_irq(true);
            Mapper_26_irq_counter = Mapper_26_irq_latch;
            Mapper_26_irq_enabled = 0;
        } else {
            Mapper_26_irq_counter++;
        }
    }
}