#include "Mapper_1.h"

/*===================================================================*/
/*                                                                   */
/*                            Mapper 1                               */
/*                                                                   */
/*===================================================================*/

uint32 Mapper_1_write_count;
uint8  Mapper_1_bits;
uint8  Mapper_1_regs[4];
uint32 Mapper_1_last_write_addr;

enum MMC1_Size {
    MMC1_SMALL,
    MMC1_512K,
    MMC1_1024K
} Mapper_1_Size;

uint32 Mapper_1_256K_base;
uint32 Mapper_1_swap;

// these are the 4 ROM banks currently selected
uint32 Mapper_1_bank1;
uint32 Mapper_1_bank2;
uint32 Mapper_1_bank3;
uint32 Mapper_1_bank4;

uint32 Mapper_1_HI1;
uint32 Mapper_1_HI2;

inline void Mapper_1_Set_CPU_Banks() {
    ROMBANK0 = ROM_pages[(Mapper_1_256K_base << 5) + (Mapper_1_bank1 & 0x1f)];
    ROMBANK1 = ROM_pages[(Mapper_1_256K_base << 5) + (Mapper_1_bank2 & 0x1f)];
    ROMBANK2 = ROM_pages[(Mapper_1_256K_base << 5) + (Mapper_1_bank3 & 0x1f)];
    ROMBANK3 = ROM_pages[(Mapper_1_256K_base << 5) + (Mapper_1_bank4 & 0x1f)];
}

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 1                                              */
/*-------------------------------------------------------------------*/
void Mapper_1_Init() {

    Mapper_1_write_count = 0;
    Mapper_1_bits = 0x00;
    Mapper_1_regs[0] = 0x0C; // reflects initial ROM state
    Mapper_1_regs[1] = 0x00;
    Mapper_1_regs[2] = 0x00;
    Mapper_1_regs[3] = 0x00;

    uint32 size_in_K = num_8k_ROM_pages * 8;

    if (size_in_K == 1024) {
        Mapper_1_Size = MMC1_1024K;
    } else if(size_in_K == 512) {
        Mapper_1_Size = MMC1_512K;
    } else {
        Mapper_1_Size = MMC1_SMALL;
    }

    Mapper_1_256K_base = 0; // use first 256K
    Mapper_1_swap = 0;

    if (Mapper_1_Size == MMC1_SMALL) {
        // set two high pages to last two banks
        Mapper_1_HI1 = num_8k_ROM_pages - 2;
        Mapper_1_HI2 = num_8k_ROM_pages - 1;
    } else {
        // set two high pages to last two banks of current 256K region
        Mapper_1_HI1 = (256/8) - 2;
        Mapper_1_HI2 = (256/8) - 1;
    }

    // set CPU bank pointers
    Mapper_1_bank1 = 0;
    Mapper_1_bank2 = 1;
    Mapper_1_bank3 = Mapper_1_HI1;
    Mapper_1_bank4 = Mapper_1_HI2;

    Mapper_1_Set_CPU_Banks();

    for ( int nPage = 0; nPage < 8; ++nPage )
        PPUBANK[ nPage ] = VROM_pages[nPage];
}

