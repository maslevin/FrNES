#include "Mapper_85.h"

#include "nes_apu.h"

unsigned char Mapper_85_irq_enabled;
unsigned char Mapper_85_irq_counter;
unsigned char Mapper_85_irq_latch;

void Mapper_85_Init() {
    apu_set_exsound(NES_APU_EXSOUND_VRC7);

	BankTable[4] = ROM_pages[0];
	BankTable[5] = ROM_pages[1];
    BankTable[6] = ROM_pages[ num_8k_ROM_pages - 2];
    BankTable[7] = ROM_pages[ num_8k_ROM_pages - 1];

    for ( int nPage = 0; nPage < 8; ++nPage )
        PPUBANK[ nPage ] = VROM_pages[ nPage ];

    Mapper_85_irq_enabled = 0;
    Mapper_85_irq_counter = 0;
    Mapper_85_irq_latch = 0;
}

void Mapper_85_Write(uint16 addr, uint8 data) {
    switch(addr & 0xF038) {
        case 0x8000: {
            BankTable[4] = ROM_pages[data];
        } break;

        case 0x8008:
        case 0x8010: {
            BankTable[5] = ROM_pages[data];
        } break;

        case 0x9000: {
            BankTable[6] = ROM_pages[data];
        }
        break;

        case 0x9010:
        case 0x9030: {
            ex_write(addr, data);
        } break;

        case 0xA000: {
            PPUBANK[ 0 ] = VROM_pages[ data ];
        } break;

        case 0xA008:
        case 0xA010: {
            PPUBANK[ 1 ] = VROM_pages[ data ];
        } break;

        case 0xB000: {
            PPUBANK[ 2 ] = VROM_pages[ data ];
        } break;

        case 0xB008:
        case 0xB010: {
            PPUBANK[ 3 ] = VROM_pages[ data ];
        } break;

        case 0xC000: {
            PPUBANK[ 4 ] = VROM_pages[ data ];
        } break;

        case 0xC008:
        case 0xC010: {
            PPUBANK[ 5 ] = VROM_pages[ data ];
        } break;

        case 0xD000: {
            PPUBANK[ 6 ] = VROM_pages[ data ];
        } break;

        case 0xD008:
        case 0xD010: {
            PPUBANK[ 7 ] = VROM_pages[ data ];
        } break;

        case 0xE000: {
            data &= 0x03;
            if (data == 0x00) {
                pNesX_Mirroring(MIRRORING_VERTICAL);
            } else if(data == 0x01) {
                pNesX_Mirroring(MIRRORING_HORIZONTAL);
            } else if(data == 0x02) {
                pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_LOW);
            } else {
                pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_HIGH);
            }
        } break;

        case 0xE008:
        case 0xE010: {
            Mapper_85_irq_latch = data;
        } break;

        case 0xF000: {
            Mapper_85_irq_enabled = data & 0x03;
            if (Mapper_85_irq_enabled & 0x02) {
                Mapper_85_irq_counter = Mapper_85_irq_latch;
            }
        } break;

        case 0xF008:
        case 0xF010: {
            Mapper_85_irq_enabled = (Mapper_85_irq_enabled & 0x01) * 3;
        } break;
    }
}

void Mapper_85_HSync() {
    if (Mapper_85_irq_enabled & 0x02) {
        if (Mapper_85_irq_counter == 0xFF) {
            set_irq(true);
            Mapper_85_irq_counter = Mapper_85_irq_latch;
        } else {
            Mapper_85_irq_counter++;
        }
    }
}

