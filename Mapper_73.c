#include "Mapper_73.h"

uint8 Mapper_73_irq_enabled;
uint32 Mapper_73_irq_counter;

void Mapper_73_Init() {
	ROMBANK0 = ROM_pages[0];
	ROMBANK1 = ROM_pages[1];
    ROMBANK2 = ROM_pages[ num_8k_ROM_pages - 2];
    ROMBANK3 = ROM_pages[ num_8k_ROM_pages - 1];

    Mapper_73_irq_counter = 0;
    Mapper_73_irq_enabled = 0;
}

void Mapper_73_Write( uint16 wAddr, unsigned char byData ) {
    switch (wAddr) {
        case 0x8000: {
            Mapper_73_irq_counter = (Mapper_73_irq_counter & 0xFFF0) | (byData & 0x0F);
        } break;

        case 0x9000: {
            Mapper_73_irq_counter = (Mapper_73_irq_counter & 0xFF0F) | ((byData & 0x0F) << 4);
        } break;

        case 0xA000: {
            Mapper_73_irq_counter = (Mapper_73_irq_counter & 0xF0FF) | ((byData & 0x0F) << 8);
        } break;

        case 0xB000: {
            Mapper_73_irq_counter = (Mapper_73_irq_counter & 0x0FFF) | ((byData & 0x0F) << 12);
        } break;

        case 0xC000: {
            Mapper_73_irq_enabled = byData;
        } break;

        case 0xF000: {
            ROMBANK0 = ROM_pages[(byData << 1) % num_8k_ROM_pages];
            ROMBANK1 = ROM_pages[((byData << 1) +1) % num_8k_ROM_pages];
        } break;
    }
}

void Mapper_73_HSync() {
    if (Mapper_73_irq_enabled & 0x02) {
        if (Mapper_73_irq_counter > 0xFFFF - 114) {
            IRQ_REQ;
            Mapper_73_irq_enabled = 0;
        } else {
            Mapper_73_irq_counter += 114;
        }
    }
}