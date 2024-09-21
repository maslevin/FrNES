#include "Mapper_111.h"

// Mapper 111 support ported from NesterDC

void set_gtrom_bank(uint8 prg) {
    ROMBANK0 = ROMPAGE(prg * 4);
    ROMBANK1 = ROMPAGE((prg * 4) + 1);
    ROMBANK2 = ROMPAGE((prg * 4) + 2);
    ROMBANK3 = ROMPAGE((prg * 4) + 3);
}

void Mapper_111_Init() {
    set_gtrom_bank(0);
//    pNesX_Mirroring(MIRRORING_FOUR_SCREEN);
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

    set_gtrom_bank(prg);

    uint8 c = chr * 8;
    for (int i = 0; i < 8; i++) {
        PPUBANK[ i ] = &VROM[(i + c) * 0x400];
    }
//    PPUBANK[8] = &PPURAM[(NAME_TABLE0 + nametable) * 0x400];
}
