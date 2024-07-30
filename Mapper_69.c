#include "Mapper_69.h"

uint8 Mapper_69_regs[1];
uint8 Mapper_69_irq_enabled;
uint32 Mapper_69_irq_counter;

/////////////////////////////////////////////////////////////////////
// Mapper 69
void Mapper_69_Init() {
    // Init ExSound
    //parent_NES->apu->SelectExSound(NES_APU_EXSOUND_FME7);

	ROMBANK0 = ROMPAGE( 0 );
	ROMBANK1 = ROMPAGE( 1 );
	ROMBANK2 = ROMLASTPAGE( 1 );
	ROMBANK3 = ROMLASTPAGE( 0 );

	if ( NesHeader.byVRomSize > 0 ) {
		for ( int nPage = 0; nPage < 8; ++nPage )
			PPUBANK[ nPage ] = &VROM[ nPage * 0x400 ];
	}

    Mapper_69_irq_enabled = 0;
    Mapper_69_irq_counter = 0;
}

void Mapper_69_Write(uint16 addr, uint8 data) {
//    printf("Map69_Write $%04X, %02X\n", addr, data);
    switch(addr & 0xE000) {
        case 0x8000: {
            Mapper_69_regs[0] = data;
        } break;

        case 0xA000: {
            switch (Mapper_69_regs[0] & 0x0F) {
                case 0x00: {
                    PPUBANK[0] = &VROM[ data * 0x400 ];
                } break;

                case 0x01: {
                    PPUBANK[1] = &VROM[ data * 0x400 ];
                } break;

                case 0x02: {
                    PPUBANK[2] = &VROM[ data * 0x400 ];
                } break;

                case 0x03: {
                    PPUBANK[3] = &VROM[ data * 0x400 ];
                } break;

                case 0x04: {
                    PPUBANK[4] = &VROM[ data * 0x400 ];
                } break;

                case 0x05: {
                    PPUBANK[5] = &VROM[ data * 0x400 ];
                } break;

                case 0x06: {
                    PPUBANK[6] = &VROM[ data * 0x400 ];
                } break;

                case 0x07: {
                    PPUBANK[7] = &VROM[ data * 0x400 ];
                } break;

                case 0x08: {
//                    if(!(data & 0x40)) {
                        VIRPC;
                        BankTable[3] = ROMPAGE(data & 0x1f);
                        REALPC;
//                    }
                } break;

                case 0x09: {
                    ROMBANK0 = ROMPAGE(data & 0x1f);
                } break;

                case 0x0A: {
                    ROMBANK1 = ROMPAGE(data & 0x1f);
                } break;

                case 0x0B: {
                    ROMBANK2 = ROMPAGE(data & 0x1f);
                } break;

                case 0x0C: {
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

                case 0x0D: {
                    Mapper_69_irq_enabled = data;
                } break;

                case 0x0E: {
                    Mapper_69_irq_counter = (Mapper_69_irq_counter & 0xFF00) | data;
                }
                break;

                case 0x0F: {
                    Mapper_69_irq_counter = (Mapper_69_irq_counter & 0x00FF) | (data << 8);
                }
                break;
            }
        } break;

        case 0xC000:
        case 0xE000: {
            // parent_NES->apu->ExWrite(addr, data);
        } break;
    }
}

void Mapper_69_HSync() {
    if (Mapper_69_irq_enabled) {
        if (Mapper_69_irq_counter <= 113) {
            K6502_DoIRQ();
            Mapper_69_irq_counter = 0;
        } else {
            Mapper_69_irq_counter -= 113;
        }
    }
}
