#include <kos.h>
#include "memory.h"

/*
 * MMC3 (mappers 4, 118, 119): a register write catches the devices up only when the PPU or the scanline counter sees
 * it: a CHR mode, CHR bank or mirroring change, the IRQ registers. Kirby's Adventure switches PRG banks ~60 times a
 * frame, each a catch-up before (a PRG bank isn't seen by the PPU or the APU: a DMC fetch reads ROM at its own cycle,
 * the CPU's). mmc3_write as it was, after mapper_synced_write in the "c" slot; built without LTO (as
 * frame_hud_perf.c), so mapper.c's link time optimized partitions, and the code they place, stay as they were
 */
extern uint8_t mmc3_sync_bank_select, mmc3_sync_registers[8], mmc3_sync_mirroring;
void mmc3_sync_write(uint16_t address, uint8_t data);
void cpu_sync_devices(void);

FRNES_HOT(c_write_sync_mmc3) void mapper_mmc3_synced_write(uint16_t address, uint8_t data) {
    uint32_t reg = address & 0xe001;
    if (!(((reg == 0x8000) && !((mmc3_sync_bank_select ^ data) & 0x80)) ||
        ((reg == 0x8001) && (((mmc3_sync_bank_select & 7) >= 6) || (mmc3_sync_registers[mmc3_sync_bank_select & 7] == data))) ||
        ((reg == 0xa000) && ((data & 1) == mmc3_sync_mirroring)) || (reg == 0xa001))) {
        cpu_sync_devices();
    }
    mmc3_sync_write(address, data);
}
