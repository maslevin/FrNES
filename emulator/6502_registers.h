#ifndef __6502_REGISTERS_H
#define __6502_REGISTERS_H

#include <kos.h>
#include <stddef.h>

typedef struct {
    int32_t clocks;
    uint16_t PC;
    uint8_t SP;
    uint8_t S;
    uint8_t A;
    uint8_t X;
    uint8_t Y;
    uint8_t NMI_Request;
    uint8_t IRQ_Request;
    uint8_t PAD1_Latch;
    uint8_t PAD1_Bit;
    uint8_t PAD2_Latch;
    uint8_t PAD2_Bit;
    uint8_t PAD_Strobe;
    uint16_t DMA_Cycles;
    // CLI, SEI and PLP change the I flag after the CPU has polled for interrupts, so the next poll still sees the
    // old value: IRQ_POLL_I_SET / IRQ_POLL_I_CLEAR (0 = use the I flag)
    uint8_t IRQ_Poll;
    // the Vs. System (mapper_boards.c, vs_*): VS_SYSTEM when the game is one, set up at its load
    uint8_t VS_Flags;
    // the bits a $4016 / $4017 read takes from the open bus (0xe0 on the NES) and the ones each port adds (the Vs.
    // System's DIP switches, coins and service button, 0 on the NES)
    uint8_t PAD_OpenMask;
    uint8_t PAD1_Extra;
    uint8_t PAD2_Extra;
    // the Vs. System's DIP switches (vs_set_dip_switches) and its last $4016 write (mapper 99's bank select, bit 2)
    uint8_t VS_Dips;
    uint8_t VS_4016;
    // a $2002 poll loop's iterations the read just made skips (ppu_poll_skip): cycles the read's instruction takes on
    // top of its own, added by the core that made the read (6502_core.s io_read, cpu.c's IO_READ / IO_READ_AT)
    uint16_t READ_SKIP;
    // the PC after the instruction making an I/O read, from those cores (0 from cpu_exact.c's reads: no skip)
    uint16_t IO_PC;
} _6502_registers_t;

// the controllers past the latches' 8 bits (input.h, mapper_boards.c), in the register page past the Zapper's state
// (+0x40 and +0x58): each port's next 24 bits inverted (the Four Score's players 3 / 4 and its signature; 0, all 1s,
// for a pad: the register page's zeroing at each load), taken at the strobe's fall (input_strobe_fall); the Famicom
// four player adapter's bit 1 latches; players 3 and 4's buttons; the game's four players kind
typedef struct {
    uint32_t tail_inverted[2];
    uint8_t d1[2];
    uint8_t pad3, pad4;
    uint8_t four_score;
    // the Zappers turned off in the game (zapper_set_enabled: bit n for NES port n; 0, on, at each load)
    uint8_t zapper_off;
    // what the strobe's fall does (input_strobe_fall, 6502_writebyte.c; 0: nothing): PAD_STROBE_FOUR_SCORE,
    // PAD_STROBE_VS_START
    uint8_t strobe_fall;
} _pad_input_t;
#define PAD_STROBE_FOUR_SCORE 1
// (the Vs. System's Ice Climber protection, hardware type 4: the pads' bit 3 always set)
#define PAD_STROBE_VS_START 2
// (the Vaus paddle's potentiometer latched: vaus_strobe)
#define PAD_STROBE_VAUS 4
#define PAD_INPUT ((_pad_input_t*)((uint8_t*)cpu_registers + 0x70))
// ($4016 / $4017 reads take port n's fields at these strides: 6502_readbyte.c)
_Static_assert(offsetof(_6502_registers_t, PAD2_Latch) == offsetof(_6502_registers_t, PAD1_Latch) + 2, "PAD2_Latch");
_Static_assert(offsetof(_6502_registers_t, PAD2_Bit) == offsetof(_6502_registers_t, PAD1_Bit) + 2, "PAD2_Bit");
_Static_assert(offsetof(_6502_registers_t, PAD1_Bit) == offsetof(_6502_registers_t, PAD1_Latch) + 1, "PAD1_Bit");
_Static_assert(offsetof(_6502_registers_t, PAD2_Extra) == offsetof(_6502_registers_t, PAD1_Extra) + 1, "PAD2_Extra");

