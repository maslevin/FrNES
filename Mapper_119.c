#include "Mapper_119.h"

uint8 Mapper_119_regs[8];

uint32 Mapper_119_prg0,Mapper_119_prg1;
uint32 Mapper_119_chr01,Mapper_119_chr23,Mapper_119_chr4,Mapper_119_chr5,Mapper_119_chr6,Mapper_119_chr7;

bool chr_swap() { return Mapper_119_regs[0] & 0x80; }
bool prg_swap() { return Mapper_119_regs[0] & 0x40; }

uint8 Mapper_119_irq_enabled; // IRQs enabled
uint8 Mapper_119_irq_counter; // IRQ scanline counter, decreasing
uint8 Mapper_119_irq_latch;   // IRQ scanline counter latch

void Mapper_119_MMC3_set_CPU_banks() {
    if (prg_swap()) {
        //printf("Setting Prg Bank1 to page [%u] and Bank2 to page [%u]\n",Mapper_119_prg1,Mapper_119_prg0);
        BankTable[4] = ROM_pages[ num_8k_ROM_pages - 2];
        BankTable[5] = ROM_pages[ Mapper_119_prg1 % num_8k_ROM_pages];
        BankTable[6] = ROM_pages[ Mapper_119_prg0 % num_8k_ROM_pages];
        BankTable[7] = ROM_pages[ num_8k_ROM_pages - 1];
    } else {
        //printf("Setting Prg Bank0 to page [%u] and Bank1 to page [%u]\n",Mapper_119_prg0,Mapper_119_prg1);
        BankTable[4] = ROM_pages[ Mapper_119_prg0 % num_8k_ROM_pages];
        BankTable[5] = ROM_pages[ Mapper_119_prg1 % num_8k_ROM_pages];
        BankTable[6] = ROM_pages[ num_8k_ROM_pages - 2];
        BankTable[7] = ROM_pages[ num_8k_ROM_pages - 1];     
    }
}

void Mapper_119_MMC3_set_PPU_banks() {
    uint8 chr_bank[8];

    if (chr_swap()) {
        chr_bank[0] = Mapper_119_chr4;
        chr_bank[1] = Mapper_119_chr5;
        chr_bank[2] = Mapper_119_chr6;
        chr_bank[3] = Mapper_119_chr7;
        chr_bank[4] = Mapper_119_chr01+0;
        chr_bank[5] = Mapper_119_chr01+1;
        chr_bank[6] = Mapper_119_chr23+0;
        chr_bank[7] = Mapper_119_chr23+1;
    } else {
        chr_bank[0] = Mapper_119_chr01+0;
        chr_bank[1] = Mapper_119_chr01+1;
        chr_bank[2] = Mapper_119_chr23+0;
        chr_bank[3] = Mapper_119_chr23+1;
        chr_bank[4] = Mapper_119_chr4;
        chr_bank[5] = Mapper_119_chr5;
        chr_bank[6] = Mapper_119_chr6;
        chr_bank[7] = Mapper_119_chr7;
    }

    if (chr_bank[0] & 0x40) {
        PPUBANK[0] = VRAM_pages[(chr_bank[0] & 0x07)];
    } else {
        PPUBANK[0] = VROM_pages[chr_bank[0]];
    }

    if (chr_bank[1] & 0x40) {
        PPUBANK[1] = VRAM_pages[(chr_bank[1] & 0x07)];
    } else {
        PPUBANK[1] = VROM_pages[chr_bank[1]];
    }

    if (chr_bank[2] & 0x40) {
        PPUBANK[2] = VRAM_pages[(chr_bank[2] & 0x07)];
    } else {
        PPUBANK[2] = VROM_pages[chr_bank[2]];
    }

    if (chr_bank[3] & 0x40) {
        PPUBANK[3] = VRAM_pages[(chr_bank[3] & 0x07)];
    } else {
        PPUBANK[3] = VROM_pages[chr_bank[3]];
    }

    if (chr_bank[4] & 0x40) {
        PPUBANK[4] = VRAM_pages[(chr_bank[4] & 0x07)];
    } else {
        PPUBANK[4] = VROM_pages[chr_bank[4]];
    }

    if (chr_bank[5] & 0x40) {
        PPUBANK[5] = VRAM_pages[(chr_bank[5] & 0x07)];
    } else {
        PPUBANK[5] = VROM_pages[chr_bank[5]];
    }

    if (chr_bank[6] & 0x40) {
        PPUBANK[6] = VRAM_pages[(chr_bank[6] & 0x07)];
    } else {
        PPUBANK[6] = VROM_pages[chr_bank[6]];
    }

    if (chr_bank[7] & 0x40) {
        PPUBANK[7] = VRAM_pages[(chr_bank[7] & 0x07)];
    } else {
        PPUBANK[7] = VROM_pages[chr_bank[7]];
    }
}

