#include "Mapper_67.h"

uint8 Mapper_67_irq_enabled;
uint8 Mapper_67_irq_counter;
uint8 Mapper_67_irq_latch;

void Mapper_67_Init() {
    ROMBANK0 = ROM_pages[0];
    ROMBANK1 = ROM_pages[1];
    ROMBANK2 = ROM_pages[ num_8k_ROM_pages - 2];
    ROMBANK3 = ROM_pages[ num_8k_ROM_pages - 1];

    PPUBANK[0] = VROM_pages[0];
    PPUBANK[1] = VROM_pages[1];
    PPUBANK[2] = VROM_pages[2];
    PPUBANK[3] = VROM_pages[3];
    PPUBANK[4] = VROM_pages[num_1k_VROM_pages - 4];
    PPUBANK[5] = VROM_pages[num_1k_VROM_pages - 3];
    PPUBANK[6] = VROM_pages[num_1k_VROM_pages - 2];
    PPUBANK[7] = VROM_pages[num_1k_VROM_pages - 1];

    Mapper_67_irq_enabled = 0;
    Mapper_67_irq_counter = 0;
    Mapper_67_irq_latch = 0;
}

void Mapper_67_Write(uint16 addr, uint8 data) {
    switch (addr & 0xF800) {
        case 0x8800: {
            PPUBANK[0] = VROM_pages[ (data * 2) ];
            PPUBANK[1] = VROM_pages[ ((data * 2) + 1) ];
        } break;

        case 0x9800: {
            PPUBANK[2] = VROM_pages[ (data * 2) ];
            PPUBANK[3] = VROM_pages[ ((data * 2) + 1) ];
        } break;

        case 0xA800: {
            PPUBANK[4] = VROM_pages[ (data * 2) ];
            PPUBANK[5] = VROM_pages[ ((data * 2) + 1) ];
        } break;

        case 0xB800: {
            PPUBANK[6] = VROM_pages[ (data * 2) ];
            PPUBANK[7] = VROM_pages[ ((data * 2) + 1) ];
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
            ROMBANK0 = ROM_pages[ (data << 1) % num_8k_ROM_pages ];
            ROMBANK1 = ROM_pages[ ((data << 1) + 1) % num_8k_ROM_pages ];
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