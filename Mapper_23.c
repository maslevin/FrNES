#include "Mapper_23.h"

extern uint32 currentCRC32;

uint16 Mapper_23_patch;
uint8 Mapper_23_regs[9];
uint8 Mapper_23_irq_enabled;
uint8 Mapper_23_irq_counter;
uint8 Mapper_23_irq_latch;

void Mapper_23_Init() {

    // Akumajou Special - Boku Dracula Kun
    // ... need to look up the translation of this as well
    if ((currentCRC32 == 0x93794634) || 
        (currentCRC32 == 0xc1fbf659)) {
        Mapper_23_patch = 0xF00C;
    } else {
        Mapper_23_patch = 0xFFFF;
    }

    ROMBANK0 = ROM_pages[0];
    ROMBANK1 = ROM_pages[1];
    ROMBANK2 = ROM_pages[ num_8k_ROM_pages - 2];
    ROMBANK3 = ROM_pages[ num_8k_ROM_pages - 1];

    Mapper_23_regs[0] = 0;
    Mapper_23_regs[1] = 1;
    Mapper_23_regs[2] = 2;
    Mapper_23_regs[3] = 3;
    Mapper_23_regs[4] = 4;
    Mapper_23_regs[5] = 5;
    Mapper_23_regs[6] = 6;
    Mapper_23_regs[7] = 7;
    Mapper_23_regs[8] = 0;

    Mapper_23_irq_enabled = 0;
    Mapper_23_irq_counter = 0;
    Mapper_23_irq_latch = 0;
}

void Mapper_23_Write(uint16 addr, uint8 data) {
    switch (addr & Mapper_23_patch) {
        case 0x8000:
        case 0x8004:
        case 0x8008:
        case 0x800C: {             
            if (Mapper_23_regs[8]) {
                ROMBANK2 = ROM_pages[data % num_8k_ROM_pages];
            } else {
                ROMBANK0 = ROM_pages[data % num_8k_ROM_pages];
            }
        } break;

        case 0x9000: {
            if (data != 0xFF) {
                data &= 0x03;
                if (data == 0) {
                    pNesX_Mirroring(MIRRORING_VERTICAL);
                } else if(data == 1) {
                    pNesX_Mirroring(MIRRORING_HORIZONTAL);
                } else if(data == 2) {
                    pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_LOW);
                } else {
                    pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_HIGH);
                }
            }
        } break;

        case 0x9008: {            
            Mapper_23_regs[8] = data & 0x02;
        } break;

        case 0xA000:
        case 0xA004:
        case 0xA008:
        case 0xA00C: {            
            ROMBANK1 = ROM_pages[data % num_8k_ROM_pages];
        } break;

        case 0xB000: {
            Mapper_23_regs[0] = (Mapper_23_regs[0] & 0xF0) | (data & 0x0F);
            PPUBANK[0] = VROM_pages[Mapper_23_regs[0]];
        } break;

        case 0xB001:
        case 0xB004: {
            Mapper_23_regs[0] = (Mapper_23_regs[0] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[0] = VROM_pages[Mapper_23_regs[0]];
        } break;

        case 0xB002:
        case 0xB008: {
            Mapper_23_regs[1] = (Mapper_23_regs[1] & 0xF0) | (data & 0x0F);
            PPUBANK[1] = VROM_pages[Mapper_23_regs[1]];
        } break;

        case 0xB003:
        case 0xB00C: {
            Mapper_23_regs[1] = (Mapper_23_regs[1] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[1] = VROM_pages[Mapper_23_regs[1]];
        } break;

        case 0xC000: {
            Mapper_23_regs[2] = (Mapper_23_regs[2] & 0xF0) | (data & 0x0F);
            PPUBANK[2] = VROM_pages[Mapper_23_regs[2]];
        } break;

        case 0xC001:
        case 0xC004: {
            Mapper_23_regs[2] = (Mapper_23_regs[2] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[2] = VROM_pages[Mapper_23_regs[2]];
        } break;

        case 0xC002:
        case 0xC008: {
            Mapper_23_regs[3] = (Mapper_23_regs[3] & 0xF0) | (data & 0x0F);
            PPUBANK[3] = VROM_pages[Mapper_23_regs[3]];
        } break;

        case 0xC003:
        case 0xC00C: {
            Mapper_23_regs[3] = (Mapper_23_regs[3] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[3] = VROM_pages[Mapper_23_regs[3]];
        } break;

        case 0xD000: {
            Mapper_23_regs[4] = (Mapper_23_regs[4] & 0xF0) | (data & 0x0F);
            PPUBANK[4] = VROM_pages[Mapper_23_regs[4]];
        } break;

        case 0xD001:
        case 0xD004: {
            Mapper_23_regs[4] = (Mapper_23_regs[4] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[4] = VROM_pages[Mapper_23_regs[4]];
        } break;

        case 0xD002:
        case 0xD008: {
            Mapper_23_regs[5] = (Mapper_23_regs[5] & 0xF0) | (data & 0x0F);
            PPUBANK[5] = VROM_pages[Mapper_23_regs[5]];
        } break;

        case 0xD003:
        case 0xD00C: {
            Mapper_23_regs[5] = (Mapper_23_regs[5] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[5] = VROM_pages[Mapper_23_regs[5]];
        } break;

        case 0xE000: {
            Mapper_23_regs[6] = (Mapper_23_regs[6] & 0xF0) | (data & 0x0F);
            PPUBANK[6] = VROM_pages[Mapper_23_regs[6]];
        } break;

        case 0xE001:
        case 0xE004: {
            Mapper_23_regs[6] = (Mapper_23_regs[6] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[6] = VROM_pages[Mapper_23_regs[6]];
        } break;

        case 0xE002:
        case 0xE008: {
            Mapper_23_regs[7] = (Mapper_23_regs[7] & 0xF0) | (data & 0x0F);
            PPUBANK[7] = VROM_pages[Mapper_23_regs[7]];
        } break;

        case 0xE003:
        case 0xE00C: {
            Mapper_23_regs[7] = (Mapper_23_regs[7] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[7] = VROM_pages[Mapper_23_regs[7]];
        } break;

        case 0xF000: {
            Mapper_23_irq_latch = (Mapper_23_irq_latch & 0xF0) | (data & 0x0F);
        } break;

        case 0xF004: {
            Mapper_23_irq_latch = (Mapper_23_irq_latch & 0x0F) | ((data & 0x0F) << 4);
        } break;

        case 0xF008: {
            Mapper_23_irq_enabled = data & 0x03;
            if (Mapper_23_irq_enabled & 0x02) {
                Mapper_23_irq_counter = Mapper_23_irq_latch;
            }
        } break;

        case 0xF00C: {
            Mapper_23_irq_enabled = (Mapper_23_irq_enabled & 0x01) * 3;
        } break;
    }
}

void Mapper_23_HSync() {
    if (Mapper_23_irq_enabled & 0x02) {
        if (Mapper_23_irq_counter == 0xFF) {
            Mapper_23_irq_counter = Mapper_23_irq_latch;
            Mapper_23_irq_enabled = (Mapper_23_irq_enabled & 0x01) * 3;
            IRQ_REQ;
        } else {
            Mapper_23_irq_counter++;
        }
    }
}