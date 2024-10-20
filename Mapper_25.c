#include "Mapper_25.h"

extern uint32 currentCRC32;

uint8 Mapper_25_patch;
uint8 Mapper_25_regs[11];
uint8 Mapper_25_irq_enabled;
uint8 Mapper_25_irq_counter;
uint8 Mapper_25_irq_latch;

void Mapper_25_Init() {
    BankTable[4] = ROM_pages[0];
    BankTable[5] = ROM_pages[1];
    BankTable[6] = ROM_pages[ num_8k_ROM_pages - 2];
    BankTable[7] = ROM_pages[ num_8k_ROM_pages - 1];

    for ( int nPage = 0; nPage < 8; ++nPage )
        PPUBANK[ nPage ] = VROM_pages[ nPage ];

    for(int i = 0; i < 8; i++)
        Mapper_25_regs[i] = 0;

    Mapper_25_regs[8] = 0;
    Mapper_25_regs[9] = num_8k_ROM_pages - 2;
    Mapper_25_regs[10] = 0;    

    Mapper_25_irq_enabled = 0;
    Mapper_25_irq_counter = 0;
    Mapper_25_irq_latch = 0;
}

void Mapper_25_Write(uint16 addr, uint8 data) {
    switch (addr & 0xF000) {
        case 0x8000: {
            if (Mapper_25_regs[10] & 0x02) {
                Mapper_25_regs[9] = data;
                BankTable[6] = ROM_pages[data % num_8k_ROM_pages];
            } else {
                Mapper_25_regs[8] = data;
                BankTable[4] = ROM_pages[data % num_8k_ROM_pages];
            }
        } break;

        case 0xA000: {
            BankTable[5] = ROM_pages[data % num_8k_ROM_pages];
        } break;
    }

    switch (addr & 0xF00F) {
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

        case 0x9001:
        case 0x9004: {
            if ((Mapper_25_regs[10] & 0x02) != (data & 0x02)) {
                uint8 swap = Mapper_25_regs[8];
                Mapper_25_regs[8] = Mapper_25_regs[9];
                Mapper_25_regs[9] = swap;
                BankTable[4] = ROM_pages[Mapper_25_regs[8] % num_8k_ROM_pages];
                BankTable[6] = ROM_pages[Mapper_25_regs[9] % num_8k_ROM_pages];
            }
            Mapper_25_regs[10] = data;
        }
        break;

        case 0xB000: {
            Mapper_25_regs[0] = (Mapper_25_regs[0] & 0xF0) | (data & 0x0F);
            PPUBANK[0] = VROM_pages[Mapper_25_regs[0]];
        } break;

        case 0xB001:
        case 0xB004: {
            Mapper_25_regs[1] = (Mapper_25_regs[1] & 0xF0) | (data & 0x0F);
            PPUBANK[1] = VROM_pages[Mapper_25_regs[1]];
        } break;

        case 0xB002:
        case 0xB008: {
            Mapper_25_regs[0] = (Mapper_25_regs[0] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[0] = VROM_pages[Mapper_25_regs[0]];
        } break;

        case 0xB003:
        case 0xB00C: {
            Mapper_25_regs[1] = (Mapper_25_regs[1] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[1] = VROM_pages[Mapper_25_regs[1]];
        } break;

        case 0xC000: {
            Mapper_25_regs[2] = (Mapper_25_regs[2] & 0xF0) | (data & 0x0F);
            PPUBANK[2] = VROM_pages[Mapper_25_regs[2]];
        } break;

        case 0xC001:
        case 0xC004: {
            Mapper_25_regs[3] = (Mapper_25_regs[3] & 0xF0) | (data & 0x0F);
            PPUBANK[3] = VROM_pages[Mapper_25_regs[3]];
        } break;

        case 0xC002:
        case 0xC008: {
            Mapper_25_regs[2] = (Mapper_25_regs[2] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[2] = VROM_pages[Mapper_25_regs[2]];
        } break;

        case 0xC003:
        case 0xC00C: {
            Mapper_25_regs[3] = (Mapper_25_regs[3] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[3] = VROM_pages[Mapper_25_regs[3]];
        } break;

        case 0xD000: {
            Mapper_25_regs[4] = (Mapper_25_regs[4] & 0xF0) | (data & 0x0F);
            PPUBANK[4] = VROM_pages[Mapper_25_regs[4]];
        } break;

        case 0xD001:
        case 0xD004: {
            Mapper_25_regs[5] = (Mapper_25_regs[5] & 0xF0) | (data & 0x0F);
            PPUBANK[5] = VROM_pages[Mapper_25_regs[5]];
        } break;

        case 0xD002:
        case 0xD008: {
            Mapper_25_regs[4] = (Mapper_25_regs[4] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[4] = VROM_pages[Mapper_25_regs[4]];
        } break;

        case 0xD003:
        case 0xD00C: {
            Mapper_25_regs[5] = (Mapper_25_regs[5] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[5] = VROM_pages[Mapper_25_regs[5]];
        } break;

        case 0xE000: {
            Mapper_25_regs[6] = (Mapper_25_regs[6] & 0xF0) | (data & 0x0F);
            PPUBANK[6] = VROM_pages[Mapper_25_regs[6]];
        } break;

        case 0xE001:
        case 0xE004: {
            Mapper_25_regs[7] = (Mapper_25_regs[7] & 0xF0) | (data & 0x0F);
            PPUBANK[7] = VROM_pages[Mapper_25_regs[7]];
        } break;

        case 0xE002:
        case 0xE008: {
            Mapper_25_regs[6] = (Mapper_25_regs[6] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[6] = VROM_pages[Mapper_25_regs[6]];
        } break;

        case 0xE003:
        case 0xE00C: {
            Mapper_25_regs[7] = (Mapper_25_regs[7] & 0x0F) | ((data & 0x0F) << 4);
            PPUBANK[7] = VROM_pages[Mapper_25_regs[7]];
        } break;

        case 0xF000: {
            Mapper_25_irq_latch = (Mapper_25_irq_latch & 0xF0) | (data & 0x0F);
        } break;

        case 0xF001:
        case 0xF004: {
            Mapper_25_irq_enabled = data & 0x03;
            if (Mapper_25_irq_enabled & 0x02) {
                Mapper_25_irq_counter = Mapper_25_irq_latch;
            }
        } break;

        case 0xF002:
        case 0xF008: {
            Mapper_25_irq_latch = (Mapper_25_irq_latch & 0x0F) | ((data & 0x0F) << 4);
        } break;

        case 0xF003:
        case 0xF00C: {
            Mapper_25_irq_enabled = (Mapper_25_irq_enabled & 0x01) * 3;
        } break;
    }
}

void Mapper_25_HSync() {
    if (Mapper_25_irq_enabled & 0x02) {
        if(!Mapper_25_patch && Mapper_25_irq_counter == 0xFF) {
            Mapper_25_irq_counter = Mapper_25_irq_latch;
            set_irq(true);
        } else if(Mapper_25_patch && Mapper_25_irq_counter == 0x00) {
            Mapper_25_irq_counter = Mapper_25_irq_latch;
            set_irq(true);
        } else {
            Mapper_25_irq_counter++;
        }
    }
}