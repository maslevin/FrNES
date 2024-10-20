#include "Mapper_64.h"

uint8 Mapper_64_regs[3];
uint8 Mapper_64_irq_latch;
uint8 Mapper_64_irq_counter;
uint8 Mapper_64_irq_enabled;

void Mapper_64_Init() {

    BankTable[4] = ROM_pages[num_8k_ROM_pages - 1];
	BankTable[5] = ROM_pages[num_8k_ROM_pages - 1];
	BankTable[6] = ROM_pages[num_8k_ROM_pages - 1];
	BankTable[7] = ROM_pages[num_8k_ROM_pages - 1];

    for ( int nPage = 0; nPage < 8; ++nPage )
        PPUBANK[ nPage ] = &VROM[ nPage * 0x400 ];

    Mapper_64_irq_latch = 0;
    Mapper_64_irq_counter = 0;
    Mapper_64_irq_enabled = 0;

    Mapper_64_regs[0] = 0;
    Mapper_64_regs[1] = 0;
    Mapper_64_regs[2] = 0;
}

void Mapper_64_Write( uint16 wAddr, unsigned char byData ) {
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
                        PPUBANK[4] = VROM_pages[ byData ];
                        PPUBANK[5] = VROM_pages[ byData + 1 ];
                    } else {
                        PPUBANK[0] = VROM_pages[ byData ];
                        PPUBANK[1] = VROM_pages[ byData + 1 ];
                    }
                } break;

                case 0x01: {
                    if(Mapper_64_regs[2]) {
                        PPUBANK[6] = VROM_pages[ byData ];
                        PPUBANK[7] = VROM_pages[ byData + 1 ];
                    } else {
                        PPUBANK[2] = VROM_pages[ byData ];
                        PPUBANK[3] = VROM_pages[ byData + 1 ];
                    }
                } break;

                case 0x02: {
                    if(Mapper_64_regs[2]) {
                        PPUBANK[0] = VROM_pages[ byData ];
                    } else {
                        PPUBANK[4] = VROM_pages[ byData ];
                    }
                }
                break;

                case 0x03: {
                    if(Mapper_64_regs[2]) {
                        PPUBANK[1] = VROM_pages[ byData ];
                    } else {
                        PPUBANK[5] = VROM_pages[ byData ];
                    }
                }
                break;

                case 0x04: {
                    if(Mapper_64_regs[2]) {
                        PPUBANK[2] = VROM_pages[ byData ];
                    } else {
                        PPUBANK[6] = VROM_pages[ byData ];
                    }
                }
                break;

                case 0x05: {
                    if(Mapper_64_regs[2]) {
                        PPUBANK[3] = VROM_pages[ byData ];
                    } else {
                        PPUBANK[7] = VROM_pages[ byData ];
                    }
                }
                break;

                case 0x06: {
                    if(Mapper_64_regs[1]) {
                        BankTable[5] = ROM_pages[byData % num_8k_ROM_pages];
                    } else {
                        BankTable[4] = ROM_pages[byData % num_8k_ROM_pages];
                    }
                }
                break;

                case 0x07: {
                    if(Mapper_64_regs[1]) {
                        BankTable[6] = ROM_pages[byData % num_8k_ROM_pages];
                    } else {
                        BankTable[5] = ROM_pages[byData % num_8k_ROM_pages];
                    }
                } break;

                case 0x08: {
                    PPUBANK[1] = VROM_pages[ byData ];
                } break;

                case 0x09: {
                    PPUBANK[3] = VROM_pages[ byData ];
                } break;

                case 0x0F: {
                    if(Mapper_64_regs[1]) {
                        BankTable[4] = ROM_pages[byData % num_8k_ROM_pages];
                    } else {
                        BankTable[6] = ROM_pages[byData % num_8k_ROM_pages];
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
					set_irq(true);
				}
			}
		}
	}
}