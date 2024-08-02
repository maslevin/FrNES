#include "Mapper_85.h"

#include "nes_apu.h"

unsigned char Mapper_85_irq_enabled;
unsigned char Mapper_85_irq_counter;
unsigned char Mapper_85_irq_latch;

void Mapper_85_Init() {
    apu_set_exsound(NES_APU_EXSOUND_VRC7);

    // set CPU bank pointers
    ROMBANK0 = ROMPAGE(0);
    ROMBANK1 = ROMPAGE(1);
    ROMBANK2 = ROMLASTPAGE(1);
    ROMBANK3 = ROMLASTPAGE(0);

    if ( NesHeader.byVRomSize > 0 ) {
        for ( int nPage = 0; nPage < 8; ++nPage )
            PPUBANK[ nPage ] = &VROM[ nPage * 0x400 ];
    } else {
        for ( int nPage = 0; nPage < 8; ++nPage )
            PPUBANK[ nPage ] = &VRAM[ nPage * 0x400 ]; 
    }

    Mapper_85_irq_enabled = 0;
    Mapper_85_irq_counter = 0;
    Mapper_85_irq_latch = 0;
}

void Mapper_85_Write(uint16 addr, uint8 data) {
    switch(addr & 0xF038) {
        case 0x8000: {
            ROMBANK0 = ROMPAGE(data);
        } break;

        case 0x8008:
        case 0x8010: {
            ROMBANK1 = ROMPAGE(data);
        } break;

        case 0x9000: {
            ROMBANK2 = ROMPAGE(data);
        }
        break;

        case 0x9010:
        case 0x9030: {
            ex_write(addr, data);
        } break;

        case 0xA000: {
            if (NesHeader.byVRomSize > 0) {
                PPUBANK[ 0 ] = &VROM[ 0 * 0x400 ];
            } else {
                PPUBANK[ 0 ] = &VRAM[ 0 * 0x400 ];
            }
        } break;

        case 0xA008:
        case 0xA010: {
            if (NesHeader.byVRomSize > 0) {
                PPUBANK[ 1 ] = &VROM[ 1 * 0x400 ];
            } else {
                PPUBANK[ 1 ] = &VRAM[ 1 * 0x400 ];
            }
        } break;

        case 0xB000: {
            if (NesHeader.byVRomSize > 0) {
                PPUBANK[ 2 ] = &VROM[ 2 * 0x400 ];
            } else {
                PPUBANK[ 2 ] = &VRAM[ 2 * 0x400 ];
            }
        } break;

        case 0xB008:
        case 0xB010: {
            if (NesHeader.byVRomSize > 0) {
                PPUBANK[ 3 ] = &VROM[ 3 * 0x400 ];
            } else {
                PPUBANK[ 3 ] = &VRAM[ 3 * 0x400 ];
            }
        } break;

        case 0xC000: {
            if (NesHeader.byVRomSize > 0) {
                PPUBANK[ 4 ] = &VROM[ 4 * 0x400 ];
            } else {
                PPUBANK[ 4 ] = &VRAM[ 4 * 0x400 ];
            }
        } break;

        case 0xC008:
        case 0xC010: {
            if (NesHeader.byVRomSize > 0) {
                PPUBANK[ 5 ] = &VROM[ 5 * 0x400 ];
            } else {
                PPUBANK[ 5 ] = &VRAM[ 5 * 0x400 ];
            }
        } break;

        case 0xD000: {
            if (NesHeader.byVRomSize > 0) {
                PPUBANK[ 6 ] = &VROM[ 6 * 0x400 ];
            } else {
                PPUBANK[ 6 ] = &VRAM[ 6 * 0x400 ];
            }
        } break;

        case 0xD008:
        case 0xD010: {
            if (NesHeader.byVRomSize > 0) {
                PPUBANK[ 7 ] = &VROM[ 7 * 0x400 ];
            } else {
                PPUBANK[ 7 ] = &VRAM[ 7 * 0x400 ];
            }
        } break;

        case 0xE000: {
            data &= 0x03;
            if (data == 0x00) {
                pNesX_Mirroring(MIRRORING_VERTICAL);
            } else if(data == 0x01) {
                pNesX_Mirroring(MIRRORING_HORIZONTAL);
            } else if(data == 0x02) {
                pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_LOW);
            } else {
                pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_HIGH);
            }
        } break;

        case 0xE008:
        case 0xE010: {
            Mapper_85_irq_latch = data;
        } break;

        case 0xF000: {
            Mapper_85_irq_enabled = data & 0x03;
            if (Mapper_85_irq_enabled & 0x02) {
                Mapper_85_irq_counter = Mapper_85_irq_latch;
            }
        } break;

        case 0xF008:
        case 0xF010: {
            Mapper_85_irq_enabled = (Mapper_85_irq_enabled & 0x01) * 3;
        } break;
    }
}

void Mapper_85_HSync() {
    if (Mapper_85_irq_enabled & 0x02) {
        if (Mapper_85_irq_counter == 0xFF) {
            IRQ_REQ;
            Mapper_85_irq_counter = Mapper_85_irq_latch;
        } else {
            Mapper_85_irq_counter++;
        }
    }
}

