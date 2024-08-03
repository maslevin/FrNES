#include "Mapper_67.h"

uint8 Mapper_67_irq_enabled;
uint8 Mapper_67_irq_counter;
uint8 Mapper_67_irq_latch;

void Mapper_67_Init() {
    // set CPU bank pointers
    ROMBANK0 = ROMPAGE(0);
    ROMBANK1 = ROMPAGE(1);
    ROMBANK2 = ROMLASTPAGE(1);
    ROMBANK3 = ROMLASTPAGE(0);

    // set PPU bank pointers
    uint32 num_1k_VROM_banks = NesHeader.byVRomSize * 8;
    PPUBANK[0] = &VROM[ 0 * 0x400 ];
    PPUBANK[1] = &VROM[ 1 * 0x400 ];
    PPUBANK[2] = &VROM[ 2 * 0x400 ];
    PPUBANK[3] = &VROM[ 3 * 0x400 ];
    PPUBANK[4] = &VROM[ (num_1k_VROM_banks - 4) * 0x400 ];
    PPUBANK[5] = &VROM[ (num_1k_VROM_banks - 3) * 0x400 ];
    PPUBANK[6] = &VROM[ (num_1k_VROM_banks - 2) * 0x400 ];
    PPUBANK[7] = &VROM[ (num_1k_VROM_banks - 1) * 0x400 ];

    Mapper_67_irq_enabled = 0;
    Mapper_67_irq_counter = 0;
    Mapper_67_irq_latch = 0;
}

void Mapper_67_Write(uint16 addr, uint8 data) {
    switch (addr & 0xF800) {
        case 0x8800: {
            PPUBANK[0] = &VROM[ (data * 2) * 0x400 ];
            PPUBANK[1] = &VROM[ ((data * 2) + 1) * 0x400 ];
        } break;

        case 0x9800: {
            PPUBANK[2] = &VROM[ (data * 2) * 0x400 ];
            PPUBANK[3] = &VROM[ ((data * 2) + 1) * 0x400 ];
        } break;

        case 0xA800: {
            PPUBANK[4] = &VROM[ (data * 2) * 0x400 ];
            PPUBANK[5] = &VROM[ ((data * 2) + 1) * 0x400 ];
        } break;

        case 0xB800: {
            PPUBANK[6] = &VROM[ (data * 2) * 0x400 ];
            PPUBANK[7] = &VROM[ ((data * 2) + 1) * 0x400 ];
        } break;

        case 0xC800: {
            Mapper_67_irq_counter = Mapper_67_irq_latch;
            Mapper_67_irq_latch = data;
        } break;

        case 0xD800: {
            Mapper_67_irq_enabled = data & 0x10;
        } break;

        case 0xE800: {
            data &= 0x03;
            if (data == 0) {
                pNesX_Mirroring(MIRRORING_VERTICAL);
            } else if (data == 1) {
                pNesX_Mirroring(MIRRORING_HORIZONTAL);
            } else if(data == 2) {
                pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_LOW);
            } else {
                pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_HIGH);
            }
        } break;

        case 0xF800: {
            uint32 num_8k_ROM_banks = NesHeader.byRomSize * 2;            
            //printf("Setting configurable ROM banks to [%lu] and [%lu]\n", ((data & 0xF) * 2) % num_8k_ROM_banks, (((data & 0xF) * 2) + 1) % num_8k_ROM_banks);
            ROMBANK0 = ROMPAGE(((data & 0xF) * 2) % num_8k_ROM_banks);
            ROMBANK1 = ROMPAGE((((data & 0xF) * 2) + 1) % num_8k_ROM_banks);
        } break;
    }
}

void Mapper_67_HSync() {
    if (Mapper_67_irq_enabled) {
        if ((ppuinfo.PPU_Scanline >= 0) && (ppuinfo.PPU_Scanline <= 239)) {
            if (PPU_R1 & (R1_SHOW_SCR | R1_SHOW_SP )) {
                if (--Mapper_67_irq_counter == 0xF6) {
                    Mapper_67_irq_counter = Mapper_67_irq_latch;
                    IRQ_REQ;
                }
            }
        }
    }
}