/*-------------------------------------------------------------------*/
/*  Mapper 1 Write Function                                          */
/*-------------------------------------------------------------------*/
void Mapper_1_Write( uint16 wAddr, unsigned char byData ) {
    uint32 reg_num;

    // if write is to a different reg, reset
    if ((wAddr & 0x6000) != (Mapper_1_last_write_addr & 0x6000)) {
        Mapper_1_write_count = 0;
        Mapper_1_bits = 0x00;
    }
    Mapper_1_last_write_addr = wAddr;

    // if bit 7 set, reset and return
    if (byData & 0x80) {
        Mapper_1_write_count = 0;
        Mapper_1_bits = 0x00;
        return;
    }

    Mapper_1_bits |= (byData & 0x01) << Mapper_1_write_count;
    if (++Mapper_1_write_count < 5) return;
  
    reg_num = (wAddr & 0x7FFF) >> 13;
    Mapper_1_regs[reg_num] = Mapper_1_bits;

    Mapper_1_write_count = 0;
    Mapper_1_bits = 0x00;

    switch(reg_num) {
        case 0: {
            // set mirroring
            if (Mapper_1_regs[0] & 0x02) {
                if (Mapper_1_regs[0] & 0x01) {
                    pNesX_Mirroring(MIRRORING_HORIZONTAL);
                } else {
                    pNesX_Mirroring(MIRRORING_VERTICAL);                    
                }
            } else {
                // one-screen mirroring
                if (Mapper_1_regs[0] & 0x01) {
                    pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_HIGH);
                } else {
                    pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_LOW);
                }
            }
        } break;

        case 1: {
            uint8 bank_num = Mapper_1_regs[1];

            if (Mapper_1_Size == MMC1_1024K) {
                if (Mapper_1_regs[0] & 0x10) {
                    if (Mapper_1_swap) {
                        Mapper_1_256K_base = (Mapper_1_regs[1] & 0x10) >> 4;
                        if (Mapper_1_regs[0] & 0x08) {
                            Mapper_1_256K_base |= ((Mapper_1_regs[2] & 0x10) >> 3);
                        }
                        Mapper_1_Set_CPU_Banks();
                        Mapper_1_swap = 0;
                    } else {
                        Mapper_1_swap = 1;
                    }
                } else {
                    // use 1st or 4th 256K banks
                    Mapper_1_256K_base = (Mapper_1_regs[1] & 0x10) ? 3 : 0;
                    Mapper_1_Set_CPU_Banks();
                }
            } else if((Mapper_1_Size == MMC1_512K) && (!num_1k_VROM_pages)) {
                Mapper_1_256K_base = (Mapper_1_regs[1] & 0x10) >> 4;
                Mapper_1_Set_CPU_Banks();
            } else {
                // set VROM bank at $0000
                if (Mapper_1_regs[0] & 0x10) {
                    // swap 4K
                    bank_num <<= 2;
                    PPUBANK[0] = VROM_pages[bank_num];
                    PPUBANK[1] = VROM_pages[bank_num + 1];
                    PPUBANK[2] = VROM_pages[bank_num + 2];
                    PPUBANK[3] = VROM_pages[bank_num + 3];
                } else {
                    // swap 8K
                    bank_num <<= 2;
                    PPUBANK[0] = VROM_pages[bank_num];
                    PPUBANK[1] = VROM_pages[bank_num + 1];
                    PPUBANK[2] = VROM_pages[bank_num + 2];
                    PPUBANK[3] = VROM_pages[bank_num + 3];
                    PPUBANK[4] = VROM_pages[bank_num + 4];
                    PPUBANK[5] = VROM_pages[bank_num + 5];
                    PPUBANK[6] = VROM_pages[bank_num + 6];
                    PPUBANK[7] = VROM_pages[bank_num + 7];
                }
            }
        } break;

        case 2: {
            uint8 bank_num = Mapper_1_regs[2];

            if ((Mapper_1_Size == MMC1_1024K) && (Mapper_1_regs[0] & 0x08)) {
                if (Mapper_1_swap) {
                    Mapper_1_256K_base = (Mapper_1_regs[1] & 0x10) >> 4;
                    Mapper_1_256K_base |= ((Mapper_1_regs[2] & 0x10) >> 3);
                    Mapper_1_Set_CPU_Banks();
                    Mapper_1_swap = 0;
                } else {
                    Mapper_1_swap = 1;
                }
            }

            // set 4K VROM bank at $1000
            if (Mapper_1_regs[0] & 0x10) {
                // swap 4K
                bank_num <<= 2;
                PPUBANK[4] = VROM_pages[bank_num];
                PPUBANK[5] = VROM_pages[bank_num + 1];
                PPUBANK[6] = VROM_pages[bank_num + 2];
                PPUBANK[7] = VROM_pages[bank_num + 3];
            }
        } break;

        case 3: {
            uint8 bank_num = Mapper_1_regs[3];

            if (Mapper_1_regs[0] & 0x08) {
                // 16K of ROM
                bank_num <<= 1;

                if (Mapper_1_regs[0] & 0x04) {
                    // 16K of ROM at $8000
                    Mapper_1_bank1 = bank_num;
                    Mapper_1_bank2 = bank_num+1;
                    Mapper_1_bank3 = Mapper_1_HI1;
                    Mapper_1_bank4 = Mapper_1_HI2;
                } else {
                    // 16K of ROM at $C000
                    if(Mapper_1_Size == MMC1_SMALL) {
                        Mapper_1_bank1 = 0;
                        Mapper_1_bank2 = 1;
                        Mapper_1_bank3 = bank_num;
                        Mapper_1_bank4 = bank_num+1;
                    }
                }
            } else {
                // 32K of ROM at $8000
                bank_num <<= 1;

                Mapper_1_bank1 = bank_num;
                Mapper_1_bank2 = bank_num+1;
                if (Mapper_1_Size == MMC1_SMALL) {
                    Mapper_1_bank3 = bank_num+2;
                    Mapper_1_bank4 = bank_num+3;
                }
            }

            Mapper_1_Set_CPU_Banks();
        } break;
    }
}
