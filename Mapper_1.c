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

void Mapper_1_Set_CPU_Banks() {
    ROMBANK0 = ROMPAGE((Mapper_1_256K_base << 5) + (Mapper_1_bank1 & ((256/8)-1)));
    ROMBANK1 = ROMPAGE((Mapper_1_256K_base << 5) + (Mapper_1_bank2 & ((256/8)-1)));
    ROMBANK2 = ROMPAGE((Mapper_1_256K_base << 5) + (Mapper_1_bank3 & ((256/8)-1)));
    ROMBANK3 = ROMPAGE((Mapper_1_256K_base << 5) + (Mapper_1_bank4 & ((256/8)-1)));
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

    uint32 num_8k_ROM_banks = NesHeader.byRomSize * 2; 
    uint32 size_in_K = num_8k_ROM_banks * 8;

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
        Mapper_1_HI1 = num_8k_ROM_banks - 2;
        Mapper_1_HI2 = num_8k_ROM_banks - 1;
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

    Mapper_1_set_CPU_banks();

    /* Set PPU VROM Banks */
    if ( NesHeader.byVRomSize > 0 ) {
        for ( nPage = 0; nPage < 8; ++nPage )
            PPUBANK[ nPage ] = &VROM[ nPage * 0x400 ];
    }

    /* Set up wiring of the interrupt pin */
    K6502_Set_Int_Wiring( 1, 1 );
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
            uint32 num_1k_VROM_banks = NesHeader.byVRomSize * 8;

            if (Mapper_1_Size == MMC1_1024K) {
                if (Mapper_1_regs[0] & 0x10) {
                    if (Mapper_1_swap) {
                        Mapper_1_256K_base = (Mapper_1_regs[1] & 0x10) >> 4;
                        if (Mapper_1_regs[0] & 0x08) {
                            Mapper_1_256K_base |= ((Mapper_1_regs[2] & 0x10) >> 3);
                        }
                        Mapper_1_set_CPU_banks();
                        Mapper_1_swap = 0;
                    } else {
                        Mapper_1_swap = 1;
                    }
                } else {
                    // use 1st or 4th 256K banks
                    Mapper_1_256K_base = (Mapper_1_regs[1] & 0x10) ? 3 : 0;
                    Mapper_1_Set_CPU_Banks();
                }
            } else if((Mapper_1_Size == MMC1_512K) && (!num_1k_VROM_banks)) {
                Mapper_1_256K_base = (Mapper_1_regs[1] & 0x10) >> 4;
                Mapper_1_Set_CPU_Banks();
            } else if (num_1k_VROM_banks) {
                // set VROM bank at $0000
                if (Mapper_1_regs[0] & 0x10) {
                    // swap 4K
                    bank_num <<= 2;
                    PPUBANK[0] = &VROM[bank_num * 0x400];
                    PPUBANK[1] = &VROM[(bank_num + 1) * 0x400];
                    PPUBANK[2] = &VROM[(bank_num + 2) * 0x400];
                    PPUBANK[3] = &VROM[(bank_num + 3) * 0x400];
                } else {
                    // swap 8K
                    bank_num <<= 2;
                    PPUBANK[0] = &VROM[bank_num * 0x400];
                    PPUBANK[1] = &VROM[(bank_num + 1) * 0x400];
                    PPUBANK[2] = &VROM[(bank_num + 2) * 0x400];
                    PPUBANK[3] = &VROM[(bank_num + 3) * 0x400];
                    PPUBANK[4] = &VROM[(bank_num + 4) * 0x400];
                    PPUBANK[5] = &VROM[(bank_num + 5) * 0x400];
                    PPUBANK[6] = &VROM[(bank_num + 6) * 0x400];
                    PPUBANK[7] = &VROM[(bank_num + 7) * 0x400];
                }
            } else {
                if (Mapper_1_regs[0] & 0x10) {
                    bank_num <<= 2;
                    /* // TODO: How to handle VRAM ... 
                    set_VRAM_bank0(0, bank_num+0);
                    set_VRAM_bank0(1, bank_num+1);
                    set_VRAM_bank0(2, bank_num+2);
                    set_VRAM_bank0(3, bank_num+3);
                    */
                }
            }
        } break;

        case 2: {
            uint8 bank_num = Mapper_1_regs[2];
            uint32 num_1k_VROM_banks = NesHeader.byVRomSize * 8;

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

            if (!num_1k_VROM_banks) {
                if (Mapper_1_regs[0] & 0x10) {
                    bank_num <<= 2;
                    /* // TODO: How to handle VRAM ...
                    set_VRAM_bank0(4, bank_num+0);
                    set_VRAM_bank0(5, bank_num+1);
                    set_VRAM_bank0(6, bank_num+2);
                    set_VRAM_bank0(7, bank_num+3);
                    */
                    break;
                }
            }

            // set 4K VROM bank at $1000
            if (Mapper_1_regs[0] & 0x10) {
                // swap 4K
                bank_num <<= 2;
                PPUBANK[4] = &VROM[bank_num * 0x400];
                PPUBANK[5] = &VROM[(bank_num + 1) * 0x400];
                PPUBANK[6] = &VROM[(bank_num + 2) * 0x400];
                PPUBANK[7] = &VROM[(bank_num + 3) * 0x400];
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

            Mapper_1_set_CPU_banks();
        } break;
    }
}
