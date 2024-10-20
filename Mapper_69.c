#include "Mapper_69.h"

#include "nes_apu.h"

uint8 Mapper_69_regs[1];
uint8 Mapper_69_irq_enabled;
int32 Mapper_69_irq_counter;

/////////////////////////////////////////////////////////////////////
// Mapper 69
void Mapper_69_Init() {
    // Init ExSound
    apu_set_exsound(NES_APU_EXSOUND_FME7);

	BankTable[4] = ROM_pages[0];
	BankTable[5] = ROM_pages[1];
    BankTable[6] = ROM_pages[ num_8k_ROM_pages - 2];
    BankTable[7] = ROM_pages[ num_8k_ROM_pages - 1];

    for ( int nPage = 0; nPage < 8; ++nPage )
        PPUBANK[ nPage ] = VROM_pages[ nPage ];

    Mapper_69_irq_enabled = 0;
    Mapper_69_irq_counter = 0;
}

void Mapper_69_Write(uint16 addr, uint8 data) {
    switch(addr & 0xE000) {
        case 0x8000: {
            Mapper_69_regs[0] = data & 0x0f;
            pNesX_DebugPrint("Map69: Setting Command to [%u]\n", Mapper_69_regs[0]);
        } break;

        case 0xA000: {
            switch (Mapper_69_regs[0]) {
                case 0x00: {
                    pNesX_DebugPrint("Map69: Set PPUBANK0 to Page [%u]\n", data);
                    PPUBANK[0] = VROM_pages[ data ];
                } break;

                case 0x01: {
                    pNesX_DebugPrint("Map69: Set PPUBANK1 to Page [%u]\n", data);
                    PPUBANK[1] = VROM_pages[ data ];
                } break;

                case 0x02: {
                    pNesX_DebugPrint("Map69: Set PPUBANK2 to Page [%u]\n", data);                    
                    PPUBANK[2] = VROM_pages[ data ];
                } break;

                case 0x03: {
                    pNesX_DebugPrint("Map69: Set PPUBANK3 to Page [%u]\n", data);                    
                    PPUBANK[3] = VROM_pages[ data ];
                } break;

                case 0x04: {
                    pNesX_DebugPrint("Map69: Set PPUBANK4 to Page [%u]\n", data);                    
                    PPUBANK[4] = VROM_pages[ data ];
                } break;

                case 0x05: {
                    pNesX_DebugPrint("Map69: Set PPUBANK5 to Page [%u]\n", data);                    
                    PPUBANK[5] = VROM_pages[ data ];
                } break;

                case 0x06: {
                    pNesX_DebugPrint("Map69: Set PPUBANK6 to Page [%u]\n", data);                    
                    PPUBANK[6] = VROM_pages[ data ];
                } break;

                case 0x07: {
                    pNesX_DebugPrint("Map69: Set PPUBANK7 to Page [%u]\n", data);
                    PPUBANK[7] = VROM_pages[ data ];
                } break;

                case 0x08: {
                    if(!(data & 0x40)) {
                        //printf("Map69: Set 0x6000-0x7FFF to Rom Page [%u]\n", data & 0x1f);
                        BankTable[3] = ROM_pages[(data & 0x1f) % num_8k_ROM_pages];
                    }
                } break;

                case 0x09: {                
                    pNesX_DebugPrint("Map69: Set BankTable[4] to Rom Page [%u]\n", data & 0x1f);
                    BankTable[4] = ROM_pages[(data & 0x1f) % num_8k_ROM_pages];
                } break;

                case 0x0A: {
                    pNesX_DebugPrint("Map69: Set BankTable[5] to Rom Page [%u]\n", data & 0x1f);                    
                    BankTable[5] = ROM_pages[(data & 0x1f) % num_8k_ROM_pages];
                } break;

                case 0x0B: {
                    pNesX_DebugPrint("Map69: Set BankTable[6] to Rom Page [%u]\n", data & 0x1f);                    
                    BankTable[6] = ROM_pages[(data & 0x1f) % num_8k_ROM_pages];
                } break;

                case 0x0C: {
                    data &= 0x03;
                    if (data == 0) {
                        pNesX_DebugPrint("Map69: Set Mirroring to Vertical\n");
                        pNesX_Mirroring(MIRRORING_VERTICAL);
                    } else if(data == 1) {
                        pNesX_DebugPrint("Map69: Set Mirroring to Horizontal\n");
                        pNesX_Mirroring(MIRRORING_HORIZONTAL);
                    } else if(data == 2) {
                        pNesX_DebugPrint("Map69: Set Mirroring to Single Screen Low\n");
                        pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_LOW);
                    } else {
                        pNesX_DebugPrint("Map69: Set Mirroring to Single Screen High\n");
                        pNesX_Mirroring(MIRRORING_SINGLE_SCREEN_HIGH);
                    }
                } break;

                case 0x0D: {
                    pNesX_DebugPrint("Map69: Set Irq Enabled to [%u]\n", data);
                    Mapper_69_irq_enabled = data;
                } break;

                case 0x0E: {
                    pNesX_DebugPrint("Map69: Set Irq Counter Low Byte to [%u]\n", data);
                    Mapper_69_irq_counter = (Mapper_69_irq_counter & 0xFF00) | data;
                } break;

                case 0x0F: {
                    pNesX_DebugPrint("Map69: Set Irq Counter High Byte to [%u]\n", data);
                    Mapper_69_irq_counter = (Mapper_69_irq_counter & 0x00FF) | (data << 8);
                } break;
            }
        } break;

        case 0xC000:
        case 0xE000: {
            ex_write(addr, data);
        } break;
    }
}

void Mapper_69_HSync() {
    if (Mapper_69_irq_enabled) {
        if (Mapper_69_irq_counter < 113) {
            set_irq(true);
            Mapper_69_irq_counter = 0;
        } else {
            Mapper_69_irq_counter -= 113;
        }
    }
}
