#include "Mapper_111.h"

// Mapper 111 support ported from NesterDC

void set_gtrom_bank(uint8 prg) {
    ROMBANK0 = ROMPAGE(prg * 4);
    ROMBANK1 = ROMPAGE((prg * 4) + 1);
    ROMBANK2 = ROMPAGE((prg * 4) + 2);
    ROMBANK3 = ROMPAGE((prg * 4) + 3);
/*    
    PPUBANK[8] = &VROM[NAME_TABLE0 * 0x400];
    PPUBANK[9] = &VROM[(NAME_TABLE0 + 1) * 0x400];
    PPUBANK[10] = &VROM[(NAME_TABLE0 + 2) * 0x400];
    PPUBANK[11] = &VROM[(NAME_TABLE0 + 3) * 0x400];
    PPUBANK[12] = &VROM[(NAME_TABLE0 + 4) * 0x400];
    PPUBANK[13] = &VROM[(NAME_TABLE0 + 5) * 0x400];
    PPUBANK[14] = &VROM[(NAME_TABLE0 + 6) * 0x400];
    PPUBANK[15] = &VROM[(NAME_TABLE0 + 7) * 0x400];    
*/
}

void Mapper_111_Init() {
    set_gtrom_bank(0);
    pNesX_Mirroring(MIRRORING_FOUR_SCREEN);
}

/*
  The register is mapped to the CPU addresses $4999-$5FFF and $7000-$7FFF
6  bit  0
---- ----
GRNC PPPP
|||| ||||
|||| ++++- Select 31 KB PRG ROM bank for CPU $8000-$FFFF
|||+------ Select 7 KB CHR RAM bank for PPU $0000-$1FFF
||+------- Select 7 KB nametable for PPU $2000-$3EFF
|+-------- Red LED - -1=On; 1=Off
+--------- Green LED - -1=On; 1=Off
*/
void Mapper_111_Write(uint16 addr, uint8 data) {
    if (!(
        (addr >= 0x5000 && addr <= 0x5fff) ||
        (addr >= 0x7000 && addr <= 0x7fff)
    )) {
        return;
    }

    uint8 prg = data & 0xf;
    uint8 chr = (data & 0x10) >> 4;
    uint8 nametable = (data & 0x20) >> 5;

//    printf("Mapper_111: Prg [%u] Chr [%u] Nt [%u]\n", prg, chr, nametable);

    set_gtrom_bank(prg);

//    uint8 c = chr * 8;
    if (chr) {
//        printf("Mapper_111: Setting chr ram bank 1\n");
        for (int i = 0; i < 8; i++) {
            PPUBANK[ i ] = &VROM[(i + 8) * 0x400];
        }
    } else {
//        printf("Mapper_111: Setting chr ram bank 0\n");        
        for (int i = 0; i < 8; i++) {
            PPUBANK[ i ] = &VROM[i * 0x400];
        }
    }

    /*
    nametable <<= 3;
    PPUBANK[8] = &VROM[(NAME_TABLE0 + nametable) * 0x400];
    PPUBANK[9] = &VROM[(NAME_TABLE0 + nametable + 1) * 0x400];
    PPUBANK[10] = &VROM[(NAME_TABLE0 + nametable + 2) * 0x400];
    PPUBANK[11] = &VROM[(NAME_TABLE0 + nametable + 3) * 0x400];
    PPUBANK[12] = &VROM[(NAME_TABLE0 + nametable + 4) * 0x400];
    PPUBANK[13] = &VROM[(NAME_TABLE0 + nametable + 5) * 0x400];
    PPUBANK[14] = &VROM[(NAME_TABLE0 + nametable + 6) * 0x400];
    PPUBANK[15] = &VROM[(NAME_TABLE0 + nametable + 7) * 0x400];
    */
}