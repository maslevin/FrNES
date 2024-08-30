#include "Mapper_64.h"

uint8 Mapper_64_regs[3];
uint8 Mapper_64_irq_latch;
uint8 Mapper_64_irq_counter;
uint8 Mapper_64_irq_enabled;

void Mapper_64_Init() {

    ROMBANK0 = ROMLASTPAGE( 0 );
	ROMBANK1 = ROMLASTPAGE( 0 );
	ROMBANK2 = ROMLASTPAGE( 0 );
	ROMBANK3 = ROMLASTPAGE( 0 );

    if ( NesHeader.byVRomSize > 0 ) {
        for ( int nPage = 0; nPage < 8; ++nPage )
            PPUBANK[ nPage ] = &VROM[ nPage * 0x400 ];
    }

    Mapper_64_irq_latch = 0;
    Mapper_64_irq_counter = 0;
    Mapper_64_irq_enabled = 0;

    Mapper_64_regs[0] = 0;
    Mapper_64_regs[1] = 0;
    Mapper_64_regs[2] = 0;
}

void Mapper_64_Write( uint16 wAddr, unsigned char byData ) {
    uint32 num_8k_ROM_banks = NesHeader.byRomSize * 2;   

    switch(wAddr & 0xF003) {
        case 0x8000: {
            Mapper_64_regs[0] = byData & 0x0F;
            Mapper_64_regs[1] = byData & 0x40;
            Mapper_64_regs[2] = byData & 0x80;
        } break;

        case 0x8001: {
            switch(Mapper_64_regs[0]) {
                case 0x00: {
                    if(Mapper_64_regs[2]) {
                        PPUBANK[4] = &VROM[ byData * 0x400 ];
                        PPUBANK[5] = &VROM[ (byData + 1) * 0x400 ];
                    } else {
                        PPUBANK[0] = &VROM[ byData * 0x400 ];
                        PPUBANK[1] = &VROM[ (byData + 1) * 0x400 ];
                    }
                } break;

                case 0x01: {
                    if(Mapper_64_regs[2]) {
                        PPUBANK[6] = &VROM[ byData * 0x400 ];
                        PPUBANK[7] = &VROM[ (byData + 1) * 0x400 ];
                    } else {
                        PPUBANK[2] = &VROM[ byData * 0x400 ];
                        PPUBANK[3] = &VROM[ (byData + 1) * 0x400 ];
                    }
                } break;

                case 0x02: {
                    if(Mapper_64_regs[2]) {
                        PPUBANK[0] = &VROM[ byData * 0x400 ];
                    } else {
                        PPUBANK[4] = &VROM[ byData * 0x400 ];
                    }
                }
                break;

                case 0x03: {
                    if(Mapper_64_regs[2]) {
                        PPUBANK[1] = &VROM[ byData * 0x400 ];
                    } else {
                        PPUBANK[5] = &VROM[ byData * 0x400 ];
                    }
                }
                break;

                case 0x04: {
                    if(Mapper_64_regs[2]) {
                        PPUBANK[2] = &VROM[ byData * 0x400 ];
                    } else {
                        PPUBANK[6] = &VROM[ byData * 0x400 ];
                    }
                }
                break;

                case 0x05: {
                    if(Mapper_64_regs[2]) {
                        PPUBANK[3] = &VROM[ byData * 0x400 ];
                    } else {
                        PPUBANK[7] = &VROM[ byData * 0x400 ];
                    }
                }
                break;

                case 0x06: {
                    if(Mapper_64_regs[1]) {
                        ROMBANK1 = ROMPAGE(byData % num_8k_ROM_banks);
                    } else {
                        ROMBANK0 = ROMPAGE(byData % num_8k_ROM_banks);
                    }
                }
                break;

                case 0x07: {
                    if(Mapper_64_regs[1]) {
                        ROMBANK2 = ROMPAGE(byData % num_8k_ROM_banks);
                    } else {
                        ROMBANK1 = ROMPAGE(byData % num_8k_ROM_banks);
                    }
                } break;

                case 0x08: {
                    PPUBANK[1] = &VROM[ byData * 0x400 ];
                } break;

                case 0x09: {
                    PPUBANK[3] = &VROM[ byData * 0x400 ];
                } break;

                case 0x0F: {
                    if(Mapper_64_regs[1]) {
                        ROMBANK0 = ROMPAGE(byData % num_8k_ROM_banks);
                    } else {
                        ROMBANK2 = ROMPAGE(byData % num_8k_ROM_banks);
                    }
                } break;
            }
        }
        break;

        case 0xA000: {
            if(!(byData & 0x01)) {
                pNesX_Mirroring(MIRRORING_VERTICAL);
            } else {
                pNesX_Mirroring(MIRRORING_HORIZONTAL);
            }
        } break;

        case 0xC000: {
            Mapper_64_irq_latch = byData;
            Mapper_64_irq_counter = Mapper_64_irq_latch;
        } break;

        case 0xC001: {
            Mapper_64_irq_counter = Mapper_64_irq_latch;
        } break;

        case 0xE000: {
            Mapper_64_irq_enabled = 0;
            Mapper_64_irq_counter = Mapper_64_irq_latch;
        } break;

        case 0xE001: {
            Mapper_64_irq_enabled = 1;
            Mapper_64_irq_counter = Mapper_64_irq_latch;
        } break;
    }
}

void Mapper_64_HSync() {
	if (Mapper_64_irq_enabled) {
		if ((ppuinfo.PPU_Scanline >= 0) && (ppuinfo.PPU_Scanline <= 239)) {
			if (PPU_R1 & (R1_SHOW_SCR | R1_SHOW_SP )) {
				if (!(--Mapper_64_irq_counter)) {
					Mapper_64_irq_counter = Mapper_64_irq_latch;
					IRQ_REQ;
				}
			}
		}
	}
}