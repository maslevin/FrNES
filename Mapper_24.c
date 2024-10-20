#include "Mapper_24.h"

#include "nes_apu.h"

unsigned char Mapper_24_irq_enabled_l;
unsigned char Mapper_24_irq_enabled_h;
unsigned char Mapper_24_irq_counter;
unsigned char Mapper_24_irq_latch;

void Mapper_24_Init() {
    apu_set_exsound(NES_APU_EXSOUND_VRC6);

    BankTable[4] = ROM_pages[0];
    BankTable[5] = ROM_pages[1];
    BankTable[6] = ROM_pages[ num_8k_ROM_pages - 2];
    BankTable[7] = ROM_pages[ num_8k_ROM_pages - 1];

    for ( int nPage = 0; nPage < 8; ++nPage )
        PPUBANK[ nPage ] = VROM_pages[nPage];

    Mapper_24_irq_enabled_l = 0;
    Mapper_24_irq_enabled_h = 0;
    Mapper_24_irq_counter = 0;
    Mapper_24_irq_latch = 0;
}

void Mapper_24_Write(uint16 addr, unsigned char data) {
    switch(addr) {
        case 0x8000: {
            BankTable[4] = ROM_pages[data << 1];
            BankTable[5] = ROM_pages[(data << 1) + 1];
        } break;

        case 0xB003: {
            data &= 0x0C;
            if(data == 0x00) {
                pNesX_Mirroring(MIRRORING_VERTICAL);
            } else if(data == 0x04) {
                pNesX_Mirroring(MIRRORING_HORIZONTAL);
            } else if(data == 0x08) {
                pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_LOW);
            } else if(data == 0x0C) {
                pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_HIGH);                
            }
        } break;

        case 0xC000: {
            BankTable[6] = ROM_pages[data];
        } break;

        case 0xD000 ... 0xD003: {
            PPUBANK[(addr & 0xf003) - 0xd000] = VROM_pages[data];
        } break;

        case 0xE000 ... 0xE003: {
            PPUBANK[(addr & 0xf003) - 0xe000 + 4] = VROM_pages[data];
        } break;

        case 0xF000: {
            Mapper_24_irq_latch = data;
        } break;

        case 0xF001: {
            Mapper_24_irq_enabled_l = data & 0x1;
            Mapper_24_irq_enabled_h = data & 0x2;
            if (Mapper_24_irq_enabled_h) {
                Mapper_24_irq_counter = Mapper_24_irq_latch;
            }
        } break;

        case 0xF002: {
            Mapper_24_irq_enabled_h = Mapper_24_irq_enabled_l;
        } break;
    }

    ex_write(addr, data);
}

void Mapper_24_HSync() {
    if (Mapper_24_irq_enabled_h) {
        if (Mapper_24_irq_counter == 0xff) {
            set_irq(true);
            Mapper_24_irq_counter = Mapper_24_irq_latch;
        } else {
            Mapper_24_irq_counter++;
        }
    }
}