// the Vs. System's RC2C05 PPUs (mapper_boards.c's vs_setup; 0, the 2C02's, for every other game): $2002's low 5 bits
// (VS_PPU_STATUS_DRIVE, which a 2C02 leaves the open bus) read as the PPU's ID (VS_PPU_STATUS_ID), and $2000 / $2001
// swapped (VS_PPU_SWAP 1), in the register page
#define VS_PPU_STATUS_ID (*((uint8_t*)cpu_registers + 0x84))
#define VS_PPU_STATUS_DRIVE (*((uint8_t*)cpu_registers + 0x85))
#define VS_PPU_SWAP (*((uint8_t*)cpu_registers + 0x86))

#define VS_SYSTEM 1
// (the player's coin: its buttons still held after their pulse, and the pulse's frames left: vs_frame_input)
#define VS_COIN_HELD 0x04
#define VS_COIN_PULSE_SHIFT 3
#define VS_COIN_PULSE_MASK 0x18
// a Zapper on port 2 ($4017 bits 3-4 through PAD2_Extra: mapper_boards.c, zapper_*)
#define VS_ZAPPER 2
// an Arkanoid (Vaus) paddle: the NES one on $4017 bits 3-4 (PAD2_Extra), the Famicom one on bit 1 (d1): mapper_boards.c, vaus_*
#define VS_VAUS 0x20

#define IRQ_POLL_I_SET   1
#define IRQ_POLL_I_CLEAR 2

typedef struct {
    uint8_t PPUCTRL;
    uint8_t PPUMASK;
    uint8_t PPUSTATUS;
    uint8_t OAMADDR;
    uint8_t OAMDATA;
    uint8_t OAMDMA;
    uint8_t PPUDATA;
    uint8_t PPUOPENBUS;
    uint16_t PPU_V;
    uint16_t PPU_T;
    uint8_t PPU_X;
    uint8_t PPU_W;
    uint8_t ODDFRAME;
    // this read is a $2002 answered without its catch-up (ppu_read_sync, in the padding byte before the cycle below)
    uint8_t STATUS_SKIP;
    // the CPU cycle before which no $2002 flag can change (ppu_status_stable_update): a $2002 read before it is answered
    // without catching the devices up, while the state it was worked out from is as it was (OAM's generation, PPUMASK,
    // PPUCTRL's sprite size, OAMADDR: compared, so the register writes need no code of their own). In the register
    // page's spare bytes (the PPU's 32), so no .bss moves
    uint32_t STATUS_STABLE_UNTIL;
    uint32_t STATUS_STABLE_OAM;
    uint8_t STATUS_STABLE_MASK;
    uint8_t STATUS_STABLE_CTRL;
    uint8_t STATUS_STABLE_OAMADDR;
} _PPU_registers_t;

#define PPUCTRL_BASE_NAMETABLE_ADDRESS   0x03
#define PPUCTRL_VRAM_INCREMENT           0x04
#define PPUCTRL_SPRITE_TABLE_ADDRESS     0x08
#define PPUCTRL_BACKGROUND_TABLE_ADDRESS 0x10
#define PPUCTRL_SPRITE_SIZE              0x20
#define PPUCTRL_PPU_MASTER_SLAVE         0x40
#define PPUCTRL_VBLANK_NMI_ENABLE        0x80

#define PPUMASK_GREYSCALE                            0x01
#define PPUMASK_SHOW_BACKGROUND_IN_LEFTMOST_8_PIXELS 0x02
#define PPUMASK_SHOW_SPRITES_IN_LEFTMOST_8_PIXELS    0x04
#define PPUMASK_ENABLE_BACKGROUND_RENDERING          0x08
#define PPUMASK_ENABLE_SPRITE_RENDERING              0x10
#define PPUMASK_EMPHASIZE_RED                        0x20
#define PPUMASK_EMPHASIZE_GREEN                      0x40
#define PPUMASK_EMPHASIZE_BLUE                       0x80

#define PPUSTATUS_SPRITE_OVERFLOW_FLAG   0x20
#define PPUSTATUS_SPRITE_0_HIT_FLAG      0x40
#define PPUSTATUS_VBLANK_FLAG            0x80

#endif
