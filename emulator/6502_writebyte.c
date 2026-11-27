#include <kos.h>

#include "cpu.h"
#include "memory.h"
#include "cartridge.h"
#include "input.h"
#include "apu.h"
#include "mapper.h"
#include "apu_synth.h"
#include "ppu.h"

FRNES_HOT(9_write) void write_byte_6502_c(uint16 address, uint8 data) {
    uint8 bank_select = (address >> 13);
    // internal RAM, always written through the first mapping so mirrors don't alias in the SH4 operand cache
    if (bank_select == 0) {
        NES_WINDOW_BYTE(address & (RAM_SIZE - 1)) = data;
    // PPU write
    } else if (bank_select == 1) {
        // (the Vs. System's RC2C05s: $2000 and $2001 swapped, out of line in vs_ppu_write)
        if (__builtin_expect(VS_PPU_SWAP, 0)) {
            vs_ppu_write(address, data);
            return;
        }
        // bring the PPU (and APU) up to the current CPU time, then a lazily rendered scanline up to date before
        // its rendering state changes
        cpu_sync_devices();
        // (a $2000 write that only flips the NMI enable doesn't change rendering: MMC3 games' IRQ handlers do that
        // around mid-frame bank switches, Pinbot 40 times a frame)
        // ($2005 mid line: the second write and fine X changes don't need the line replayed, nor do a $2000 write
        // changing only the nametable bits and a $2001 one switching only the sprites or the emphasis: ppu_sync_lazy_write)
        // (and ppu_sync() has nothing to do unless the line is rendered lazily: none of this with rendering off)
        if ((ppu_line_mode == PPU_LINE_LAZY) &&
            (((address & 7) != 0) || ((ppu_registers -> PPUCTRL ^ data) & ~PPUCTRL_VBLANK_NMI_ENABLE)) &&
            !ppu_sync_lazy_write(address & 7, data)) {
            ppu_sync();
        }
        // every write to a PPU register refreshes the PPU I/O latch
        ppu_open_bus_drive(data, 0xff);
#ifdef AC_PPU_LOG
        {
            // test builds: PPU register writes with the scanline / dot they land on (test_main.c sets ac_ppu_log_on)
            extern uint8_t ac_ppu_log_on;
            if (ac_ppu_log_on) {
                printf("W %X %02X sl=%lu dot=%lu pc=%04X\n", address & 7, data, (unsigned long)ppu_current_scanline(),
                    (unsigned long)ppu_current_dot(), cpu_registers -> PC);
            }
        }
#endif
        switch (address & 0x0007) {
            case 0x0: {
                // enabling NMI while the vblank flag is still set triggers an NMI. Interrupts are polled before an
                // instruction's last cycle, so after a write on the last cycle it is taken one instruction later
                // (2 = delayed; a read-modify-write's first write makes it 1 again, see RMW)
                if (!(ppu_registers -> PPUCTRL & PPUCTRL_VBLANK_NMI_ENABLE) && (data & PPUCTRL_VBLANK_NMI_ENABLE) && ppu_vblank_flag_for_nmi()) {
                    cpu_registers -> NMI_Request = 2;
                }
                if ((ppu_registers -> PPUCTRL & PPUCTRL_VBLANK_NMI_ENABLE) && !(data & PPUCTRL_VBLANK_NMI_ENABLE)) {
                    ppu_nmi_disable_race();
                }
                if (__builtin_expect((ppu_registers -> PPUCTRL ^ data) & PPUCTRL_SPRITE_SIZE, 0)) {
                    mapper_ppuctrl_written(ppu_registers -> PPUCTRL, data);
                }
                ppu_registers -> PPUCTRL = data;
                ppu_registers -> PPU_T = (ppu_registers -> PPU_T & 0xf3ff) | (((uint16_t)data & 0x03) << 10);
            } break;
            
            case 0x01: {
                if ((ppu_registers -> PPUMASK ^ data) & PPUMASK_GREYSCALE) {
                    ppu_palette_dirty = PPU_PALETTE_DIRTY_ALL;
                }
                ppu_rendering_switched(ppu_registers -> PPUMASK, data);
                ppu_registers -> PPUMASK = data;
            } break;

            case 0x03: {
                ppu_registers -> OAMADDR = data;
            } break;

            case 0x04: {
                // while rendering uses OAM, a write doesn't reach it and bumps the high 6 bits of the address instead
                if (ppu_oam_busy_dot()) {
                    ppu_registers -> OAMADDR = (ppu_registers -> OAMADDR + 4) & 0xfc;
                    break;
                }
                // attribute bytes have no bits 2-4
                ((uint8_t*)SPRRAM_ADDR)[ppu_registers -> OAMADDR] = ((ppu_registers -> OAMADDR & 3) == 2) ? (data & 0xe3) : data;
                ppu_oam_generation++;
                ppu_registers -> OAMADDR++;
            } break;

            case 0x05: {
                if (ppu_registers -> PPU_W) {
                    ppu_registers -> PPU_T = (ppu_registers -> PPU_T & 0x0c1f) | (((uint16_t)data & 0xf8) << 2) | (((uint16_t)data & 0x07) << 12);
                } else {
                    ppu_registers -> PPU_T = (ppu_registers -> PPU_T & 0xffe0) | (data >> 3);
                    ppu_registers -> PPU_X = (data & 0x07);
                }
                ppu_registers -> PPU_W ^= 1;
            } break;

            case 0x06: {
                if (ppu_registers -> PPU_W) {
                    ppu_blank_address_write();
                    ppu_registers -> PPU_T = (ppu_registers -> PPU_T & 0xff00) | data;
                    ppu_registers -> PPU_V = ppu_registers -> PPU_T;
                    ppu_bus_address(ppu_registers -> PPU_V);
                } else {
                    ppu_registers -> PPU_T = (ppu_registers -> PPU_T & 0x00ff) | (((uint16_t)data & 0x3f) << 8);
                }
                ppu_registers -> PPU_W ^= 1;                
            } break;

            case 0x07: {
                //TODO: check whether we are rendering or not to handle strange v-changes
                ppu_blank_data_write(data);
                uint16_t addr = ppu_registers -> PPU_V & 0x3fff;
                ppu_bus_address(addr);
                if (addr >= 0x3f00) {
                    // $3F10/$3F14/$3F18/$3F1C mirror the background color entries, palette entries are 6 bits
                    uint8_t index = addr & 0x1f;
                    if ((index & 0x13) == 0x10) {
                        index &= ~0x10;
                    }
                    uint8_t* entry = &((uint8_t*)PALETTE_RAM_ADDR)[index];
                    if (*entry != (data & 0x3f)) {
                        *entry = data & 0x3f;
                        // the backdrop color is in every background palette's colors, entries 4 / 8 / 12 are never shown
                        ppu_palette_dirty |= (index >= 0x10) ? PPU_PALETTE_DIRTY_SPRITES :
                            ((index == 0) ? PPU_PALETTE_DIRTY_ALL : (1 << (index >> 2)));
                    }
                } else if (addr >= 0x2000) {
                    ppu_nametable_write(addr, data);
                } else if ((num_1k_VROM_pages == 0) || (mapper_chr_ram_slots & (1 << (addr >> 10)))) {
                    // pattern tables are only writable where the cartridge has CHR RAM
                    ppu_chr_ram_write(addr, data);
                }
                if (ppu_registers -> PPUMASK & (PPUMASK_ENABLE_BACKGROUND_RENDERING | PPUMASK_ENABLE_SPRITE_RENDERING)) {
                    ppu_data_increment();
                } else {
                    ppu_registers -> PPU_V += (ppu_registers -> PPUCTRL & PPUCTRL_VRAM_INCREMENT) ? 32 : 1;
                }
                ppu_bus_address(ppu_registers -> PPU_V);
            } break;
        }
    // APU / Sprite DMA / mapper write
    } else if (bank_select == 2) {
        cpu_sync_devices_io(address);
        // sound registers are logged with their CPU cycle and replayed by the synthesizer at the end of the frame
        if ((address <= 0x4013) || (address == 0x4015) || (address == 0x4017)) {
            apu_synth_log_write(address, data, cpu_cycle_now);
            if (address != 0x4017) {
                apu_write_register(address, data);
            }
        }
        switch (address) {
            // OAM DMA, copies a 256 byte page into sprite RAM starting at OAMADDR and stalls the CPU
            // (the DMA starts on the CPU's next read, in cpu_exact.c: the batch ends after this instruction)
            case 0x4014: {
                cpu_dma_oam_start(data);
                cpu_batch_cut = 1;
            } break;

            // APU frame counter
            case 0x4017: {
                apu_write_frame_counter(data);
            } break;

            // Controller strobe, while bit 0 is high the controllers keep reloading their shift registers
            // The controllers only see the strobe line on APU put cycles, so a strobe that is high for just one cycle (an
            // RMW instruction writing $41 then $40) is missed when that cycle is a get cycle (odd, as AccuracyCoin's
            // Controller Strobing test aligns them against OAM DMA here)
            case 0x4016: {
                static uint32_t strobe_cycle;
                static uint8_t strobe_saved_bit1, strobe_saved_bit2;
                // the Vs. System's bank select (mapper 99) and coin counter outputs
                if (__builtin_expect(cpu_registers -> VS_Flags, 0)) {
                    vs_write_4016(data);
                }
                uint8_t strobe = data & 1;
                if (strobe && !cpu_registers -> PAD_Strobe) {
                    strobe_cycle = cpu_cycle_now;
                    strobe_saved_bit1 = cpu_registers -> PAD1_Bit;
                    strobe_saved_bit2 = cpu_registers -> PAD2_Bit;
                } else if (!strobe && cpu_registers -> PAD_Strobe && (cpu_cycle_now == strobe_cycle + 1) && !CPU_GET_CYCLE(strobe_cycle)) {
                    cpu_registers -> PAD1_Bit = strobe_saved_bit1;
                    cpu_registers -> PAD2_Bit = strobe_saved_bit2;
                    cpu_registers -> PAD_Strobe = 0;
                    break;
                }
                // (the strobe's fall: the controllers' bits past the latches', the Four Score's; a Vs. protection's)
                if (__builtin_expect(cpu_registers -> PAD_Strobe && !strobe && PAD_INPUT -> strobe_fall, 0)) {
                    input_strobe_fall();
                }
                cpu_registers -> PAD_Strobe = strobe;
                if (strobe) {
                    cpu_registers -> PAD1_Bit = 0;
                    cpu_registers -> PAD2_Bit = 0;
                }
            } break;
/*            
            case 0x4000: {

            } break;

            case 0x4001: {

            } break;

            case 0x4002: {

            } break;

            case 0x4003: {

            } break;

            case 0x4004: {

            } break;

            case 0x4005: {

            } break;

            case 0x4006: {

            } break;

            case 0x4007: {

            } break;
            
            case 0x4008: {

            } break;

            case 0x4009: {

            } break;

            case 0x400A: {

            } break;

            case 0x400B: {

            } break;
            
            case 0x400C: {

            } break;

            case 0x400D: {

            } break;

            case 0x400E: {

            } break;

            case 0x400F: {

            } break;
            
            case 0x4010: {

            } break;

            case 0x4011: {

            } break;

            case 0x4012: {

            } break;

            case 0x4013: {

            } break;

            case 0x4014: {

            } break;

            case 0x4015: {

            } break;

            case 0x4016: {

            } break;

            case 0x4017: {

            } break;
*/             

            // cartridge space $4020-$5FFF
            default: {
                if (address >= 0x4020) {
                    mapper_write_low(address, data);
                }
            } break;
        }
    // general memory space write, RAM and WRAM only (ROM is not writable until mappers are implemented)
    } else if (address < 0x8000) {
        // $6000-$7FFF: PRG RAM, unless the mapper has ROM there or registers (mapper_6000_mode); any write marks it
        // for the player's battery autosave
        prg_ram_written = 1;
        if (__builtin_expect(mapper_6000_mode, 0)) {
            if (mapper_6000_mode == MAPPER_6000_REGISTERS) {
                cpu_sync_devices();
                mapper_write_6000(address, data);
            }
        } else if (WRAM != NULL) {
            NES_WINDOW_BYTE(address) = data;
        }
    // ROM space writes go to the mapper, which catches the PPU and APU up first (mapper_synced_write; MMC1 only for
    // the writes that change something) and brings a lazily rendered scanline up to date (ppu_sync) only for writes
    // that change what is rendered
    } else {
#ifdef AC_PPU_LOG
        {
            // (and the mapper's)
            extern uint8_t ac_ppu_log_on;
            if (ac_ppu_log_on) {
                printf("M %04X %02X sl=%lu dot=%lu pc=%04X\n", address, data, (unsigned long)ppu_current_scanline(),
                    (unsigned long)ppu_current_dot(), cpu_registers -> PC);
            }
        }
#endif
        mapper_write_fn(address, data);
    }

}