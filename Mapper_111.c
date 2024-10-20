#include "Mapper_111.h"

// Mapper 111 support ported from NesterDC

void set_gtrom_bank(uint8 prg) {
    uint8 base = prg << 2;
    BankTable[4] = ROM_pages[base];
    BankTable[5] = ROM_pages[base + 1];
    BankTable[6] = ROM_pages[base + 2];
    BankTable[7] = ROM_pages[base + 3];
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
    uint8 chr = ((data & 0x10) >> 4) << 3;
    uint8 nametable = (data & 0x20) >> 5;

    set_gtrom_bank(prg);

    for (int i = 0; i < 8; i++) {
        PPUBANK[ i ] = VROM_pages[i + chr];
    }

    nametable <<= 3;
    PPUBANK[8] = &PPURAM[(NAME_TABLE0 + nametable) * 0x400];
    PPUBANK[9] = &PPURAM[(NAME_TABLE0 + nametable + 1) * 0x400];
    PPUBANK[10] = &PPURAM[(NAME_TABLE0 + nametable + 2) * 0x400];
    PPUBANK[11] = &PPURAM[(NAME_TABLE0 + nametable + 3) * 0x400];
    PPUBANK[12] = &PPURAM[(NAME_TABLE0 + nametable + 4) * 0x400];
    PPUBANK[13] = &PPURAM[(NAME_TABLE0 + nametable + 5) * 0x400];
    PPUBANK[14] = &PPURAM[(NAME_TABLE0 + nametable + 6) * 0x400];
    PPUBANK[15] = &PPURAM[(NAME_TABLE0 + nametable + 7) * 0x400];
}
