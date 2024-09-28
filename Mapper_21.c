#include "Mapper_21.h"

uint8 Mapper_21_regs[9];
uint8 Mapper_21_irq_enabled;
uint8 Mapper_21_irq_counter;
uint8 Mapper_21_irq_latch;

void Mapper_21_Init() {
    ROMBANK0 = ROM_pages[0];
    ROMBANK1 = ROM_pages[1];
    ROMBANK2 = ROM_pages[ num_8k_ROM_pages - 2];
    ROMBANK3 = ROM_pages[ num_8k_ROM_pages - 1];

    Mapper_21_regs[0] = 0;
    Mapper_21_regs[1] = 1;
    Mapper_21_regs[2] = 2;
    Mapper_21_regs[3] = 3;
    Mapper_21_regs[4] = 4;
    Mapper_21_regs[5] = 5;
    Mapper_21_regs[6] = 6;
    Mapper_21_regs[7] = 7;
    Mapper_21_regs[8] = 0;

    Mapper_21_irq_enabled = 0;
    Mapper_21_irq_counter = 0;
    Mapper_21_irq_latch = 0;
}

void Mapper_21_Write(uint16 addr, uint8 data) {
    switch (addr & 0xF0CF) {
        case 0x8000: {
            if (Mapper_21_regs[8] & 0x02) {
                ROMBANK2 = ROM_pages[data];
            } else {
                ROMBANK0 = ROM_pages[data];
            }
        } break;

        case 0xA000: {
            ROMBANK1 = ROM_pages[data];
        } break;

        case 0x9000: {
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
        } break;

        case 0x9002:
        case 0x9080: {
            Mapper_21_regs[8] = data;
        } break;

        case 0xB000: {
            Mapper_21_regs[0] = (Mapper_21_regs[0] & 0xF0) | (data & 0x0F);
            PPUBANK[0] = VROM_pages[Mapper_21_regs[0]];
        } break;

        case 0xB002:
        case 0xB040: {
            Mapper_21_regs[0] = (Mapper_21_regs[0] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[0] = VROM_pages[Mapper_21_regs[0]];
        } break;

        case 0xB001:
        case 0xB004:
        case 0xB080: {
            Mapper_21_regs[1] = (Mapper_21_regs[1] & 0xF0) | (data & 0x0F);
            PPUBANK[1] = VROM_pages[Mapper_21_regs[1]];
        } break;

        case 0xB003:
        case 0xB006:
        case 0xB0C0: {
            Mapper_21_regs[1] = (Mapper_21_regs[1] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[1] = VROM_pages[Mapper_21_regs[1]];
        } break;

        case 0xC000: {
            Mapper_21_regs[2] = (Mapper_21_regs[2] & 0xF0) | (data & 0x0F);
            PPUBANK[2] = VROM_pages[Mapper_21_regs[2]];
        } break;

        case 0xC002:
        case 0xC040: {
            Mapper_21_regs[2] = (Mapper_21_regs[2] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[2] = VROM_pages[Mapper_21_regs[2]];
        } break;

        case 0xC001:
        case 0xC004:
        case 0xC080: {
            Mapper_21_regs[3] = (Mapper_21_regs[3] & 0xF0) | (data & 0x0F);
            PPUBANK[3] = VROM_pages[Mapper_21_regs[3]];
        } break;

        case 0xC003:
        case 0xC006:
        case 0xC0C0: {
            Mapper_21_regs[3] = (Mapper_21_regs[3] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[3] = VROM_pages[Mapper_21_regs[3]];
        } break;

        case 0xD000: {
            Mapper_21_regs[4] = (Mapper_21_regs[4] & 0xF0) | (data & 0x0F);
            PPUBANK[4] = VROM_pages[Mapper_21_regs[4]];
        } break;

        case 0xD002:
        case 0xD040: {
            Mapper_21_regs[4] = (Mapper_21_regs[4] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[4] = VROM_pages[Mapper_21_regs[4]];
        } break;

        case 0xD001:
        case 0xD004:
        case 0xD080: {
            Mapper_21_regs[5] = (Mapper_21_regs[5] & 0xF0) | (data & 0x0F);
            PPUBANK[5] = VROM_pages[Mapper_21_regs[5]];
        } break;

        case 0xD003:
        case 0xD006:
        case 0xD0C0: {
            Mapper_21_regs[5] = (Mapper_21_regs[5] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[5] = VROM_pages[Mapper_21_regs[5]];
        } break;

        case 0xE000: {
            Mapper_21_regs[6] = (Mapper_21_regs[6] & 0xF0) | (data & 0x0F);
            PPUBANK[6] = VROM_pages[Mapper_21_regs[6]];
        } break;

        case 0xE002:
        case 0xE040: {
            Mapper_21_regs[6] = (Mapper_21_regs[6] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[6] = VROM_pages[Mapper_21_regs[6]];
        } break;

        case 0xE001:
        case 0xE004:
        case 0xE080: {
            Mapper_21_regs[7] = (Mapper_21_regs[7] & 0xF0) | (data & 0x0F);
            PPUBANK[7] = VROM_pages[Mapper_21_regs[7]];
        } break;

        case 0xE003:
        case 0xE006:
        case 0xE0C0: {
            Mapper_21_regs[7] = (Mapper_21_regs[7] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[7] = VROM_pages[Mapper_21_regs[7]];
        } break;

        case 0xF000: {
            Mapper_21_irq_latch = (Mapper_21_irq_latch & 0xF0) | (data & 0x0F);
        } break;

        case 0xF002:
        case 0xF040: {
            Mapper_21_irq_latch = (Mapper_21_irq_latch & 0x0F) | ((data & 0x0F) << 4);
        } break;

        case 0xF003:
        case 0xF0C0: {
            Mapper_21_irq_enabled = (Mapper_21_irq_enabled & 0x01) * 3;
        } break;

        case 0xF004:
        case 0xF080: {
            Mapper_21_irq_enabled = data & 0x03;
            if (Mapper_21_irq_enabled & 0x02) {
                Mapper_21_irq_counter = Mapper_21_irq_latch;
            }
        } break;
    }
}

void Mapper_21_HSync() {
    if (Mapper_21_irq_enabled & 0x02) {
        if (Mapper_21_irq_counter == 0xFF) {
            Mapper_21_irq_counter = Mapper_21_irq_latch;
            Mapper_21_irq_enabled = (Mapper_21_irq_enabled & 0x01) * 3;
            IRQ_REQ;
        } else {
            Mapper_21_irq_counter++;
        }
    }
}