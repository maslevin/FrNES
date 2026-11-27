#include <kos.h>

#include "cpu.h"
#include "memory.h"
#include "apu.h"
#include "ppu.h"
#include "mapper.h"

// a $2004 read while rendering sees what OAM is doing this many dots past ppu_oam_busy_dot() (the end of the read,
// calibrated against Mesen 2.2.1 with AccuracyCoin's $2004 Stress Test)
#ifndef PPU_OAM_READ_EXTRA_DOTS
#define PPU_OAM_READ_EXTRA_DOTS -1
#endif

// a $2007 read while rendering reloads its buffer from the PPU bus this many dots past ppu_oam_busy_dot()
// (Mesen 2.2.1 loads it 5 dots after the read: AccuracyCoin's $2007 Stress Test, SMB3's status bar read)
#ifndef PPU_DATA_READ_EXTRA_DOTS
#define PPU_DATA_READ_EXTRA_DOTS 4
#endif

// Reads the PPU bus for $0000-$3EFF
static inline uint8 ppu_bus_read(uint16 address) {
    if (address < 0x2000) {
        return ((uint8_t*)PATTERN_TABLES_ADDR)[address];
    }
    return ((uint8_t*)NAMETABLE0_ADDR)[address & 0x0fff];
}

/*
 * CPU open bus: reading an address nothing drives returns the last value on the data bus. That is usually the high
 * byte of the address (the last operand byte fetched), except right after another read in the I/O range, e.g. the
 * dummy read of an indexed access that crossed a page: then it is the value that read put on the bus.
 */
// (also set by the dummy read of an indexed access over open bus, DUMMY_READ in cpu.c / dummy_io in 6502_core.s)
uint8_t cpu_io_bus_value;
uint32_t cpu_io_bus_cycle = 0xffffffff;
// the CPU's internal data bus: the same, except that $4015 reads only drive this one
uint8_t cpu_internal_bus_value;

// (DMA reads drive the bus too, cpu_exact.c keeps cpu_io_bus_value on every cycle)
static inline uint8 cpu_open_bus(uint16 address) {
    // (the bus as the previous cycle left it, or as another read on this cycle drove it: a DMA's)
    return ((uint32_t)(cpu_cycle_now - cpu_io_bus_cycle) <= 1) ? cpu_io_bus_value : (address >> 8);
}