void Mapper_119_Init() {
    for (int i = 0; i < 8; i++)
        Mapper_119_regs[i] = 0x00;

    // set CPU bank pointers
    Mapper_119_prg0 = 0;
    Mapper_119_prg1 = 1;
    Mapper_119_MMC3_set_CPU_banks();

    // set VROM banks
    Mapper_119_chr01 = 0;
    Mapper_119_chr23 = 2;
    Mapper_119_chr4  = 4;
    Mapper_119_chr5  = 5;
    Mapper_119_chr6  = 6;
    Mapper_119_chr7  = 7;
    Mapper_119_MMC3_set_PPU_banks();

    Mapper_119_irq_enabled = 0;
    Mapper_119_irq_counter = 0;
    Mapper_119_irq_latch = 0;
}

void Mapper_119_Write(uint16 addr, uint8 data) {
    switch (addr & 0xE001) {
        case 0x8000: {
            Mapper_119_regs[0] = data;
            Mapper_119_MMC3_set_PPU_banks();
            Mapper_119_MMC3_set_CPU_banks();
        } break;

        case 0x8001: {
            uint32 bank_num;

            Mapper_119_regs[1] = data;
            bank_num = Mapper_119_regs[1];

            switch (Mapper_119_regs[0] & 0x07) {
                case 0x00: {
                    if (num_1k_VROM_pages) {
                        bank_num &= 0xfe;
                        Mapper_119_chr01 = bank_num;
                        Mapper_119_MMC3_set_PPU_banks();
                    }
                } break;

                case 0x01: {
                    if (num_1k_VROM_pages) {
                        bank_num &= 0xfe;
                        Mapper_119_chr23 = bank_num;
                        Mapper_119_MMC3_set_PPU_banks();
                    }
                } break;

                case 0x02: {
                    if (num_1k_VROM_pages) {
                        Mapper_119_chr4 = bank_num;
                        Mapper_119_MMC3_set_PPU_banks();
                    }
                } break;

                case 0x03: {
                    if (num_1k_VROM_pages) {
                        Mapper_119_chr5 = bank_num;
                        Mapper_119_MMC3_set_PPU_banks();
                    }
                } break;

                case 0x04: {
                    if (num_1k_VROM_pages) {
                        Mapper_119_chr6 = bank_num;
                        Mapper_119_MMC3_set_PPU_banks();
                    }
                } break;

                case 0x05: {
                    if (num_1k_VROM_pages) {
                        Mapper_119_chr7 = bank_num;
                        Mapper_119_MMC3_set_PPU_banks();
                    }
                } break;

                case 0x06: {
                    Mapper_119_prg0 = bank_num;
                    Mapper_119_MMC3_set_CPU_banks();
                } break;

                case 0x07: {
                    Mapper_119_prg1 = bank_num;
                    Mapper_119_MMC3_set_CPU_banks();
                } break;
            }
        } break;

        case 0xA000: {
            Mapper_119_regs[2] = data;

            if (ROM_Mirroring != MIRRORING_FOUR_SCREEN) {
                if (data & 0x01) {
                    pNesX_Mirroring(MIRRORING_HORIZONTAL);
                } else {
                    pNesX_Mirroring(MIRRORING_VERTICAL);
                }
            }
        } break;

        case 0xA001: {
            Mapper_119_regs[3] = data;
        } break;

        case 0xC000: {
            Mapper_119_regs[4] = data;
            Mapper_119_irq_counter = Mapper_119_regs[4];
        } break;

        case 0xC001: {
            Mapper_119_regs[5] = data;
            Mapper_119_irq_latch = Mapper_119_regs[5];
        } break;

        case 0xE000: {
            Mapper_119_regs[6] = data;
            Mapper_119_irq_enabled = 0;
        } break;

        case 0xE001: {
            Mapper_119_regs[7] = data;
            Mapper_119_irq_enabled = 1;
        } break;
    }
}

void Mapper_119_HSync() {
    if (Mapper_119_irq_enabled) {
        if ((ppuinfo.PPU_Scanline >= 0) && (ppuinfo.PPU_Scanline <= 239)) {
            if (PPU_R1 & (R1_SHOW_SCR | R1_SHOW_SP )) {
                if (Mapper_119_irq_counter == 0) {
                    Mapper_119_irq_counter = Mapper_119_irq_latch;
                    set_irq(true);
                }
                Mapper_119_irq_counter--;
            }
        }
    }
}