FRNES_HOT(8_read) uint8 read_byte_6502_c(uint16 address) {
    uint8 result;
    uint8 bank_select = (address >> 13);
    // internal RAM, always read through the first mapping so mirrors don't alias in the SH4 operand cache
    if (bank_select == 0) {
        result = NES_WINDOW_BYTE(address & (RAM_SIZE - 1));
    // PPU read
    } else if (bank_select == 1) {
        // bring the PPU (and APU) up to the current CPU time first, unless this is a $2002 read none of whose flags can
        // change before now (ppu_read_sync)
        ppu_read_sync(address);
        // write only registers return the PPU I/O latch (the last value written to or read from the PPU)
        result = ppu_open_bus_read();
        // PPU registers are mirrored every 8 bytes through $3FFF
        switch (address & 0x0007) {
            // PPUSTATUS $2002
            case 0x2: {
                ppu_status_read_sync();
                // Set return value, the low 5 bits come from the I/O latch
                // (the Vs. System's RC2C05s, VS_PPU_STATUS_DRIVE set: the low 5 bits are the PPU's ID, VS_PPU_STATUS_ID)
                if (__builtin_expect(VS_PPU_STATUS_DRIVE, 0)) {
                    ppu_open_bus_drive(ppu_registers -> PPUSTATUS | VS_PPU_STATUS_ID, 0xff);
                } else {
                    ppu_open_bus_drive(ppu_registers -> PPUSTATUS, 0xe0);
                }
                result = ppu_registers -> PPUOPENBUS;

#ifdef AC_PPU_LOG
                {
                    extern uint8_t ac_ppu_log_on;
                    static uint8_t last_status = 0xff;
                    // (only changes, $2002 is polled in loops)
                    if (ac_ppu_log_on && (ppu_registers -> PPUSTATUS != last_status)) {
                        printf("R 2 %02X sl=%lu dot=%lu pc=%04X\n", ppu_registers -> PPUSTATUS, (unsigned long)ppu_current_scanline(),
                            (unsigned long)ppu_current_dot(), cpu_registers -> PC);
                    }
                    last_status = ppu_registers -> PPUSTATUS;
                }
#endif
                // Reset the V-Blank flag
                ppu_registers -> PPUSTATUS &= ~PPUSTATUS_VBLANK_FLAG;

                // Reset W register
                ppu_registers -> PPU_W = 0;
            } break;

            // OAMDATA $2004
            case 0x4: {
                ppu_sync();
                // while rendering uses OAM, reads see what it is doing (see ppu_oam_read_while_rendering)
                uint32_t busy_dot = ppu_oam_busy_dot();
                result = busy_dot ? ppu_oam_read_while_rendering((busy_dot + PPU_OAM_READ_EXTRA_DOTS) % 341) : ((uint8_t*)SPRRAM_ADDR)[ppu_registers -> OAMADDR];
                ppu_open_bus_drive(result, 0xff);
            } break;

            // PPUDATA $2007
            case 0x7: {
                // a read on the cycle right after another one isn't taken (Mesen 2: the PPU ignores $2007 reads for 6
                // dots after one), it only sees the I/O latch: the repeated reads of a DMA halted LDA $2007
                static uint32_t ppu_data_read_cycle = 0x80000000;
                if ((uint32_t)(cpu_cycle_now - ppu_data_read_cycle) == 1) {
                    result = ppu_registers -> PPUOPENBUS;
                    break;
                }
                ppu_data_read_cycle = cpu_cycle_now;
                ppu_sync();
                uint16 addr = ppu_registers -> PPU_V & 0x3fff;
                // while rendering, the buffer reloads from whatever the PPU is fetching a few dots later
                uint32_t busy_dot = ppu_oam_busy_dot();
                if (busy_dot) {
                    addr = ppu_rendering_bus_address((busy_dot + PPU_DATA_READ_EXTRA_DOTS) % 341) & 0x3fff;
                }
                // (the buffer loads from addr before the mapper sees the address change: an MMC2 latch it trips
                // switches the bank for later fetches, as in Mesen 2)
                ppu_data_increment();
                if (addr < 0x3f00) {
                    // reads are delayed through the read buffer
                    result = ppu_registers -> PPUDATA;
                    ppu_registers -> PPUDATA = ppu_bus_read(addr);
                } else {
                    // palette reads are immediate, the top 2 bits come from the I/O latch, and the
                    // read buffer is filled with the nametable byte "underneath" the palette
                    uint8 index = addr & 0x1f;
                    if ((index & 0x13) == 0x10) {
                        index &= ~0x10;
                    }
                    // (greyscale mode applies to reads too: only the top two bits of the color come back)
                    result = (((uint8_t*)PALETTE_RAM_ADDR)[index] & ((ppu_registers -> PPUMASK & PPUMASK_GREYSCALE) ? 0x30 : 0x3f)) |
                        (ppu_registers -> PPUOPENBUS & 0xc0);
                    ppu_registers -> PPUDATA = ppu_bus_read(addr & 0x2fff);
                }
                ppu_bus_address(addr);
                ppu_bus_address(ppu_registers -> PPU_V);
                // (palette reads leave the top two bits undriven)
                ppu_open_bus_drive(result, (addr < 0x3f00) ? 0xff : 0x3f);
            } break;
        }
    // APU / IO / mapper read
    } else if (bank_select == 2) {
        cpu_sync_devices_io(address);
// THIS IS JUST THE FRNES CODE FOR READING FROM APU / IO Registers
        switch (address) {
            // write only APU registers read as CPU open bus,
            // approximated as the high byte of the address since that was the last byte on the data bus
            case 0x4000 ... 0x4014: {
                result = cpu_open_bus(address);
            } break;

            // APU status, reading acknowledges the frame counter IRQ
            // (bit 5 is open bus, the internal one)
            case 0x4015: {
                result = apu_read_status((cpu_cycle_now == cpu_io_bus_cycle + 1) ? cpu_internal_bus_value : (address >> 8));
            } break;

            // Controller ports, buttons shift out A, B, Select, Start, Up, Down, Left, Right then read as 1
            // bits 5-7 are open bus
            case 0x4016:
            case 0x4017: {
                // (port n's latch, bit and extra bits: PAD1_ / PAD2_, 2 bytes apart, the extras 1 apart)
                uint32_t n = address & 1;
                uint8_t* latch = &cpu_registers -> PAD1_Latch + 2 * n;
                if (cpu_registers -> PAD_Strobe) {
                    latch[1] = 0;
                }
                // the latch's 8 bits (and the Famicom four player adapter's bit 1), then 24 more from the tail (1s for a
                // pad), then 1s
                uint32_t bit = latch[1];
                result = (bit < 8) ? (((latch[0] >> bit) & 1) | (((PAD_INPUT -> d1[n] >> bit) & 1) << 1)) :
                    (~(PAD_INPUT -> tail_inverted[n] >> (bit - 8)) & 1);
                if (!(cpu_registers -> PAD_Strobe) && (bit < 32)) {
                    latch[1] = bit + 1;
                }
                // (the Vs. System's DIP switches and coins in place of open bus bits)
                result |= (cpu_open_bus(address) & cpu_registers -> PAD_OpenMask) | (&cpu_registers -> PAD1_Extra)[n];
            } break;

            // cartridge space $4020-$5FFF (MMC5's registers and ExRAM), open bus where the mapper has nothing
            default: {
                result = (address >= 0x4020) ? mapper_read_low(address, cpu_open_bus(address)) : cpu_open_bus(address);
            } break;
        }
    // a mapper answering $6000-$7FFF reads itself (Bandai's EEPROM output, a VRC2's latch on a board without RAM)
    } else if ((bank_select == 3) && __builtin_expect(mapper_6000_reads, 0)) {
        result = mapper_read_6000(address, cpu_open_bus(address));
    // WRAM is not mapped when the cartridge has none
    } else if ((bank_select == 3) && (WRAM == NULL)) {
        result = cpu_open_bus(address);
    // general memory space read
    } else {
        result = NES_WINDOW_BYTE(address);
    }

    // ($4015 is inside the CPU: reading it doesn't drive the external data bus, which keeps its value, usually the
    // high byte of the address fetched just before)
    if ((bank_select == 1) || (bank_select == 2)) {
        if (address != 0x4015) {
            cpu_io_bus_value = result;
        } else if (cpu_cycle_now != cpu_io_bus_cycle + 1) {
            cpu_io_bus_value = address >> 8;
        }
        cpu_internal_bus_value = result;
        cpu_io_bus_cycle = cpu_cycle_now;
    }
    return result;
}