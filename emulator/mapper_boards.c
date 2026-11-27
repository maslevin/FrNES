/**
 * Mappers added on 2026-10-22: Taito TC0190 / TC0690 (33 / 48), X1-005 / X1-017 (80 / 207 / 82), Irem G-101 / H3001
 * (32 / 65), Jaleco SS88006 (18), Sunsoft-1 / -2 (89 / 93 / 184) and Namco 175 / 340 (210). mapper.c reaches them
 * through the board_* entry points (mapper.h) from its default branches.
 *
 * This file is built without LTO (see the Makefile): with it in the link time optimized unit, its size changed LTO's
 * inlining in the CPU's hot paths (cpu_exact.c, read_byte / write_byte) and MM3's Needle Man stage ran 4% slower.
 * New mappers go here (or in another file built the same way), not in mapper.c.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mapper.h"
#include "frnes.h"
#include "input.h"
#include "memory.h"
#include "cartridge.h"
#include "ppu.h"
#include "cpu.h"
#include "fds.h"
#include "input_recording.h"
#include "state.h"
#ifdef FRNES_PLAYER_BUILD
// (the Zapper's: the player's controller and the reticle, zapper_player_input / zapper_reticle_draw; in the player
// builds only, the Makefile's FRNES_PLAYER_BUILD: the test builds' code doesn't reference KOS's PVR calls from
// outside the link time optimized code, which would move KOS's code there)
#include <dc/maple.h>
#include <dc/maple/controller.h>
#include <dc/maple/mouse.h>
#include <dc/video.h>
#include "frame_hud.h"
#include "video.h"
#endif

uint8_t board_6000 = 0;
uint8_t board_timer = 0;
uint8_t board_low = 0;
uint8_t board_low_unsynced = 0, board_own_irqs = 0;
uint8_t board_saves = 0;
// board_number is 16 bits (NES 2.0's 12-bit mapper numbers as they are, past the internal ids below) in the .bss bytes
// the 8-bit one and namco175_write_enable had, so that no .bss section moves (tools/bss_order pins each by name and
// offset; fds.o's timing variables follow): its low byte the old board_number's, its high byte namco175_write_enable's
// section; that one now in the register page (+0x2ac, still saved with Namco 175's state). The value read is a copy
// in the register page (+0x2ae: one aligned load; saved and loaded with it), set with the two bytes (board_number_set)
#ifdef _arch_dreamcast
#define BOARD_BSS(name) __attribute__((section(".bss." name)))
#else
#define BOARD_BSS(name)
#endif
static uint8_t board_number_low BOARD_BSS("board_number");
static uint8_t board_number_high BOARD_BSS("namco175_write_enable");
#define board_number (*((const uint16_t*)((const uint8_t*)cpu_registers + 0x2ae)))
#define namco175_write_enable (*((uint8_t*)cpu_registers + 0x2ac))
static uint8_t board_submapper;

static void board_number_set(uint32_t id) {
    board_number_low = (uint8_t)id;
    board_number_high = (uint8_t)(id >> 8);
    *((uint16_t*)((uint8_t*)cpu_registers + 0x2ae)) = (uint16_t)id;
}
// board_number for the NSF board (NSF_BOARD, past 8 bits): 1, the MMC1's number, which mapper_init sets up itself, so
// no header or game database mapper reaches board_init with it; board_init sets board_number from a checked number
// (never a truncated one), board_override clears it for every other game
#define BOARD_NSF 1
// (and mapper 555's: board_number was 8 bits when these ids were given, and mapper 2 never comes here)
#define BOARD_EVENT2 2
// (mapper 268's, the Coolboy: mapper 3 never comes here either)
#define BOARD_COOLBOY 3
// (mappers 561 and 562's, the Game Doctors: mapper.c runs 9 and 10, the MMC2 and MMC4, itself)
#define BOARD_SGD 9
#define BOARD_TGD 10
// (mapper 319's, HP-898F: mapper.c runs 11, Color Dreams, itself)
#define BOARD_HP898F 11
// (mapper 313's, the reset based TxROM multicarts: mapper.c runs 30, UNROM 512, itself)
#define BOARD_RESET_TXROM 30
// (mapper 332's, Super 40-in-1 WS: mapper.c runs 34, BNROM / NINA-001, itself)
#define BOARD_SUPER40 34
// (mapper 260's, the HP10xx / HP20xx multicarts: mapper.c runs 76, Namco 3446, itself)
#define BOARD_HPXX 76
// (mapper 264's, Yoko's 2-in-1 boards on Cony's ASIC: mapper.c runs 85, VRC7, itself)
#define BOARD_YOKO 85
// (mapper 558's, the YC-03-09 boards: mapper.c runs 88, Namco 3433, itself)
#define BOARD_YC0309 88
// (mapper 534's, the ING-022 ASIC: mapper.c runs 95, Namcot 3425, itself)
#define BOARD_ING022 95
// (mapper 265's, BMC-T-262: mapper.c runs 154, Namcot 3453, itself)
#define BOARD_T262 154
// (mapper 551's, Jncota's 178: mapper.c runs 7, AxROM, itself)
#define BOARD_JNCOTA 7
// (mapper 396's, BMC-830752C: mapper.c runs 69, the FME-7, itself)
#define BOARD_830752 69
// (mapper 361's, J.Y.'s YY841101C: mapper.c runs 71, Camerica, itself)
#define BOARD_YY841101 71
// (mapper 314's, PB011003-3x1: mapper.c runs 64, RAMBO-1, itself)
#define BOARD_PB011003 64
// (mapper 286's, BS-5: mapper.c runs 66, GxROM, itself)
#define BOARD_BS5 66
// (mapper 374's, the reset-based MMC1 4-in-1s: mapper.c runs 22, the VRC2a, itself)
#define BOARD_MMC1_RESET 22
// (mapper 403's, the 89433 board: mapper.c runs 21, the VRC4a / c, itself)
#define BOARD_89433 21
// (mapper 434's, S-009: mapper.c runs 23, the VRC2b / VRC4e, itself)
#define BOARD_S009 23
// (mapper 283's, GS-2004 / GS-2013: mapper.c runs 24, the VRC6a, itself)
#define BOARD_GS2004 24
// (mapper 348's, BMC-830118C: mapper.c runs 25, the VRC4b / d, itself)
#define BOARD_830118C 25
// (mapper 439's, YS2309: mapper.c runs 26, the VRC6b, itself)
#define BOARD_YS2309 26
// (mapper 285's, A65AS: mapper.c runs 118, TxSROM, itself)
#define BOARD_A65AS 118
// (mapper 336's, BMC-K-3046: mapper.c runs 119, TQROM, itself)
#define BOARD_K3046 119
// (mapper 359's, BMC-SB-5013: mapper.c runs 159, Bandai's FCG with a 24C01, itself)
#define BOARD_SB5013 159
// (mapper 290's, BMC-NTD-03: mapper.c runs 206, the Namco 108, itself)
#define BOARD_NTD03 206
// (mapper 261's, BMC-810544-C-A1: mapper.c runs 153, Bandai's FCG with 8 kB of PRG RAM, itself)
#define BOARD_810544 153
// (mapper 287's, BMC-411120-C / K-3088: mapper.c runs 75, the VRC1, itself)
#define BOARD_411120C 75
// (mapper 366's, BMC-GN-45: mapper.c runs 111, GTROM, itself)
#define BOARD_GN45 111
// (mapper 263's, the KOF97 MMC3: mapper.c runs 73, the VRC3, itself)
#define BOARD_KOF97 73
// (mapper 346's, Kaiser KS-7012: mapper.c runs 67, Sunsoft-3, itself)
#define BOARD_KS7012 67
// (mapper 312's, Kaiser KS-7013B: mapper.c runs 68, Sunsoft-4, itself)
#define BOARD_KS7013B 68
// (mapper 306's, Kaiser KS-7016: 239, a number iNES leaves unassigned)
#define BOARD_KS7016 239
// board_number for a mapper number: the ids above for the mappers past 8 bits given one while it was 8 bits, the
// others' own numbers
static inline uint16_t board_id(uint32_t number) {
    return (number == NSF_BOARD) ? BOARD_NSF : (number == 555) ? BOARD_EVENT2 : (number == 268) ? BOARD_COOLBOY :
        (number == 561) ? BOARD_SGD : (number == 562) ? BOARD_TGD : (number == 319) ? BOARD_HP898F :
        (number == 313) ? BOARD_RESET_TXROM : (number == 332) ? BOARD_SUPER40 : (number == 260) ? BOARD_HPXX : (number == 264) ? BOARD_YOKO : (number == 558) ? BOARD_YC0309 : ((number == 534) || (number == 422)) ? BOARD_ING022 : (number == 265) ? BOARD_T262 : (number == 551) ? BOARD_JNCOTA : (number == 396) ? BOARD_830752 :
        (number == 361) ? BOARD_YY841101 : (number == 314) ? BOARD_PB011003 : (number == 286) ? BOARD_BS5 : (number == 374) ? BOARD_MMC1_RESET :
        (number == 403) ? BOARD_89433 : (number == 434) ? BOARD_S009 : (number == 283) ? BOARD_GS2004 : (number == 348) ? BOARD_830118C : (number == 439) ? BOARD_YS2309 : (number == 359) ? BOARD_SB5013 : (number == 290) ? BOARD_NTD03 : (number == 261) ? BOARD_810544 : (number == 263) ? BOARD_KOF97 : (number == 346) ? BOARD_KS7012 : (number == 312) ? BOARD_KS7013B :
        (number == 306) ? BOARD_KS7016 : (number == 287) ? BOARD_411120C : (number == 366) ? BOARD_GN45 : (number == 285) ? BOARD_A65AS : (number == 336) ? BOARD_K3046 :
        (number < 0x1000) ? (uint16_t)number : 0;
}
static void vs_setup(void);
static void zapper_setup(void);
static void vaus_setup(void);
static void vaus_run(void);
#ifdef FRNES_PLAYER_BUILD
static void vaus_player_input(void);
static int32_t zapper_stick(int32_t value, int32_t step);
#endif
#ifdef FRNES_PLAYER_BUILD
static void zapper_reticle_draw(void);
static void zapper_gun_report(void);
#endif
static void zapper_run(uint32_t cycles);
static uint32_t zapper_cycles_to_event(void);
static const struct { uint32_t crc; uint8_t mapper, submapper; } board_overrides[] = {
    // Taito TC0690 (headers say 33)
    { 0xa7b0536c, 48, 0 }, { 0x1500e835, 48, 1 }, { 0x6cdc0cd9, 48, 0 }, { 0x99c395f9, 48, 0 }, { 0x40c0ad47, 48, 0 },
    // Taito X1-005 with the nametables wired to the CHR banks (Fudou Myouou Den, header says 80)
    { 0x7678f1d5, 207, 0 },
    // Tengen 800004 (Namco 108 style, four-screen): Gauntlet (unlicensed), header says MMC3
    { 0xcd50a092, 206, 0 },
    // Irem G-101 on Major League's board
    { 0x243a8735, 32, 1 },
    // Namco 175 (submapper 1) and 340 (2), headers say 19
    { 0xc247cc80, 210, 1 }, { 0x0c47946d, 210, 1 }, { 0xbd523011, 210, 1 },
    { 0x1dc0f740, 210, 2 }, { 0x46fd7843, 210, 2 }, { 0x2447e03b, 210, 2 }, { 0x6ec51de5, 210, 2 },
};

#include "prg_ram_table.h"
#include "mapper_table.h"

uint8_t cartridge_single_screen = 0, cartridge_db_no_battery = 0;

// an entry's flags (tools/gen_mapper_table.py, from the NES 2.0 XML database): its mirroring when it gives one (bits
// 0-2), and its battery (bit 3) added to the header's; a verified entry (bit 4) without one only notes it, the header's
// battery kept so an existing save is never dropped
static void board_db_flags(uint8_t flags) {
    switch (flags & 7) {
        case 1: NesHeader.byInfo1 &= ~9; break;
        case 2: NesHeader.byInfo1 = (NesHeader.byInfo1 & ~8) | 1; break;
        case 3: NesHeader.byInfo1 |= 8; break;
        case 4: case 5: NesHeader.byInfo1 &= ~8; cartridge_single_screen = (flags & 7) - 3; break;
    }
    if (flags & 8) {
        NesHeader.byInfo1 |= 2;
    } else if ((flags & 16) && (NesHeader.byInfo1 & 2)) {
        printf(" - No battery in the game database (an existing save is still kept)\n");
        cartridge_db_no_battery = 1;
    }
}

uint32_t board_mirroring() {
    if (cartridge_single_screen) {
        return (cartridge_single_screen == 1) ? NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW : NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH;
    }
    return (NesHeader.byInfo1 & 1) ? NAMETABLE_MIRRORING_VERTICAL : NAMETABLE_MIRRORING_HORIZONTAL;
}

// the game database's entry for the ROM (its index), -1 if it has none; changes nothing
static int32_t board_db_index(uint32_t prg_chr_crc) {
    int low = 0, high = MAPPER_TABLE_SIZE - 1;
    while (low <= high) {
        int middle = (low + high) / 2;
        if (mapper_table_crcs[middle] == prg_chr_crc) {
            return middle;
        }
        if (mapper_table_crcs[middle] < prg_chr_crc) {
            low = middle + 1;
        } else {
            high = middle - 1;
        }
    }
    return -1;
}

// an entry's mapper and submapper (submapper << 12)
static int32_t board_db_board(int32_t index) {
    // (the Datach, 157: Bandai's LZ93D50 with its 24C02, nes2_datach_cartridge's)
    return ((mapper_table_boards[index] & 0xfff) == 157) ? (16 | (5 << 12)) : mapper_table_boards[index];
}

int32_t board_db_mapper(uint32_t prg_chr_crc) {
    int32_t index = board_db_index(prg_chr_crc);
    if (index < 0) {
        return -1;
    }
    board_db_flags(mapper_table_flags[index]);
    return board_db_board(index);
}

/*
 * load_nes_file: an iNES 1.0 ROM's PRG RAM, as Mesen 2 sizes it: its game database's size for the games in
 * prg_ram_table.h (tools/gen_prg_ram_table.py; mostly discrete logic boards, which have none), else 8 kB on every board
 * (Mesen 2 gives a ROM it doesn't know, homebrew such as AccuracyCoin included, work RAM even on NROM), except
 * mapper 30, whose battery bit means its flash
 */
uint32_t board_prg_ram_size(const uint8_t* data, uint32_t size, uint32_t mapper) {
    // by the CRC32 of the ROM after its header and trainer, as board_db_mapper's
    if ((NesHeader.byInfo1 & 4) && (size >= 512)) {
        data += 512;
        size -= 512;
    }
    uint32_t crc = input_recording_crc32(data, size);
    int low = 0, high = PRG_RAM_TABLE_SIZE - 1;
    while (low <= high) {
        int middle = (low + high) / 2;
        if (prg_ram_crcs[middle] == crc) {
            printf(" - PRG RAM %u kB (game database)\n", prg_ram_kb[middle]);
            return prg_ram_kb[middle] * 1024;
        }
        if (prg_ram_crcs[middle] < crc) {
            low = middle + 1;
        } else {
            high = middle - 1;
        }
    }
    if (mapper == 30) {
        return 0;
    }
    return 0x2000;
}

// load_nes_file: dumps whose iNES headers give less CHR ROM than the file holds (by the CRC32 of everything after the
// header, as Mesen 2's database sizes them): The Flintstones (J), 256 kB
uint32_t board_chr_rom_size(const uint8_t* data, uint32_t size, uint32_t chr_size) {
    if ((chr_size < 0x40000) && (input_recording_crc32(data, size) == 0x40c0ad47)) {
        printf(" - CHR ROM size corrected from the ROM database\n");
        return 0x40000;
    }
    return chr_size;
}

/**
 * The smallest PRG ROM window the loaded board switches, in bytes (32, 16 or 8 kB; 8 kB for boards not listed): for the
 * frontend's Game Genie, whose codes patch a CPU address and so whichever bank is mapped there. Boards without PRG
 * banking (NROM, CNROM...) count as one 32 kB window. By the header's / database's mapper number; here, out of the link
 * time optimized code (in mapper.c it reordered LTO's partitions), and nothing calls it while emulating
 */
FRNES_BOARD_TEXT uint32_t mapper_prg_window(void) {
    switch (cartridge_mapper_number) {
        case 0: case 3: case 7: case 11: case 13: case 34: case 66: case 77: case 79: case 113: case 132: case 86: case 87: case 140:
        case 144: case 184: case 185: case 241: case 42: case 108: case 283: case 133: case 216: case 171: case 143: case 149: case 46:
            return 0x8000;
        case 1: case 2: case 10: case 16: case 28: case 30: case 68: case 70: case 71: case 72: case 73: case 78: case 89:
        case 92: case 93: case 94: case 97: case 152: case 153: case 159: case 180: case 188: case 228:
        case 232: case 60: case 235: case 434: case 403: case 439: case 202: case 203: case 230: case 285: case 336: case 290: case 81: case 62: case 261: case 233: case 61:
            return 0x4000;
        default:
            return 0x2000;
    }
}

// the board another mapper number runs as (board_override's, without its state: frnes_rom_support's too), else the
// number itself
static uint32_t board_alias(uint32_t number) {
    // Tengen 800037 runs as the RAMBO-1 (mapper 64) with its own write handler (mapper.c, by cartridge_mapper_number)
    if (number == 158) {
        return 64;
    }
    // mapper 552 is the X1-017 with its PRG bits in the order the ROM is wired (x1_write_6000, by cartridge_mapper_number)
    if (number == 552) {
        return 82;
    }
    // mapper 233 (the Tsang Hai 42-in-1s...) is 226 with PRG bit 5 from a flip-flop each reset toggles (latch226_apply,
    // by cartridge_mapper_number)
    if (number == 233) {
        return 226;
    }
    // mapper 155 is the MMC1A (Tatakae!! Ramen Man, The Money Game): the MMC1, its PRG RAM never switched off by bit 4
    // of the PRG register (mapper.c's mmc1_apply_prg, by cartridge_mapper_number)
    if (number == 155) {
        return 1;
    }
    // mappers 281, 282, 295, 358, 386 and 387 are the J.Y. Company ASIC on the JY-0xx multicarts (Super HiK 4-in-1s...):
    // 209's board with their own outer banks (jy_prg_page, jy_chr_reg, by cartridge_mapper_number)
    if ((number == 281) || (number == 282) || (number == 295) || (number == 358) || (number == 386) || (number == 387)) {
        return 209;
    }
    if (number == 388) {
        return 90;
    }
    // iNES mapper 151 is the Vs. System's VRC1 (older dumps of Vs. The Goonies and Vs. Gradius; NES 2.0 has them as 75)
    if (number == 151) {
        return 75;
    }
    return number;
}

// the ROM's board from board_overrides (by the CRC32 of the ROM after its header): 1 with its mapper and submapper
static int board_crc_override(uint32_t crc, uint32_t* mapper, uint32_t* submapper) {
    for (uint32_t i = 0; i < sizeof(board_overrides) / sizeof(board_overrides[0]); i++) {
        if (board_overrides[i].crc == crc) {
            *mapper = board_overrides[i].mapper;
            *submapper = board_overrides[i].submapper;
            return 1;
        }
    }
    return 0;
}

/*
 * The mapper numbers FrNES plays (mapper_init's boards in mapper.c, board_init's here, and the numbers board_override
 * turns into one of them), sorted. mapper_init loads only these (board_supported), and frnes_rom_support answers from
 * them, so the menus and the loader can't disagree: a board added to a switch must be added here too, or it won't load
 * (tools/rom_support_check.py checks both against the ROM set)
 */
static const uint16_t board_supported_numbers[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 28, 29, 30, 32,
    33, 34, 35, 36, 37, 40, 42, 43, 44, 45, 46, 47, 48, 50, 51, 52, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67,
    68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 97,
    99, 103, 105, 106, 108, 111, 112, 113, 114, 115, 117, 118, 119, 120, 121, 123, 125, 126, 132, 133, 134, 136,
    137, 138, 139, 140, 141, 142, 143, 144, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 162, 163,
    164, 167, 171, 172, 173, 176, 177, 178, 180, 183, 184, 185, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196,
    197, 198, 200, 201, 202, 203, 205, 206, 207, 209, 210, 211, 212, 215, 216, 219, 221, 222, 224, 225, 226, 227,
    228, 230, 232, 233, 235, 236, 240, 241, 242, 245, 246, 248, 249, 250, 253, 254, 260, 261, 263, 264, 265, 267,
    268, 281, 282, 283, 285, 286, 287, 290, 295, 302, 304, 305, 306, 307, 309, 312, 313, 314, 319, 332, 336, 346,
    348, 358, 359, 361, 362, 366, 368, 374, 377, 386, 387, 388, 395, 396, 403, 415, 422, 434, 438, 439, 445, 449,
    516, 519, 521, 522, 529, 530, 534, 535, 551, 552, 555, 558, 561, 562
};

// 1 when mapper_init can load the board (submapper: the NES 2.0 header's, or board_override's)
static int board_supported_submapper(uint32_t number, uint32_t submapper) {
    if (number == NSF_BOARD) {
        return 1;
    }
    // (UNL-8237A's submapper 1, its other outer bits, not done; mapper 12's submappers past the SMC's 1)
    if (((number == 215) && (submapper != 0)) || ((number == 12) && (submapper > 1))) {
        return 0;
    }
    int32_t low = 0, high = (int32_t)(sizeof(board_supported_numbers) / sizeof(board_supported_numbers[0])) - 1;
    while (low <= high) {
        int32_t middle = (low + high) / 2;
        if (board_supported_numbers[middle] == number) {
            return 1;
        }
        if (board_supported_numbers[middle] < number) {
            low = middle + 1;
        } else {
            high = middle - 1;
        }
    }
    return 0;
}

// mapper_init's: the board board_override chose, with its submapper
__attribute__((cold, noinline)) int board_supported(uint32_t number) {
    return board_supported_submapper(number, board_submapper);
}

__attribute__((cold, noinline)) uint32_t board_override(uint32_t number) {
    board_submapper = cartridge_submapper;
    vs_setup();
    // (an NSF tune's cartridge: cartridge_convert saw the tune, and the header is the one nsf_cartridge wrote; a .nes
    // whose header says NSF_IMAGE_MAPPER isn't an NSF)
    uint32_t nsf_tune = (board_number == BOARD_NSF) && (number == NSF_IMAGE_MAPPER);
    // (no board's handlers until board_init picks one: mapper_write_select gives NROM board_write too)
    board_number_set(0);
    if (nsf_tune) {
        return NSF_BOARD;
    }
    uint32_t alias = board_alias(number);
    if (alias != number) {
        return alias;
    }
    uint32_t mapper, submapper;
    if (board_crc_override(currentCRC32, &mapper, &submapper)) {
        board_submapper = submapper;
        printf(" ...board: mapper %lu submapper %lu\n", (unsigned long)mapper, (unsigned long)board_submapper);
        return mapper;
    }
    return number;
}

// (a mask for the usual power of two page counts: '%' is a libgcc call on the SH4, see mapper.c)
static inline uint32_t page_wrap(uint32_t page, uint32_t pages) {
    return (pages & (pages - 1)) ? (page % pages) : (page & (pages - 1));
}

static uint8_t* prg_page(uint32_t page) {
    return ROM_pages[page_wrap(page, num_8k_ROM_pages)];
}

static uint8_t* chr_page(uint32_t page) {
    // CHR RAM cartridges keep their pages in VROM_pages too (see cartridge.c)
    uint32_t pages = num_1k_VROM_pages ? num_1k_VROM_pages : num_1k_VRAM_pages;
    return VROM_pages[pages ? page_wrap(page, pages) : 0];
}

static void prg_16k(uint32_t slot, uint32_t bank) {
    set_6502_bank_pointer(slot, prg_page(bank * 2));
    set_6502_bank_pointer(slot + 1, prg_page(bank * 2 + 1));
}

static void prg_32k(uint32_t bank) {
    for (uint32_t i = 0; i < 4; i++) {
        set_6502_bank_pointer(i, prg_page(bank * 4 + i));
    }
}

static void chr_4k(uint32_t half, uint32_t bank) {
    for (uint32_t i = 0; i < 4; i++) {
        set_ppu_pattern_table_bank_pointer(half * 4 + i, chr_page(bank * 4 + i));
    }
}

static void chr_8k(uint32_t bank) {
    chr_4k(0, bank * 2);
    chr_4k(1, bank * 2 + 1);
}

// a CPU cycle counter starting to count (as counter_timer_start in mapper.c)
static void counter_timer_start() {
    if (!mapper_cpu_timer) {
        mapper_timer_start(cpu_cycle_now);
    }
}

// a 1 kB page of the cartridge's CHR RAM: VRAM_pages on a cartridge with CHR ROM as well, the CHR window's own
// pages (VROM_pages, cartridge.c) on a CHR RAM only one, whose VRAM_pages are never set
static uint8_t* chr_ram_page(uint32_t page) {
    uint32_t pages = num_1k_VRAM_pages ? num_1k_VRAM_pages : 1;
    return num_1k_VROM_pages ? VRAM_pages[page % pages] : VROM_pages[page % pages];
}

static void chr_1k(uint32_t slot, uint32_t bank) {
    set_ppu_pattern_table_bank_pointer(slot, chr_page(bank));
}

static void chr_2k(uint32_t slot, uint32_t bank) {
    // (bank in 1 kB units, its low bit ignored)
    set_ppu_pattern_table_bank_pointer(slot, chr_page(bank & ~1u));
    set_ppu_pattern_table_bank_pointer(slot + 1, chr_page(bank | 1));
}

/*
 * Whether a CHR bank is already the one mapped: a write that maps it again changes nothing the PPU sees, so it needs
 * no catch-up (each one replays the scanline so far dot by dot: Death Race's 144 board, Castle of Deceit's 11)
 */
static int chr_slots_mapped(uint32_t slot, uint32_t page, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        if (ppu_pattern_table_bank_pointer(slot + i) != chr_page(page + i)) {
            return 0;
        }
    }
    return 1;
}

static int chr_1k_mapped(uint32_t slot, uint32_t bank) {
    return chr_slots_mapped(slot, bank, 1);
}

static int chr_2k_mapped(uint32_t slot, uint32_t bank) {
    return chr_slots_mapped(slot, bank & ~1u, 2);
}

static int chr_4k_mapped(uint32_t half, uint32_t bank) {
    return chr_slots_mapped(half * 4, bank * 4, 4);
}

static int chr_8k_mapped(uint32_t bank) {
    return chr_slots_mapped(0, bank * 8, 8);
}

/**
 * Taito TC0190 (mapper 33: Akira, Power Blazer) and TC0690 (mapper 48: Don Doko Don 2, Bubble Bobble 2, The
 * Jetsons, The Flintstones (J)): $8000 8 kB PRG at $8000 (TC0190: bit 6 mirroring, set for horizontal), $8001 8 kB
 * PRG at $A000, $8002 / $8003 2 kB CHR at $0000 / $0800 (in 2 kB units), $A000-$A003 1 kB CHR at $1000-$1C00; the
 * last two banks are fixed at $C000. The TC0690 adds an MMC3-like scanline counter clocked by A12 (in mapper.c,
 * tc0690_irq_*): $C000 the latch (written inverted), $C001 reload, $C002 enable, $C003 disable and acknowledge
 * ($C000 / $C001 acknowledge too), and $E000 bit 6 mirroring. Its IRQ comes up later than the MMC3's would, 22 CPU
 * cycles after the clock (6 on The Jetsons' board, submapper 1, whose latch is also one more), as Mesen 2 times it.
 */
static void taito_tc0190_write(uint16_t address, uint8_t data) {
    switch (address & ((board_number == 48) ? 0xe003 : 0xa003)) {
        case 0x8000:
            if (board_number == 33) {
                ppu_sync();
                set_ppu_nametable_mirroring((data & 0x40) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
            }
            set_6502_bank_pointer(0, prg_page(data & 0x3f));
            break;
        case 0x8001:
            set_6502_bank_pointer(1, prg_page(data & 0x3f));
            break;
        case 0x8002: case 0x8003:
            if (!chr_2k_mapped((address & 1) * 2, data * 2)) {
                ppu_sync();
                chr_2k((address & 1) * 2, data * 2);
            }
            break;
        case 0xa000: case 0xa001: case 0xa002: case 0xa003:
            if (!chr_1k_mapped(4 + (address & 3), data)) {
                ppu_sync();
                chr_1k(4 + (address & 3), data);
            }
            break;
        case 0xc000: case 0xc001: case 0xc002: case 0xc003:
            tc0690_irq_write(address & 3, data);
            break;
        case 0xe000:
            ppu_sync();
            set_ppu_nametable_mirroring((data & 0x40) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
            break;
    }
}

__attribute__((cold, noinline)) static void taito_tc0190_init() {
    if (board_number == 48) {
        tc0690_irq_init(board_submapper == 1);
    }
    set_6502_bank_pointer(0, prg_page(0));
    set_6502_bank_pointer(1, prg_page(1));
    set_6502_bank_pointer(2, prg_page(num_8k_ROM_pages - 2));
    set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
    chr_8k(0);
}

/**
 * Taito X1-005 (mapper 80: Minelvaton Saga; 207: Fudou Myouou Den) and X1-017 (mapper 82: SD Keiji Blader), registers
 * at $7EF0-$7EFF:
 *   X1-005: $7EF0 / $7EF1 2 kB CHR at $0000 / $0800 (in 1 kB units; on 207 bit 7 also picks the nametable for
 *     $2000-$27FF / $2800-$2FFF), $7EF2-$7EF5 1 kB CHR at $1000-$1C00, $7EF6 / $7EF7 bit 0 mirroring (set for
 *     vertical), $7EF8 / $7EF9 $A3 enables its 128 bytes of RAM at $7F00-$7FFF (mirrored once), $7EFA / $7EFB,
 *     $7EFC / $7EFD, $7EFE / $7EFF 8 kB PRG at $8000 / $A000 / $C000
 *   X1-017: $7EF0 / $7EF1 2 kB CHR (low bit ignored), $7EF2-$7EF5 1 kB CHR, swapped between the halves with $7EF6
 *     bit 1; $7EF6 bit 0 mirroring; $7EF7 / $7EF8 / $7EF9 $CA / $69 / $84 enable its RAM at $6000-$67FF / $6800-$6FFF
 *     / $7000-$73FF; $7EFA / $7EFB / $7EFC 8 kB PRG at $8000 / $A000 / $C000 (the value shifted right twice; on
 *     mapper 552, nesdev's X1-017 as the mask ROM is wired, its bits 0-5 reversed: up to 512 kB, run as 82)
 * The last bank is fixed at $E000. The RAM is the $6000 window's WRAM (so it's the battery save); reads it doesn't
 * enable are open bus (mapper_read_6000).
 */
static uint8_t x1_registers[16];

static void x1_017_apply_chr() {
    const uint8_t* r = x1_registers;
    uint32_t flip = (r[6] & 2) ? 4 : 0;
    chr_2k(0 ^ flip, r[0]);
    chr_2k(2 ^ flip, r[1]);
    for (uint32_t i = 0; i < 4; i++) {
        chr_1k((4 + i) ^ flip, r[2 + i]);
    }
}

static int x1_ram_enabled(uint16_t address) {
    const uint8_t* r = x1_registers;
    if (board_number != 82) {
        return (address >= 0x7f00) && (r[8] == 0xa3);
    }
    if (address < 0x6800) {
        return r[7] == 0xca;
    }
    if (address < 0x7000) {
        return r[8] == 0x69;
    }
    return (address < 0x7400) && (r[9] == 0x84);
}

static void x1_write_6000(uint16_t address, uint8_t data) {
    if ((address & 0xfff0) != 0x7ef0) {
        if (x1_ram_enabled(address)) {
            NES_WINDOW_BYTE(address) = data;
            if (board_number != 82) {
                NES_WINDOW_BYTE(address ^ 0x80) = data;
            }
        }
        return;
    }
    uint32_t reg = address & 0x0f;
    if (board_number != 82) {
        // (pairs of addresses)
        if ((reg >= 6) && (reg & 1)) {
            reg--;
        }
        // (a write that leaves the mapping as it is needs no catch-up: Taito Grand Prix's road writes $7EF3 mid line, the
        // same bank every line, ~34 lines a frame each replayed dot by dot before)
        uint8_t previous = x1_registers[reg];
        x1_registers[reg] = data;
        switch (reg) {
            case 0: case 1:
                if ((board_number != 207) && chr_slots_mapped(reg * 2, data & ~1u, 2)) {
                    break;
                }
                if ((board_number == 207) && (previous == data) && chr_slots_mapped(reg * 2, data & ~1u, 2)) {
                    break;
                }
                ppu_sync();
                chr_2k(reg * 2, data);
                if (board_number == 207) {
                    uint8_t a = x1_registers[0] >> 7, b = x1_registers[1] >> 7;
                    set_ppu_nametable_pages(a, a, b, b);
                }
                break;
            case 2: case 3: case 4: case 5:
                if (chr_slots_mapped(4 + reg - 2, data, 1)) {
                    break;
                }
                ppu_sync();
                chr_1k(4 + reg - 2, data);
                break;
            case 6:
                if (board_number == 80) {
                    ppu_sync();
                    set_ppu_nametable_mirroring((data & 1) ? NAMETABLE_MIRRORING_VERTICAL : NAMETABLE_MIRRORING_HORIZONTAL);
                }
                break;
            case 10: case 12: case 14:
                set_6502_bank_pointer((reg - 10) / 2, prg_page(data));
                break;
        }
        return;
    }
    x1_registers[reg] = data;
    switch (reg) {
        case 0: case 1: case 2: case 3: case 4: case 5:
            ppu_sync();
            x1_017_apply_chr();
            break;
        case 6:
            ppu_sync();
            set_ppu_nametable_mirroring((data & 1) ? NAMETABLE_MIRRORING_VERTICAL : NAMETABLE_MIRRORING_HORIZONTAL);
            x1_017_apply_chr();
            break;
        case 10: case 11: case 12: {
            // (mapper 552, the X1-017 as it's wired: the PRG ROM's A13-A18 from bits 5-0, reversed; 82 is the older
            // reading, A13-A16 from bits 2-5)
            uint32_t bank = data >> 2;
            if (cartridge_mapper_number == 552) {
                bank = 0;
                for (uint32_t i = 0; i < 6; i++) {
                    bank |= ((data >> i) & 1) << (5 - i);
                }
            }
            set_6502_bank_pointer(reg - 10, prg_page(bank));
        } break;
    }
}

__attribute__((cold, noinline)) static void x1_init() {
    memset(x1_registers, 0, sizeof(x1_registers));
    resize_wram(0x2000);
    set_wram_bank_pointer(WRAM);
    mapper_6000_mode = MAPPER_6000_REGISTERS;
    mapper_6000_reads = 1;
    board_6000 = 1;
    for (uint32_t i = 0; i < 3; i++) {
        set_6502_bank_pointer(i, prg_page(i));
    }
    set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
    chr_8k(0);
}

/**
 * Irem G-101 (mapper 32: Image Fight, Kaiketsu Yanchamaru 2, Major League): $8000-$8FFF 8 kB PRG at $8000 (or $C000
 * in mode 1), $9000-$9FFF bit 0 mirroring (set for horizontal) and bit 1 the PRG mode, $A000-$AFFF 8 kB PRG at $A000,
 * $B000-$B007 1 kB CHR. Mode 0 fixes the second last bank at $C000, mode 1 at $8000; the last is at $E000. Major
 * League's board (submapper 1) has one-screen mirroring and only mode 0.
 */
static uint8_t g101_prg0, g101_mode;

static void g101_apply_prg() {
    uint32_t second_last = num_8k_ROM_pages - 2;
    set_6502_bank_pointer(0, prg_page(g101_mode ? second_last : g101_prg0));
    set_6502_bank_pointer(2, prg_page(g101_mode ? g101_prg0 : second_last));
}

static void g101_write(uint16_t address, uint8_t data) {
    switch (address & 0xf000) {
        case 0x8000:
            g101_prg0 = data & 0x1f;
            g101_apply_prg();
            break;
        case 0x9000:
            g101_mode = (board_submapper == 1) ? 0 : ((data >> 1) & 1);
            g101_apply_prg();
            if (board_submapper != 1) {
                ppu_sync();
                set_ppu_nametable_mirroring((data & 1) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
            }
            break;
        case 0xa000:
            set_6502_bank_pointer(1, prg_page(data & 0x1f));
            break;
        case 0xb000:
            if (!chr_1k_mapped(address & 7, data)) {
                ppu_sync();
                chr_1k(address & 7, data);
            }
            break;
    }
}

__attribute__((cold, noinline)) static void g101_init() {
    g101_prg0 = g101_mode = 0;
    g101_apply_prg();
    set_6502_bank_pointer(1, prg_page(1));
    set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
    chr_8k(0);
    if (board_submapper == 1) {
        set_ppu_nametable_mirroring(NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW);
    }
}

/**
 * Irem H3001 (mapper 65: Kaiketsu Yanchamaru 3, Daiku no Gen San 2): $8000 / $A000 / $C000 8 kB PRG at $8000 /
 * $A000 / $C000 (the last bank at $E000), $9001 bit 7 mirroring (set for horizontal), $B000-$B007 1 kB CHR. The IRQ
 * counter counts CPU cycles down from its reload value ($9005 high, $9006 low byte; $9004 reloads it), and reaching 0
 * raises the IRQ and stops it; $9003 bit 7 starts / stops it. $9003 and $9004 acknowledge.
 */
static uint16_t h3001_counter, h3001_reload;
static uint8_t h3001_enabled;

static void h3001_write(uint16_t address, uint8_t data) {
    switch (address) {
        case 0x8000: set_6502_bank_pointer(0, prg_page(data)); break;
        case 0xa000: set_6502_bank_pointer(1, prg_page(data)); break;
        case 0xc000: set_6502_bank_pointer(2, prg_page(data)); break;
        case 0x9001:
            ppu_sync();
            set_ppu_nametable_mirroring((data & 0x80) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
            break;
        case 0x9003: case 0x9004:
            if (mapper_cpu_timer) {
                mapper_cpu_run(cpu_cycle_now);
            }
            counter_timer_start();
            if (address == 0x9003) {
                h3001_enabled = data >> 7;
            } else {
                h3001_counter = h3001_reload;
            }
            cpu_irq_clear(CPU_IRQ_MAPPER);
            mapper_cpu_timer = h3001_enabled;
            cpu_cut_batch_for_event(mapper_cycles_to_event());
            break;
        case 0x9005: h3001_reload = (h3001_reload & 0x00ff) | (data << 8); break;
        case 0x9006: h3001_reload = (h3001_reload & 0xff00) | data; break;
        default:
            if ((address & 0xfff8) == 0xb000) {
                if (!chr_1k_mapped(address & 7, data)) {
                    ppu_sync();
                    chr_1k(address & 7, data);
                }
            }
            break;
    }
}

static void h3001_cpu_run(uint32_t cycles) {
    uint32_t to_zero = h3001_counter ? h3001_counter : 0x10000;
    if (cycles >= to_zero) {
        h3001_counter = 0;
        h3001_enabled = 0;
        mapper_cpu_timer = 0;
        mapper_irq_flag |= CPU_IRQ_MAPPER;
        cpu_irq_update_line();
    } else {
        h3001_counter -= cycles;
    }
}

__attribute__((cold, noinline)) static void h3001_init() {
    h3001_counter = h3001_reload = 0;
    h3001_enabled = 0;
    set_6502_bank_pointer(0, prg_page(0));
    set_6502_bank_pointer(1, prg_page(1));
    set_6502_bank_pointer(2, prg_page(num_8k_ROM_pages - 2));
    set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
    chr_8k(0);
}

/**
 * Jaleco SS88006 (mapper 18: Jajamaru Gekimaden, Ninja Jajamaru, Pizza Pop): registers take a nibble each, the even
 * address the low one and the odd the high one (decoded with $F003): $8000-$8003 / $9000-$9001 8 kB PRG at $8000 / $A000 / $C000 (the last
 * bank at $E000), $A000-$D003 the eight 1 kB CHR banks, $E000-$E003 the IRQ reload value (lowest nibble first). $F000
 * reloads the counter, $F001 bit 0 starts it and bits 1-3 make it 12, 8 or 4 bits wide (the highest set bit wins),
 * both acknowledge. $F002 mirroring (horizontal, vertical, one screen low / high). The counter counts CPU cycles down
 * in its width, and reaching 0 raises the IRQ (it keeps counting, wrapping around). $F003's ADPCM sound and $9002's
 * PRG RAM protection aren't emulated.
 */
static uint8_t ss88006_prg[3], ss88006_chr[8], ss88006_reload[4], ss88006_enabled;
static uint16_t ss88006_counter, ss88006_mask;

static void ss88006_write(uint16_t address, uint8_t data) {
    uint32_t high = address & 1;
    uint32_t pair = (address & 2) >> 1;
    data &= 0x0f;
    switch (address & 0xf000) {
        case 0x8000: case 0x9000: {
            uint32_t bank = ((address >> 12) - 8) * 2 + pair;
            if (bank > 2) {
                break;
            }
            ss88006_prg[bank] = high ? ((ss88006_prg[bank] & 0x0f) | (data << 4)) : ((ss88006_prg[bank] & 0xf0) | data);
            set_6502_bank_pointer(bank, prg_page(ss88006_prg[bank]));
        } break;
        case 0xa000: case 0xb000: case 0xc000: case 0xd000: {
            uint32_t bank = ((address >> 12) - 0xa) * 2 + pair;
            ss88006_chr[bank] = high ? ((ss88006_chr[bank] & 0x0f) | (data << 4)) : ((ss88006_chr[bank] & 0xf0) | data);
            if (!chr_1k_mapped(bank, ss88006_chr[bank])) {
                ppu_sync();
                chr_1k(bank, ss88006_chr[bank]);
            }
        } break;
        case 0xe000:
            ss88006_reload[address & 3] = data;
            break;
        case 0xf000:
            switch (address & 3) {
                case 0: case 1:
                    if (mapper_cpu_timer) {
                        mapper_cpu_run(cpu_cycle_now);
                    }
                    counter_timer_start();
                    if (address & 1) {
                        ss88006_enabled = data & 1;
                        ss88006_mask = (data & 8) ? 0x000f : ((data & 4) ? 0x00ff : ((data & 2) ? 0x0fff : 0xffff));
                    } else {
                        const uint8_t* r = ss88006_reload;
                        ss88006_counter = r[0] | (r[1] << 4) | (r[2] << 8) | (r[3] << 12);
                    }
                    cpu_irq_clear(CPU_IRQ_MAPPER);
                    mapper_cpu_timer = ss88006_enabled;
                    cpu_cut_batch_for_event(mapper_cycles_to_event());
                    break;
                case 2: {
                    static const uint8_t modes[4] = { NAMETABLE_MIRRORING_HORIZONTAL, NAMETABLE_MIRRORING_VERTICAL,
                                                      NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW, NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH };
                    ppu_sync();
                    set_ppu_nametable_mirroring(modes[data & 3]);
                } break;
            }
            break;
    }
}

// CPU cycles until the counter's part in the width next reaches 0
static uint32_t ss88006_cycles_to_zero() {
    uint32_t value = ss88006_counter & ss88006_mask;
    return value ? value : (uint32_t)ss88006_mask + 1;
}

static void ss88006_cpu_run(uint32_t cycles) {
    if (cycles >= ss88006_cycles_to_zero()) {
        mapper_irq_flag |= CPU_IRQ_MAPPER;
        cpu_irq_update_line();
    }
    uint16_t value = (uint16_t)((ss88006_counter & ss88006_mask) - cycles) & ss88006_mask;
    ss88006_counter = (ss88006_counter & ~ss88006_mask) | value;
}

__attribute__((cold, noinline)) static void ss88006_init() {
    memset(ss88006_prg, 0, sizeof(ss88006_prg));
    memset(ss88006_chr, 0, sizeof(ss88006_chr));
    memset(ss88006_reload, 0, sizeof(ss88006_reload));
    ss88006_counter = 0;
    ss88006_mask = 0xffff;
    ss88006_enabled = 0;
    for (uint32_t i = 0; i < 3; i++) {
        set_6502_bank_pointer(i, prg_page(0));
    }
    set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
    chr_8k(0);
}

/**
 * Sunsoft:
 *   89 (Sunsoft-2 on the Sunsoft-3 board: Mito Koumon): writes to $8000-$FFFF, bits 4-6 the 16 kB PRG bank at $8000
 *      (the last is fixed at $C000), bits 0-2 and 7 the 8 kB CHR bank, bit 3 the one-screen nametable
 *   93 (Sunsoft-2 / -1: Fantasy Zone, Shanghai): bits 4-6 the 16 kB PRG bank at $8000; bit 0 enables the CHR RAM,
 *      which isn't emulated (it stays enabled)
 *   184 (Sunsoft-1: The Wing of Madoola, Atlantis no Nazo): writes to $6000-$7FFF, bits 0-2 the 4 kB CHR bank at
 *      $0000, bits 4-6 the one at $1000, whose top bit is always set; 32 kB PRG
 */
static void sunsoft_simple_write(uint16_t address, uint8_t data) {
    (void)address;
    prg_16k(0, (data >> 4) & 7);
    if (board_number == 89) {
        ppu_sync();
        chr_8k((data & 7) | ((data & 0x80) >> 4));
        set_ppu_nametable_mirroring((data & 8) ? NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH : NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW);
    }
}

/**
 * Jaleco JF-13 (mapper 86: Moero!! Pro Yakyuu, Moero!! Pro Yakyuu Toushou Taikai): writes to $6000-$6FFF select
 * bits 4-5 the 32 kB PRG bank, bits 0-1 and 6 the 8 kB CHR bank. $7000-$7FFF drives the uPD7756 speech chip ("strike",
 * "ball"...), whose samples aren't in the ROM dump: not emulated. No PRG RAM.
 */
static void jf13_write(uint16_t address, uint8_t data) {
    if ((address & 0x7000) == 0x6000) {
        prg_32k((data >> 4) & 3);
        if (!chr_8k_mapped((data & 3) | ((data >> 4) & 4))) {
            ppu_sync();
            chr_8k((data & 3) | ((data >> 4) & 4));
        }
    }
}

__attribute__((cold, noinline)) static void jf13_init() {
    mapper_6000_mode = MAPPER_6000_REGISTERS;
    board_6000 = 1;
    prg_32k(0);
    chr_8k(0);
}

static void sunsoft184_write(uint8_t data) {
    if (!chr_4k_mapped(0, data & 7) || !chr_4k_mapped(1, ((data >> 4) & 7) | 4)) {
        ppu_sync();
        chr_4k(0, data & 7);
        chr_4k(1, ((data >> 4) & 7) | 4);
    }
}

__attribute__((cold, noinline)) static void sunsoft_simple_init() {
    if (board_number == 184) {
        mapper_6000_mode = MAPPER_6000_REGISTERS;
        board_6000 = 1;
        prg_32k(0);
        chr_8k(0);
        return;
    }
    prg_16k(0, 0);
    prg_16k(2, num_8k_ROM_pages / 2 - 1);
    chr_8k(0);
}

/**
 * CPROM (mapper 13: Videomation): 32 kB PRG, 16 kB CHR RAM, $0000-$0FFF its first 4 kB, $1000-$1FFF the 4 kB bank
 * that writes to $8000-$FFFF select (bits 0-1, with bus conflicts)
 */
static void cprom_write(uint16_t address, uint8_t data) {
    data &= prg_page((address >> 13) & 3)[address & 0x1fff];
    if (!chr_4k_mapped(1, data & 3)) {
        ppu_sync();
        chr_4k(1, data & 3);
    }
}

__attribute__((cold, noinline)) static void cprom_init() {
    prg_32k(0);
    chr_4k(0, 0);
    chr_4k(1, 0);
}

/**
 * UNROM 7408 (mapper 180: Crazy Climber): the first 16 kB PRG bank fixed at $8000, writes select the one at $C000
 */
static void unrom180_write(uint8_t data) {
    prg_16k(2, data & 7);
}

__attribute__((cold, noinline)) static void unrom180_init() {
    prg_16k(0, 0);
    prg_16k(2, 0);
    chr_8k(0);
}

/**
 * Jaleco JF-16 / Irem Holy Diver (mapper 78, with bus conflicts): bits 0-2 the 16 kB PRG bank at $8000 (the last is
 * fixed at $C000), bits 4-7 the 8 kB CHR bank, bit 3 mirroring: vertical / horizontal on Holy Diver (submapper 3),
 * one screen otherwise (Cosmo Carrier)
 */
static void jf16_write(uint16_t address, uint8_t data) {
    data &= NES_WINDOW_BYTE(address);
    prg_16k(0, data & 7);
    // (a PRG bank switch that leaves the CHR bank and the nametables as they are needs no PPU catch-up: Holy Diver
    // switches banks mid frame, each sync replaying its scanline so far dot by dot)
    uint32_t chr = (data >> 4) & 0x0f, high = (data >> 3) & 1;
    uint32_t mirroring = (board_submapper == 3) ? (high ? NAMETABLE_MIRRORING_VERTICAL : NAMETABLE_MIRRORING_HORIZONTAL)
                                                : (high ? NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH : NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW);
    // (the pages that mirroring maps: vertical 0 1 0 1, horizontal 0 0 1 1, one screen all the same)
    uint32_t vh = (board_submapper == 3);
    if (chr_8k_mapped(chr) && (ppu_nametable_slot_page(0) == (vh ? 0 : high)) && (ppu_nametable_slot_page(1) == high) &&
        (ppu_nametable_slot_page(2) == (vh ? !high : high)) && (ppu_nametable_slot_page(3) == (vh ? 1 : high))) {
        return;
    }
    ppu_sync();
    chr_8k(chr);
    set_ppu_nametable_mirroring(mirroring);
}

__attribute__((cold, noinline)) static void jf16_init() {
    prg_16k(0, 0);
    prg_16k(2, num_8k_ROM_pages / 2 - 1);
    chr_8k(0);
}

/**
 * Bandai's discrete boards (with bus conflicts; nesdev's INES Mapper 070 / 152), the last 16 kB of PRG fixed at $C000:
 *   70 (Kamen Rider Club, Family Trainer: Manhattan Police, Meiro Daisakusen, Space Shadow): bits 4-7 the 16 kB PRG
 *      bank at $8000, bits 0-3 the 8 kB CHR bank
 *   152 (Arkanoid II (J), Gegege no Kitarou 2): bits 4-6 the PRG bank, bits 0-3 the CHR bank, bit 7 the one-screen
 *      nametable. Older dumps have it as mapper 70 with the header's four screen bit set (fwNES's convention), taken
 *      as 152 here
 */
static void bandai70_write(uint16_t address, uint8_t data) {
    data &= NES_WINDOW_BYTE(address);
    uint32_t one_screen = (board_number == 152) || (NesHeader.byInfo1 & 8);
    prg_16k(0, (data >> 4) & (one_screen ? 7 : 0x0f));
    if (!chr_8k_mapped(data & 0x0f)) {
        ppu_sync();
        chr_8k(data & 0x0f);
    }
    if (one_screen) {
        ppu_sync();
        set_ppu_nametable_mirroring((data & 0x80) ? NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH : NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW);
    }
}

__attribute__((cold, noinline)) static void bandai70_init() {
    prg_16k(0, 0);
    prg_16k(2, num_8k_ROM_pages / 2 - 1);
    chr_8k(0);
}

/**
 * Jaleco JF-17 / JF-19 (mappers 72 / 92, with bus conflicts; nesdev's INES Mapper 072 / 092): a write to $8000-$FFFF
 * loads bits 0-3 (92: 0-4) as the 16 kB PRG bank when bit 7 goes from 0 to 1, and bits 0-3 as the 8 kB CHR bank when
 * bit 6 does. 72 (Pinball Quest (J), Moero!! Juudou Warriors, Moero!! Pro Tennis): the bank at $8000, the last fixed at
 * $C000; 92 (Moero!! Pro Soccer, Moero!! Pro Yakyuu '88 Ketteiban): the first fixed at $8000, the bank at $C000.
 * Their uPD7756 speech chip ($7000-$7FFF on some, its samples not in the dumps) isn't emulated. The last write, for the
 * edges, in the register page (+0x81: 0 at each load, a state's with it; no Vs. board uses it)
 */
#define jf17_latch (*((uint8_t*)cpu_registers + 0x81))

static void jf17_write(uint16_t address, uint8_t data) {
    data &= NES_WINDOW_BYTE(address);
    uint8_t rising = data & ~jf17_latch;
    jf17_latch = data;
    if (rising & 0x80) {
        if (board_number == 92) {
            prg_16k(2, data & 0x1f);
        } else {
            prg_16k(0, data & 0x0f);
        }
    }
    if ((rising & 0x40) && !chr_8k_mapped(data & 0x0f)) {
        ppu_sync();
        chr_8k(data & 0x0f);
    }
}

__attribute__((cold, noinline)) static void jf17_init() {
    jf17_latch = 0;
    prg_16k(0, 0);
    prg_16k(2, num_8k_ROM_pages / 2 - 1);
    chr_8k(0);
}

/**
 * Jaleco JF-11 / JF-14 (mapper 140: Bio Senshi Dan, Mississippi Satsujin Jiken, Youkai Club...): GxROM with its
 * register at $6000-$7FFF (board_write_6000; nesdev's INES Mapper 140): bits 4-5 the 32 kB PRG bank, bits 0-3 the 8 kB CHR bank. No
 * PRG RAM
 */
static void jf11_write(uint8_t data) {
    prg_32k((data >> 4) & 3);
    if (!chr_8k_mapped(data & 0x0f)) {
        ppu_sync();
        chr_8k(data & 0x0f);
    }
}

/**
 * Bandai's Karaoke Studio (mapper 188, bus conflicts; nesdev's INES Mapper 188): the base cartridge's ROM first in the
 * file, an expansion cassette's (Top Hit 20 Vol. 1 / 2) after it. A write to $8000-$FFFF selects the 16 kB bank at
 * $8000: bits 0-2 of the base ROM's with bit 4 set, of the cassette's with it clear (the base ROM's last without one);
 * bit 5 the mirroring (set for horizontal). $C000 is the base ROM's last bank. $6000-$7FFF reads the microphone's A / B
 * buttons (bits 0-1, active low: player 1's A / B) and its 1-bit sound level (bit 2: silence); 8 kB of CHR RAM
 */
static uint32_t karaoke_base_banks(void) {
    // (128 kB of base ROM, the rest the cassette's)
    return (num_8k_ROM_pages > 16) ? 8 : num_8k_ROM_pages / 2;
}

static void karaoke188_write(uint16_t address, uint8_t data) {
    data &= NES_WINDOW_BYTE(address);
    uint32_t base = karaoke_base_banks();
    uint32_t cassette = num_8k_ROM_pages / 2 - base;
    uint32_t bank = (data & 0x10) ? (data & 7) : (cassette ? (base + (data & 7) % cassette) : base - 1);
    prg_16k(0, bank);
    ppu_sync();
    set_ppu_nametable_mirroring((data & 0x20) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
}

static uint8_t karaoke188_read_6000(uint8_t open_bus) {
    uint8_t pad = cpu_registers -> PAD1_Latch;
    return (open_bus & 0xf8) | ((pad & 1) ? 0 : 1) | ((pad & 2) ? 0 : 2);
}

__attribute__((cold, noinline)) static void karaoke188_init() {
    mapper_6000_mode = MAPPER_6000_REGISTERS;
    mapper_6000_reads = 1;
    board_6000 = 1;
    prg_16k(0, 0);
    prg_16k(2, karaoke_base_banks() - 1);
    chr_8k(0);
}

/**
 * Nintendo World Championships 1990 (mapper 105; nesdev's INES Mapper 105): an MMC1 (its 5-bit serial port: a write
 * with bit 7 set resets it, the fifth loads the register $8000 / $A000 / $C000 / $E000 selects) on a board with two
 * 128 kB PRG chips, 8 kB of CHR RAM (unbanked) and a timer. Until $A000 bit 4 has been written 0 then 1 the first
 * chip's first 32 kB is at $8000; then $A000 bit 3 picks the chip: the first switched in 32 kB from $A000 bits 1-2,
 * the second MMC1 style in 16 kB from $E000 (the control register's modes). The control register's bits 0-1 the
 * mirroring. The timer, a 30-bit count of CPU cycles, runs while $A000 bit 4 is clear (set: cleared, its IRQ
 * acknowledged) and raises the IRQ at $20000000 | DIP switches << 25: the tournament's setting here, switch 3 closed
 * ($28000000, 6:15). Its state in the register page (+0xa0-+0xaf, zeroed at each load and saved with a state)
 */
typedef struct {
    uint8_t shift, count, control, a000, e000, unlock;
    // the mirroring last set, + 1 (0: none yet, as after a load); a byte
    uint8_t mirroring, pad;
    uint32_t counter;
    // the CPU cycle of the last write to $8000-$FFFF (the MMC1 ignores one on the cycle after another: a read modify
    // write instruction's two writes, as mmc1_write)
    uint32_t last_write_cycle;
} nwc_state_t;
#define NWC ((nwc_state_t*)((uint8_t*)cpu_registers + 0xa0))
#define NWC_TIMER_TARGET (0x20000000u | (4u << 25))

static void nwc_apply(void) {
    nwc_state_t* n = NWC;
    static const uint8_t mirrorings[4] FRNES_COLD_RODATA = { NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW,
        NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH, NAMETABLE_MIRRORING_VERTICAL, NAMETABLE_MIRRORING_HORIZONTAL };
    if (n -> unlock < 2) {
        prg_32k(0);
    } else if (!(n -> a000 & 0x08)) {
        prg_32k((n -> a000 >> 1) & 3);
    } else {
        uint32_t bank = 8 + (n -> e000 & 7);
        switch ((n -> control >> 2) & 3) {
            case 0: case 1:
                prg_32k(4 + ((n -> e000 >> 1) & 3));
                break;
            case 2:
                prg_16k(0, 8);
                prg_16k(2, bank);
                break;
            case 3:
                prg_16k(0, bank);
                prg_16k(2, 15);
                break;
        }
    }
    // (only when the mirroring changes: Rad Racer's PRG bank writes mid scanline, ten on one line a frame, each a
    // replay of the line dot by dot otherwise. The PRG banks change nothing the PPU sees, and the CHR RAM isn't banked)
    uint8_t mirroring = mirrorings[n -> control & 3];
    if (n -> mirroring != mirroring + 1) {
        ppu_sync();
        set_ppu_nametable_mirroring(mirroring);
        n -> mirroring = mirroring + 1;
    }
}

static void nwc_write(uint16_t address, uint8_t data) {
    nwc_state_t* n = NWC;
    uint32_t consecutive = (cpu_cycle_now == n -> last_write_cycle + 1);
    n -> last_write_cycle = cpu_cycle_now;
    if (consecutive) {
        return;
    }
    if (data & 0x80) {
        n -> shift = n -> count = 0;
        n -> control |= 0x0c;
        nwc_apply();
        return;
    }
    n -> shift |= (data & 1) << n -> count;
    if (++n -> count < 5) {
        return;
    }
    uint8_t value = n -> shift;
    n -> shift = n -> count = 0;
    switch (address & 0xe000) {
        case 0x8000:
            n -> control = value;
            break;
        case 0xa000:
            n -> a000 = value;
            if ((n -> unlock == 0) && !(value & 0x10)) {
                n -> unlock = 1;
            } else if ((n -> unlock == 1) && (value & 0x10)) {
                n -> unlock = 2;
            }
            if (mapper_cpu_timer) {
                mapper_cpu_run(cpu_cycle_now);
            }
            counter_timer_start();
            if (value & 0x10) {
                n -> counter = 0;
                cpu_irq_clear(CPU_IRQ_MAPPER);
            }
            mapper_cpu_timer = !(value & 0x10) && (n -> counter < NWC_TIMER_TARGET);
            cpu_cut_batch_for_event(mapper_cycles_to_event());
            break;
        case 0xc000:
            break;
        case 0xe000:
            n -> e000 = value;
            break;
    }
    nwc_apply();
}

static void nwc_cpu_run(uint32_t cycles) {
    nwc_state_t* n = NWC;
    if (cycles >= NWC_TIMER_TARGET - n -> counter) {
        n -> counter = NWC_TIMER_TARGET;
        mapper_cpu_timer = 0;
        mapper_irq_flag |= CPU_IRQ_MAPPER;
        cpu_irq_update_line();
    } else {
        n -> counter += cycles;
    }
}

__attribute__((cold, noinline)) static void nwc_init() {
    memset(NWC, 0, sizeof(nwc_state_t));
    NWC -> control = 0x0c;
    nwc_apply();
    chr_8k(0);
}

/**
 * FK23C / FK23CA (mapper 176: the Coolboy / Waixing multicarts and single-game boards; Mesen 2's Fk23C): an MMC3 (its
 * IRQ counter and registers through mmc3_board_write) whose banks fk23_apply maps itself after every write, from the
 * MMC3's registers as written (with the extended mode's four more: $8001 with selects 8-11, PRG at $C000 / $E000 and
 * two more 1 kB CHR banks) and four outer registers, at $5xx0-$5xx3 when the address has bits 4 and 12 set (board_low;
 * while $A001's bit 6 enables them or bit 5 doesn't map WRAM there):
 *   $5xx0: bits 0-2 the PRG mode (0-2: MMC3 banks in a 512 / 256 / 128 kB block, 3: 16 kB, 4: 32 kB), bit 4 the CHR
 *     block 128 kB (else 256 kB), bit 5 CHR RAM for the pattern tables, bit 6 the CNROM CHR mode (8 kB banks), bits 3 and
 *     7 PRG base bits 7 and 8
 *   $5xx1: PRG base bits 0-6 (16 kB units); $5xx2: the CHR base (8 kB units) and bit 6 PRG base bit 9; $5xx3: bit 1 the
 *     extended MMC3 mode, bits 2 / 6 CNROM CHR banking from $8000-$9FFF / $C000-$FFFF writes (bits 0-1)
 * $A001 bit 3 allows one-screen mirroring ($A000 bits 0-1); bit 5 the WRAM banking: bits 0-1 the 8 kB bank at
 * $6000-$7FFF, the next one at $4000-$5FFF (where the outer registers' writes go while bit 6 doesn't enable them), and
 * with bit 2 CHR banks 0-7 on the CHR RAM. State in the register page (+0xd0-+0xe7, saved with a state; the banks
 * again on a load)
 */
static void mmc3_board_write(uint16_t address, uint8_t data);

typedef struct {
    uint8_t prg_mode, chr_128k, chr_ram, cnrom_mode, ext_mode, cnrom_chr, cnrom_reg, current;
    uint8_t mmc3_chr_mode, mirroring, a001, pad;
    uint16_t prg_base;
    uint8_t chr_base, pad2;
    uint8_t registers[12];
} fk23_state_t;
#define FK23 ((fk23_state_t*)((uint8_t*)cpu_registers + 0xd0))

static void fk23_prg(uint32_t slot, uint32_t page) {
    set_6502_bank_pointer(slot, prg_page(page));
}

static void fk23_chr(uint32_t slot, uint32_t page) {
    const fk23_state_t* f = FK23;
    uint32_t ram = !num_1k_VROM_pages || (f -> chr_ram && num_1k_VRAM_pages) || (((f -> a001 & 0x24) == 0x24) && (page <= 7));
    if (ram && num_1k_VRAM_pages) {
        mapper_chr_ram_slots |= 1 << slot;
        set_ppu_pattern_table_bank_pointer(slot, chr_ram_page(page));
    } else {
        mapper_chr_ram_slots &= ~(1 << slot);
        set_ppu_pattern_table_bank_pointer(slot, chr_page(page));
    }
}

static void fk23_apply(void) {
    const fk23_state_t* f = FK23;
    const uint8_t* r = f -> registers;
    uint32_t swap = (f -> current & 0x40) ? 2 : 0;
    switch (f -> prg_mode) {
        case 0: case 1: case 2:
            if (f -> ext_mode) {
                uint32_t outer = f -> prg_base << 1;
                fk23_prg(0 ^ swap, r[6] | outer);
                fk23_prg(1, r[7] | outer);
                fk23_prg(2 ^ swap, r[8] | outer);
                fk23_prg(3, r[9] | outer);
            } else {
                uint32_t inner = 0x3f >> f -> prg_mode, outer = (f -> prg_base << 1) & ~inner;
                fk23_prg(0 ^ swap, (r[6] & inner) | outer);
                fk23_prg(1, (r[7] & inner) | outer);
                fk23_prg(2 ^ swap, (0xfe & inner) | outer);
                fk23_prg(3, (0xff & inner) | outer);
            }
            break;
        case 3:
            for (uint32_t i = 0; i < 4; i++) {
                fk23_prg(i, (f -> prg_base << 1) + (i & 1));
            }
            break;
        case 4:
            for (uint32_t i = 0; i < 4; i++) {
                fk23_prg(i, ((f -> prg_base & 0xffe) << 1) + i);
            }
            break;
    }
    if (!f -> mmc3_chr_mode) {
        uint32_t inner = f -> cnrom_mode ? (f -> chr_128k ? 1 : 3) : 0;
        for (uint32_t i = 0; i < 8; i++) {
            fk23_chr(i, (((f -> cnrom_reg & inner) | f -> chr_base) << 3) + i);
        }
    } else {
        uint32_t flip = (f -> current & 0x80) ? 4 : 0;
        if (f -> ext_mode) {
            uint32_t outer = f -> chr_base << 3;
            static const uint8_t ext_order[8] FRNES_COLD_RODATA = { 0, 10, 1, 11, 2, 3, 4, 5 };
            for (uint32_t i = 0; i < 8; i++) {
                fk23_chr(i ^ flip, r[ext_order[i]] | outer);
            }
        } else {
            uint32_t inner = f -> chr_128k ? 0x7f : 0xff, outer = (f -> chr_base << 3) & ~inner;
            uint8_t values[8] = { r[0] & 0xfe, r[0] | 1, r[1] & 0xfe, r[1] | 1, r[2], r[3], r[4], r[5] };
            for (uint32_t i = 0; i < 8; i++) {
                fk23_chr(i ^ flip, (values[i] & inner) | outer);
            }
        }
    }
    if (WRAM != NULL) {
        uint32_t banks = WRAM_size / 0x2000;
        set_wram_bank_pointer(WRAM + ((f -> a001 & 0x20) ? ((f -> a001 & 3) % banks) : 0) * 0x2000);
    }
    uint32_t m = f -> mirroring & ((f -> a001 & 0x08) ? 3 : 1);
    static const uint8_t mirrorings[4] FRNES_COLD_RODATA = { NAMETABLE_MIRRORING_VERTICAL, NAMETABLE_MIRRORING_HORIZONTAL,
        NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW, NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH };
    set_ppu_nametable_mirroring(mirrorings[m]);
}

// (the WRAM bank after $6000's at $4000-$5FFF, with $A001's bit 5)
static uint8_t* fk23_low_ram(uint16_t address) {
    const fk23_state_t* f = FK23;
    if (!(f -> a001 & 0x20) || (WRAM == NULL) || (address < 0x4020)) {
        return NULL;
    }
    uint32_t banks = WRAM_size / 0x2000;
    return WRAM + (((f -> a001 & 3) + 1) % banks) * 0x2000 + (address & 0x1fff);
}

static uint8_t fk23_read_low(uint16_t address, uint8_t open_bus) {
    uint8_t* ram = fk23_low_ram(address);
    return ram ? *ram : open_bus;
}

static void fk23_write_low(uint16_t address, uint8_t data) {
    fk23_state_t* f = FK23;
    if (!(f -> a001 & 0x40) && (f -> a001 & 0x20)) {
        uint8_t* ram = fk23_low_ram(address);
        if (ram) {
            *ram = data;
        }
        return;
    }
    if ((address & 0x5010) != 0x5010) {
        return;
    }
    switch (address & 3) {
        case 0:
            f -> prg_mode = data & 7;
            f -> chr_128k = (data >> 4) & 1;
            f -> chr_ram = (data >> 5) & 1;
            f -> mmc3_chr_mode = !(data & 0x40);
            f -> prg_base = (f -> prg_base & ~0x180) | ((data & 0x80) << 1) | ((data & 0x08) << 4);
            break;
        case 1:
            f -> prg_base = (f -> prg_base & ~0x7f) | (data & 0x7f);
            break;
        case 2:
            f -> prg_base = (f -> prg_base & ~0x200) | ((data & 0x40) << 3);
            f -> chr_base = data;
            f -> cnrom_reg = 0;
            break;
        case 3:
            f -> ext_mode = (data >> 1) & 1;
            f -> cnrom_mode = (data & 0x44) != 0;
            break;
    }
    ppu_sync();
    fk23_apply();
}

static void fk23_write(uint16_t address, uint8_t data) {
    fk23_state_t* f = FK23;
    // (the MMC3 its registers and IRQ counter, with the low bits of the selects; its banks are fk23_apply's)
    mmc3_board_write(address, data);
    if (f -> cnrom_mode && ((address <= 0x9fff) || (address >= 0xc000))) {
        f -> cnrom_reg = data & 3;
    }
    switch (address & 0xe001) {
        case 0x8000:
            f -> current = data;
            break;
        case 0x8001: {
            uint32_t reg = f -> current & (f -> ext_mode ? 0x0f : 0x07);
            if (reg < 12) {
                f -> registers[reg] = data;
            }
        } break;
        case 0xa000:
            f -> mirroring = data & 3;
            break;
        case 0xa001:
            f -> a001 = (data & 0x20) ? data : (data & 0xc0);
            break;
    }
    ppu_sync();
    fk23_apply();
}

__attribute__((cold, noinline)) static void fk23_init() {
    fk23_state_t* f = FK23;
    memset(f, 0, sizeof(fk23_state_t));
    f -> mmc3_chr_mode = 1;
    // (1 MB of PRG and of CHR ROM: it starts in the second 512 kB, as Mesen 2's)
    f -> prg_base = ((num_8k_ROM_pages == 128) && (num_1k_VROM_pages == 1024)) ? 0x20 : 0;
    static const uint8_t initial[12] FRNES_COLD_RODATA = { 0, 2, 4, 5, 6, 7, 0, 1, 0xfe, 0xff, 0xff, 0xff };
    memcpy(f -> registers, initial, sizeof(initial));
    fk23_apply();
}

/**
 * Address latch multicarts and Waixing's 178 (Mesen 2's Mapper58 / UnlD1038 / Mapper241 / Waixing178 as references):
 *   58 (GameStar / Study & Game multicarts): a write to $8000-$FFFF selects by its address: bits 0-2 the 16 kB PRG bank
 *      (bit 6 set: at $8000 and $C000, clear: the 32 kB bank of bits 1-2), bits 3-5 the 8 kB CHR bank, bit 7 the
 *      mirroring (set for horizontal)
 *   59 (UNL-D1038 multicarts): address bits 4-6 the 16 kB PRG bank (bit 7 set: at both halves, clear: the 32 kB bank of
 *      bits 5-6), bits 0-2 the 8 kB CHR bank, bit 3 the mirroring, bit 8 $8000-$FFFF reads giving the DIP switches (0)
 *   241 (BxROM with PRG RAM: Chinese educational and plug-and-play titles): the written value the 32 kB PRG bank
 *   178 (Waixing, Coolboy's educational and RPG ports): $4800-$4803 (and their mirrors to $4FFF): 0 bit 0 the mirroring,
 *      bit 1 the UNROM mode (the 16 kB bank at $8000, the last of its 128 kB at $C000; bit 2 with it the pair 6 / 7),
 *      bit 2 without it NROM-128 (the bank at both halves; else 32 kB); 1 bits 0-2 the bank inside a 128 kB block; 2 the
 *      block; 3 bits 0-1 the 8 kB PRG RAM bank (32 kB). 8 kB CHR RAM. In the register page (+0x140-+0x144, with a state)
 *   551 (Jncota's re-releases of C&E / Supertone's RPGs: 聖火列傳, 荊軻新傳...; the nesdev wiki's, no Mesen 2 board):
 *      178's registers with 0 bits 0-1 the mode (0 32 kB, 1 UNROM, 2 NROM-128, 3 UNROM with the fixed bank's bit 0 from
 *      1's), 3 the 8 kB CHR ROM bank, the header's mirroring, 8 kB of PRG RAM (BOARD_JNCOTA)
 */
#define WAIXING178_REGISTERS ((uint8_t*)cpu_registers + 0x140)
#define D1038_DIP_READ (*((uint8_t*)cpu_registers + 0x144))

static void latch58_59_write(uint16_t address, uint8_t data) {
    (void)data;
    uint32_t bank, both, chr, horizontal;
    if (board_number == 58) {
        bank = address & 7; both = address & 0x40; chr = (address >> 3) & 7; horizontal = address & 0x80;
    } else {
        bank = (address >> 4) & 7; both = address & 0x80; chr = address & 7; horizontal = address & 0x08;
        D1038_DIP_READ = (address & 0x100) != 0;
    }
    if (both) {
        prg_16k(0, bank);
        prg_16k(2, bank);
    } else {
        prg_32k(bank >> 1);
    }
    if (!chr_8k_mapped(chr)) {
        ppu_sync();
        chr_8k(chr);
    }
    ppu_sync();
    set_ppu_nametable_mirroring(horizontal ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
}

/*
 * More latch multicarts (Mesen 2's Hp898f, Mapper57, Mapper227 as references):
 *   319 (HP-898F / KD-7/9-E: the 4-in-1s, 63- to 78-in-1s, Olympic 2000; the nesdev wiki's bits, NES 2.0's bank
 *       order: Mesen 2's formulas are the UNIF dump's shuffled one): two registers (address & $6004, at $6000-$7FFF and
 *       $E000-$FFFF): $6000 bits 4-7 the 8 kB CHR bank; $6004 bit 3 PRG A15, bit 4 A16, bit 6 NROM-256 (else bit 5 PRG
 *       A14 at both halves), bit 7 the mirroring (vertical when set). $5FF0's solder pad reads 0. Board 11 (BOARD_HP898F)
 *   57 (the GK / YH multicarts): $8000 (address & $8800) bits 0-2 and 6 the CHR bank, $8800 bits 0-2 OR'd into it, bit
 *       3 the mirroring (horizontal when set), bit 4 the 32 kB mode, bits 5-7 the 16 kB PRG bank (both halves, else the
 *       pair of bits 6-7)
 *   227 (1200-in-1, the 6-in-1 / 18-in-1 Chinese multicarts): the written address alone: bits 2-6 and 8 the 16 kB PRG
 *       bank, bit 0 the 32 kB mode, bit 7 NROM (else UNROM style: the last bank of its 128 kB at $C000 with bit 9, its
 *       first without), bit 1 the mirroring (horizontal when set). 8 kB CHR RAM, unprotected
 *   242 (the Waixing / Nanjing RPG ports, Dragon Quest translations; Mesen 2's Mapper242): the written address's bits
 *       3-6 the 32 kB PRG bank, bit 1 the mirroring (horizontal when set)
 *   148 (Sachen's SA-0037 / Tengen's 800008: Mahjong Academy, Pyramid II...; Mesen 2's Sachen_148, bus conflicts): bit 3
 *       the 32 kB PRG bank, bits 0-2 the 8 kB CHR bank
 *   212 (the "BMC Super HIK 300-in-1"s; Mesen 2's Mapper212): the written address's bit 14 the 32 kB mode (bits 1-2,
 *       else bits 0-2 at both halves), bits 0-2 the CHR bank, bit 3 the mirroring; $6000-$7FFF reads (address & $E010
 *       $6000) with bit 7 set, the game's multicart check
 *   177 (Henggedianzi's Chinese releases; Mesen 2's Henggedianzi177): the written value the 32 kB PRG bank, bit 5 the
 *       mirroring (horizontal when set)
 *   332 (Super 40-in-1 WS; Mesen 2's Super40in1Ws): $6000-$6FFF (board_6000) until $6000's bit 5 locks them: odd
 *       addresses the 8 kB CHR bank, even ones the 16 kB PRG bank (bit 3 set: NROM-128, the bank at both halves; clear:
 *       the 32 kB pair, bit 0 clear at $8000 and set at $C000),
 *       bit 4 the mirroring. The lock in the register page (+0x1ad). Board 34 (BOARD_SUPER40)
 *   15 (the 100-in-1 / 168-in-1 Contra Function 16 multicarts; Mesen 2's Mapper15): the value's bits 0-6 the 16 kB bank,
 *       bit 7 the 8 kB half, bit 6 the mirroring; address bits 0-1 the mode: 0 32 kB, 1 UNROM (the 16 kB at $C000 the
 *       bank | 7), 2 one 8 kB bank everywhere, 3 16 kB at both halves. 8 kB CHR RAM (its write protect in modes 0 / 3
 *       isn't emulated: the [m015] conversions write it there, blank in Mesen 2). A trainer at $7000 (ffe_load_trainer)
 *   236 (Realtec 8031 / 8099 / 8106 / 8155 multicarts; the nesdev wiki's latches, Mesen 2's Bmc70in1 to compare): the
 *       written address's low byte, $8000-$BFFF bit 5 the mirroring (horizontal when set) and bits 0-3 the 8 kB CHR
 *       bank (CHR RAM boards: bits 0-2 PRG A17-A19), $C000-$FFFF bits 4-5 the mode and bits 0-3 PRG A14-A17 (CHR RAM:
 *       bits 0-2 A14-A16): modes 0 / 1 UNROM (the bank, its 128 kB's last at $C000), 2 NROM-256, 3 NROM-128; the reset
 *       button clears the mode and the outer bits. Mode 1 reads $8000-$FFFF with address bits 0-3 the solder pads'
 *       (0): its banks are then a copy with each 16 bytes' first byte (board_state, 32 kB from latch_more_init, the
 *       window's WINDOW_BOARD). Its latches in the register page (+0x1ec-+0x1ed)
 *   265 (BMC-T-262, the T-262 multicarts with 8 kB of CHR RAM; the nesdev wiki's latch, Mesen 2's T262 for its address
 *       bits): until the written address's bit 13 locks it, address bits 5, 6 and 8 the 128 kB block (16 kB bank bits
 *       3-5), bit 7 the mode (set: the bank at both halves; clear: the block's bank 7 at $C000), bit 1 the mirroring
 *       (horizontal when set); the value's bits 0-2 the 16 kB bank, at any time. Not emulated: the wiki's A14 bit
 *       (NROM-256), whose place it leaves unclear. In the register page (+0x1ee-+0x1ef)
 *   226 (the 76-in-1 / Super 42-in-1 multicarts; the nesdev wiki's registers, Mesen 2's Mapper226 to compare):
 *       $8000-$FFFF by address bit 0, R0 / R1: the 16 kB PRG bank R0 bits 0-4, R0 bit 7 and R1 bit 0 (bits 0-6),
 *       R0 bit 5 set the bank at both halves (clear: the even / odd pair), R0 bit 6 the mirroring (vertical when set);
 *       one 8 kB CHR bank; the reset button clears them. In the register page (+0x200-+0x201)
 *   63 (NTDEC's TH2291-3 / CH-011 and the 82AB multicarts: 124-in-1, Powerful 255-in-1; the nesdev wiki's latch): the
 *       written address, bits 3-10 (submapper 1: 3-9) the 32 kB PRG bank, bit 1 32 kB (clear: NROM-128, bit 2 the 16 kB
 *       half), bit 0 the mirroring (horizontal when set); 8 kB of CHR RAM. Not emulated: open bus for unpopulated banks
 *       (the menus' game counts; they wrap here), the CHR RAM's write protect. In the register page (+0x217-+0x218)
 *   200 / 201 / 225 (address latch multicarts; the nesdev wiki's, Mesen 2's Mapper200 / NovelDiamond / Mapper225 to
 *       compare): 200 the written address's bits 0-2 the 16 kB PRG bank at both halves and the 8 kB CHR bank, bit 3
 *       the mirroring (vertical when set); 201 bits 0-1 the 32 kB PRG bank, bits 0-2 the CHR bank; 225 (the 52 / 64 /
 *       72-in-1s) bit 14 the banks' bit 6, bits 6-11 the 16 kB PRG bank, bit 12 the bank at both halves (clear: the
 *       pair), bits 0-5 the 8 kB CHR bank, bit 13 the mirroring (horizontal when set), and four nibbles of RAM at
 *       $5800-$5FFF (board_low; +0x219-+0x21c in the register page)
 *   108 (the FDS conversions' boards: Bubble Bobble (LH31), Falsion, Dababa, Pro Wrestling; the nesdev wiki's
 *       submappers, Mesen 2's Bb to compare): the last 32 kB at $8000, an 8 kB PRG bank at $6000 (+0x21e, $FF at power
 *       on); a write's value: 1 (DH-08) at $F000-$FFFF the $6000 bank (8 kB of CHR RAM); 2 at $E000-$FFFF the $6000
 *       bank and the 8 kB CHR bank; 3 at $8000-$FFFF the $6000 bank (CHR RAM); 4 (Pro Wrestling) at $8000-$FFFF the
 *       CHR bank ($6000 the last 8 kB); 0 as Mesen 2's: $8000-$8FFF / $A000-$AFFF / $C000-$CFFF / $E000-$FFFF both,
 *       $9000 / $B000 / $D000 the CHR bank's bit 0
 *   221 (NTDEC's N625092 multicarts; Mesen 2's Mapper221 to compare, the wiki's latches): $8000-$BFFF the written
 *       address the mode (bits 2-7 the 16 kB outer bank, bit 0 the mirroring, horizontal when set; bit 1 set with bit
 *       8: UNROM, the outer | 7 at $C000; bit 1 alone 32 kB; clear: the bank at both halves), $C000-$FFFF address bits
 *       0-2 the inner bank. In the register page (+0x202-+0x204: the mode, the inner bank)
 * 319's and 57's registers in the register page (+0x1a0-+0x1a3, zeroed at each load, saved with a state)
 */
#define LATCH226 ((uint8_t*)cpu_registers + 0x200)
#define LATCH63 ((uint8_t*)cpu_registers + 0x217)

static void latch63_apply(void) {
    uint32_t a = LATCH63[0] | (LATCH63[1] << 8);
    uint32_t bank = (a >> 3) & ((board_submapper == 1) ? 0x7f : 0xff);
    if (a & 2) {
        prg_32k(bank);
    } else {
        prg_16k(0, (bank << 1) | ((a >> 2) & 1));
        prg_16k(2, (bank << 1) | ((a >> 2) & 1));
    }
    set_ppu_nametable_mirroring((a & 1) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
}
#define LATCH221 ((uint8_t*)cpu_registers + 0x202)

#define M233_RESET (*((uint8_t*)cpu_registers + 0x278))

static void latch226_apply(void) {
    const uint8_t* r = LATCH226;
    uint32_t bank = (r[0] & 0x1f) | ((cartridge_mapper_number == 233) ? ((M233_RESET & 1) << 5) : ((r[0] & 0x80) >> 2)) |
        ((r[1] & 1) << 6);
    if (r[0] & 0x20) {
        prg_16k(0, bank);
        prg_16k(2, bank);
    } else {
        prg_32k(bank >> 1);
    }
    set_ppu_nametable_mirroring((r[0] & 0x40) ? NAMETABLE_MIRRORING_VERTICAL : NAMETABLE_MIRRORING_HORIZONTAL);
}

static void latch221_apply(void) {
    const uint8_t* r = LATCH221;
    uint32_t mode = r[0] | (r[1] << 8), outer = (mode >> 2) & 0x3f, inner = r[2] & 7;
    if (mode & 2) {
        if (mode & 0x100) {
            prg_16k(0, outer | inner);
            prg_16k(2, outer | 7);
        } else {
            prg_32k((outer | (inner & 6)) >> 1);
        }
    } else {
        prg_16k(0, outer | inner);
        prg_16k(2, outer | inner);
    }
    set_ppu_nametable_mirroring((mode & 1) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
}

#define LATCH236 ((uint8_t*)cpu_registers + 0x1ec)
#define LATCH265 ((uint8_t*)cpu_registers + 0x1ee)

static void latch265_apply(void) {
    const uint8_t* l = LATCH265;
    uint32_t base = l[0] & 0x38, bank = base | (l[1] & 7);
    prg_16k(0, bank);
    prg_16k(2, (l[0] & 0x40) ? bank : (base | 7));
    set_ppu_nametable_mirroring((l[1] & 0x80) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
}
#define LATCH332_LOCK (*((uint8_t*)cpu_registers + 0x1ad))
#define LATCH319_REGISTERS ((uint8_t*)cpu_registers + 0x1a0)
#define LATCH57_REGISTERS ((uint8_t*)cpu_registers + 0x1a2)

static void latch_more_apply(void) {
    uint32_t chr, horizontal;
    if (board_number == BOARD_HP898F) {
        const uint8_t* r = LATCH319_REGISTERS;
        uint32_t bank = ((r[1] >> 4) & 1) << 2 | ((r[1] >> 3) & 1) << 1;
        if (r[1] & 0x40) {
            prg_32k(bank >> 1);
        } else {
            prg_16k(0, bank | ((r[1] >> 5) & 1));
            prg_16k(2, bank | ((r[1] >> 5) & 1));
        }
        chr = r[0] >> 4;
        horizontal = !(r[1] & 0x80);
    } else {
        const uint8_t* r = LATCH57_REGISTERS;
        uint32_t bank = (r[1] >> 5) & 7;
        if (r[1] & 0x10) {
            prg_32k(bank >> 1);
        } else {
            prg_16k(0, bank);
            prg_16k(2, bank);
        }
        chr = ((r[0] & 0x40) >> 3) | ((r[0] | r[1]) & 7);
        horizontal = r[1] & 0x08;
    }
    if (!chr_8k_mapped(chr)) {
        ppu_sync();
        chr_8k(chr);
    }
    ppu_sync();
    set_ppu_nametable_mirroring(horizontal ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
}

// 319's at $6000-$7FFF (board_6000) and $E000-$FFFF, 57's and 227's at $8000-$FFFF
static void* board_buffer(void);
static void* board_buffer_alloc(size_t size, size_t align);

static void latch236_apply(void) {
    const uint8_t* l = LATCH236;
    uint32_t bank = num_1k_VROM_pages ? (l[1] & 0x0f) : (((l[0] & 7) << 3) | (l[1] & 7));
    switch ((l[1] >> 4) & 3) {
        case 0:
            prg_16k(0, bank);
            prg_16k(2, bank | 7);
            break;
        case 1:
            if (board_buffer() != NULL) {
                // (the solder pads, 0, as address bits 0-3 of every read)
                const uint32_t pads = 0;
                uint8_t* copy = board_buffer();
                for (uint32_t i = 0; i < 4; i++) {
                    const uint8_t* src = prg_page(((i < 2) ? bank : (bank | 7)) * 2 + (i & 1));
                    for (uint32_t j = 0; j < 0x2000; j++) {
                        copy[i * 0x2000 + j] = src[(j & ~0x0fu) | pads];
                    }
                }
                dcache_wback_range((uintptr_t)copy, 0x8000);
                for (uint32_t i = 0; i < 4; i++) {
                    set_6502_bank_pointer(i, copy + i * 0x2000);
                }
            }
            break;
        case 2:
            prg_32k(bank >> 1);
            break;
        case 3:
            prg_16k(0, bank);
            prg_16k(2, bank);
            break;
    }
    if (num_1k_VROM_pages) {
        chr_8k(l[0] & 0x0f);
    }
    set_ppu_nametable_mirroring((l[0] & 0x20) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
}

static void latch_more_write(uint16_t address, uint8_t data) {
    if (board_number == 108) {
        uint32_t prg = 0, chr = 0;
        switch (board_submapper) {
            case 1: prg = address >= 0xf000; break;
            case 2: prg = chr = address >= 0xe000; break;
            case 3: prg = 1; break;
            case 4: chr = 1; break;
            default:
                if (((address & 0x9000) == 0x8000) || (address >= 0xf000)) {
                    prg = chr = 1;
                } else {
                    data &= 1;
                    chr = 1;
                }
                break;
        }
        ppu_sync();
        if (prg) {
            ((uint8_t*)cpu_registers)[0x21e] = data;
            set_wram_bank_pointer(prg_page(data));
        }
        if (chr && num_1k_VROM_pages) {
            chr_8k(data);
        }
        return;
    }
    if (board_number == 200) {
        ppu_sync();
        prg_16k(0, address & 7);
        prg_16k(2, address & 7);
        chr_8k(address & 7);
        set_ppu_nametable_mirroring((address & 8) ? NAMETABLE_MIRRORING_VERTICAL : NAMETABLE_MIRRORING_HORIZONTAL);
        return;
    }
    if (board_number == 201) {
        ppu_sync();
        prg_32k(address & 3);
        chr_8k(address & 7);
        return;
    }
    if (board_number == 225) {
        uint32_t high = (address >> 8) & 0x40, bank = high | ((address >> 6) & 0x3f);
        ppu_sync();
        if (address & 0x1000) {
            prg_16k(0, bank);
            prg_16k(2, bank);
        } else {
            prg_32k(bank >> 1);
        }
        chr_8k(high | (address & 0x3f));
        set_ppu_nametable_mirroring((address & 0x2000) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
        return;
    }
    if (board_number == 63) {
        LATCH63[0] = (uint8_t)address;
        LATCH63[1] = (uint8_t)(address >> 8);
        ppu_sync();
        latch63_apply();
        return;
    }
    if (board_number == 226) {
        LATCH226[address & 1] = data;
        latch226_apply();
        return;
    }
    if (board_number == 221) {
        if (address < 0xc000) {
            LATCH221[0] = (uint8_t)address;
            LATCH221[1] = (uint8_t)(address >> 8);
        } else {
            LATCH221[2] = address & 7;
        }
        ppu_sync();
        latch221_apply();
        return;
    }
    if (board_number == BOARD_T262) {
        uint8_t* l = LATCH265;
        if (!(l[0] & 0x80)) {
            l[0] = (uint8_t)((((address >> 5) & 3) << 3) | (((address >> 8) & 1) << 5) | (((address >> 7) & 1) << 6) |
                (((address >> 13) & 1) << 7));
            l[1] = (l[1] & 0x7f) | (((address >> 1) & 1) << 7);
        }
        l[1] = (l[1] & 0x80) | (data & 7);
        ppu_sync();
        latch265_apply();
        return;
    }
    if (board_number == 236) {
        LATCH236[(address & 0x4000) ? 1 : 0] = (uint8_t)address;
        ppu_sync();
        latch236_apply();
        return;
    }
    if (board_number == BOARD_HP898F) {
        if ((address & 0x6000) == 0x6000) {
            LATCH319_REGISTERS[(address >> 2) & 1] = data;
            latch_more_apply();
        }
        return;
    }
    if (board_number == 57) {
        LATCH57_REGISTERS[(address & 0x0800) ? 1 : 0] = data;
        latch_more_apply();
        return;
    }
    if (board_number == 177) {
        prg_32k(data);
        ppu_sync();
        set_ppu_nametable_mirroring((data & 0x20) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
        return;
    }
    if (board_number == BOARD_SUPER40) {
        if (LATCH332_LOCK || (address >= 0x7000)) {
            return;
        }
        if (address & 1) {
            if (!chr_8k_mapped(data)) {
                ppu_sync();
                chr_8k(data);
            }
            return;
        }
        LATCH332_LOCK = (data & 0x20) != 0;
        uint32_t pair = (~data >> 3) & 1;
        prg_16k(0, data & ~pair);
        prg_16k(2, data | pair);
        ppu_sync();
        set_ppu_nametable_mirroring((data & 0x10) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
        return;
    }
    if (board_number == 15) {
        uint32_t half = data >> 7, bank = (data & 0x7f) << 1;
        switch (address & 3) {
            case 0:
                for (uint32_t i = 0; i < 4; i++) {
                    set_6502_bank_pointer(i, prg_page((bank + i) ^ half));
                }
                break;
            case 1: case 3:
                bank |= half;
                set_6502_bank_pointer(0, prg_page(bank));
                set_6502_bank_pointer(1, prg_page(bank + 1));
                bank = (((address & 3) == 3) ? bank : (bank | 0x0e)) | half;
                set_6502_bank_pointer(2, prg_page(bank));
                set_6502_bank_pointer(3, prg_page(bank + 1));
                break;
            case 2:
                for (uint32_t i = 0; i < 4; i++) {
                    set_6502_bank_pointer(i, prg_page(bank | half));
                }
                break;
        }
        ppu_sync();
        set_ppu_nametable_mirroring((data & 0x40) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
        return;
    }
    if ((board_number == 242) || (board_number == 148) || (board_number == 212)) {
        uint32_t chr = 0, horizontal;
        if (board_number == 242) {
            prg_32k((address >> 3) & 0x0f);
            horizontal = address & 2;
        } else if (board_number == 148) {
            data &= NES_WINDOW_BYTE(address);
            prg_32k((data >> 3) & 1);
            chr = data & 7;
            // (the header's mirroring)
            horizontal = 0;
        } else {
            if (address & 0x4000) {
                prg_32k((address >> 1) & 3);
            } else {
                prg_16k(0, address & 7);
                prg_16k(2, address & 7);
            }
            chr = address & 7;
            horizontal = address & 8;
        }
        if ((board_number != 242) && !chr_8k_mapped(chr)) {
            ppu_sync();
            chr_8k(chr);
        }
        if (board_number != 148) {
            ppu_sync();
            set_ppu_nametable_mirroring(horizontal ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
        }
        return;
    }
    uint32_t bank = ((address >> 2) & 0x1f) | ((address & 0x100) >> 3), both = address & 1;
    if (address & 0x80) {
        if (both) {
            prg_32k(bank >> 1);
        } else {
            prg_16k(0, bank);
            prg_16k(2, bank);
        }
    } else {
        prg_16k(0, both ? (bank & 0x3e) : bank);
        prg_16k(2, (address & 0x200) ? (bank | 7) : (bank & 0x38));
    }
    ppu_sync();
    set_ppu_nametable_mirroring((address & 2) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
}

static void ffe_load_trainer(void);
__attribute__((cold, noinline)) static void latch_more_init(void) {
    memset(LATCH319_REGISTERS, 0, 4);
    if (board_number == BOARD_HP898F) {
        mapper_6000_mode = MAPPER_6000_REGISTERS;
        board_6000 = 1;
    }
    if (board_number == 227) {
        latch_more_write(0x8000, 0);
        chr_8k(0);
    } else if ((board_number == 242) || (board_number == 148)) {
        prg_32k(0);
        chr_8k(0);
        if (board_number == 242) {
            set_ppu_nametable_mirroring(NAMETABLE_MIRRORING_VERTICAL);
        }
    } else if (board_number == 212) {
        mapper_6000_mode = MAPPER_6000_REGISTERS;
        mapper_6000_reads = 1;
        board_6000 = 1;
        latch_more_write(0x8000, 0);
    } else if (board_number == 108) {
        ((uint8_t*)cpu_registers)[0x21e] = 0xff;
        prg_32k(num_8k_ROM_pages / 4 - 1);
        // (the $6000 window reads its ROM bank only with WRAM allocated: 6502_readbyte.c gives open bus without it)
        if (WRAM == NULL) {
            resize_wram(0x2000);
        }
        mapper_6000_mode = MAPPER_6000_ROM;
        set_wram_bank_pointer(prg_page(0xff));
        chr_8k(0);
    } else if ((board_number == 200) || (board_number == 201) || (board_number == 225)) {
        prg_32k(0);
        chr_8k(0);
        memset((uint8_t*)cpu_registers + 0x219, 0, 4);
    } else if (board_number == 63) {
        LATCH63[0] = LATCH63[1] = 0;
        chr_8k(0);
        latch63_apply();
    } else if ((board_number == 226) || (board_number == 221)) {
        memset(LATCH226, 0, 5);
        M233_RESET = 0;
        chr_8k(0);
        if (board_number == 226) {
            latch226_apply();
        } else {
            latch221_apply();
        }
    } else if (board_number == BOARD_T262) {
        LATCH265[0] = LATCH265[1] = 0;
        chr_8k(0);
        latch265_apply();
    } else if (board_number == 236) {
        LATCH236[0] = LATCH236[1] = 0;
        // (4 kB aligned: the CPU window's MMU pages)
        board_buffer_alloc(0x8000, 0x1000);
        chr_8k(0);
        latch236_apply();
    } else if ((board_number == 177) || (board_number == 15)) {
        chr_8k(0);
        latch_more_write(0x8000, 0);
        if (board_number == 15) {
            // (the Nesticle conversions' trainers, started from $7000: Doraemon's reset vector is $7100)
            ffe_load_trainer();
        }
    } else if (board_number == BOARD_SUPER40) {
        LATCH332_LOCK = 0;
        mapper_6000_mode = MAPPER_6000_REGISTERS;
        board_6000 = 1;
        chr_8k(0);
        latch_more_write(0x6000, 0);
    } else {
        latch_more_apply();
    }
}

/*
 * Sachen's 74LS374N (mapper 150: Sachen's own carts, Olympic I.Q., Poker III, Strategist...; Mesen 2's
 * Sachen74LS374N): $4100 (address & $C101) selects one of eight 3-bit registers, $4101 writes it and reads it back
 * (bits 0-2, the rest open bus), at $4100-$7FFF: 2 bit 0 / 4 bit 0 / 6 bits 0-1 the 8 kB CHR bank (bits 2, 0-1 on
 * 150), 5 bits 0-1 the 32 kB PRG bank, 7 bits 1-2 the nametables (0, 0, 0, 1 / horizontal / vertical / one screen).
 * The DIP switch (OR $04 on writes) off. In the register page (+0x1a4-+0x1ac, saved with a state)
 * Sachen's 8259 (mappers 141 / 138 / 139 / 137: 8259A / B / C / D; Mesen 2's Sachen8259) on the same registers: 5 the
 * 32 kB PRG bank, 7 bit 0 the "simple" mode (registers 1-3 as 0, vertical mirroring; D horizontal), bits 1-2 the
 * nametables (vertical, horizontal, 0 1 1 1, one screen; D's first two swapped); CHR ROM in 2 kB banks (A / B / C:
 * register n | 4 << 3, shifted 1 / 0 / 2 and OR'd 0 1 0 1 / 0 0 0 0 / 0 1 2 3; none with CHR RAM) or D's four 1 kB banks
 * at $0000-$0FFF (4's bits and 6's bit 0 above 1-3), the last 4 kB at $1000
 */
#define SACHEN374 ((uint8_t*)cpu_registers + 0x1a4)

static void sachen8259_apply(void) {
    const uint8_t* r = SACHEN374 + 1;
    uint32_t simple = r[7] & 1, d = (board_number == 137);
    prg_32k(r[5]);
    if (d) {
        uint32_t chr[4] = { r[0], ((r[4] & 1) << 4) | r[simple ? 0 : 1], ((r[4] & 2) << 3) | r[simple ? 0 : 2],
            ((r[4] & 4) << 2) | ((r[6] & 1) << 3) | r[simple ? 0 : 3] };
        ppu_sync();
        for (uint32_t i = 0; i < 4; i++) {
            chr_1k(i, chr[i]);
        }
        uint32_t pages = num_1k_VROM_pages ? num_1k_VROM_pages : 8;
        chr_4k(1, pages / 4 - 1);
    } else if (num_1k_VROM_pages) {
        static const uint8_t shifts[3] FRNES_COLD_RODATA = { 1, 0, 2 };
        static const uint8_t ors[3][4] FRNES_COLD_RODATA = { { 0, 1, 0, 1 }, { 0, 0, 0, 0 }, { 0, 1, 2, 3 } };
        uint32_t v = (board_number == 141) ? 0 : ((board_number == 138) ? 1 : 2);
        uint32_t high = r[4] << 3;
        ppu_sync();
        for (uint32_t i = 0; i < 4; i++) {
            chr_2k(i * 2, (((high | r[(simple || !i) ? 0 : i]) << shifts[v]) | ors[v][i]) * 2);
        }
    }
    ppu_sync();
    uint32_t vertical_first = !d;
    switch (simple ? 0 : ((r[7] >> 1) & 3)) {
        case 0: set_ppu_nametable_mirroring(vertical_first ? NAMETABLE_MIRRORING_VERTICAL : NAMETABLE_MIRRORING_HORIZONTAL); break;
        case 1: set_ppu_nametable_mirroring(vertical_first ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL); break;
        case 2: set_ppu_nametable_pages(0, 1, 1, 1); break;
        case 3: set_ppu_nametable_mirroring(NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW); break;
    }
}

static void sachen374_apply(void) {
    const uint8_t* r = SACHEN374 + 1;
    uint32_t chr = ((r[4] & 1) << 2) | (r[6] & 3);
    if (!chr_8k_mapped(chr)) {
        ppu_sync();
        chr_8k(chr);
    }
    prg_32k(r[5] & 3);
    ppu_sync();
    switch ((r[7] >> 1) & 3) {
        case 0: set_ppu_nametable_pages(0, 0, 0, 1); break;
        case 1: set_ppu_nametable_mirroring(NAMETABLE_MIRRORING_HORIZONTAL); break;
        case 2: set_ppu_nametable_mirroring(NAMETABLE_MIRRORING_VERTICAL); break;
        case 3: set_ppu_nametable_mirroring(NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW); break;
    }
}

// $4100-$5FFF (board_low) and $6000-$7FFF (board_6000)
static void sachen374_write(uint16_t address, uint8_t data) {
    uint8_t* r = SACHEN374;
    switch (address & 0xc101) {
        case 0x4100:
            r[0] = data & 7;
            break;
        case 0x4101:
            r[1 + r[0]] = data & 7;
            if (board_number == 150) {
                sachen374_apply();
            } else {
                sachen8259_apply();
            }
            break;
    }
}

static uint8_t sachen374_read(uint16_t address, uint8_t open_bus) {
    const uint8_t* r = SACHEN374;
    return ((address & 0xc101) == 0x4101) ? (uint8_t)((open_bus & 0xf8) | r[1 + r[0]]) : open_bus;
}

__attribute__((cold, noinline)) static void sachen374_init(void) {
    memset(SACHEN374, 0, 9);
    mapper_6000_mode = MAPPER_6000_REGISTERS;
    board_6000 = 1;
    if (board_number == 150) {
        sachen374_apply();
    } else {
        // (as Mesen 2's: the PRG bank set, the CHR and nametables as they were until the first write)
        prg_32k(0);
        chr_8k(0);
    }
}

static void waixing178_apply(void) {
    const uint8_t* r = WAIXING178_REGISTERS;
    uint32_t inner = r[1] & 7, block = r[2] << 3;
    if (board_number == BOARD_JNCOTA) {
        // (the mode as 178's bits 1-2: 1 UNROM, 3 with the fixed bank's bit 0, 2 NROM-128)
        static const uint8_t modes[4] FRNES_COLD_RODATA = { 0, 2, 4, 6 };
        uint32_t mode = modes[r[0] & 3];
        if (mode & 2) {
            prg_16k(0, block | inner);
            prg_16k(2, block | ((mode & 4) ? (6 | (r[1] & 1)) : 7));
        } else if (mode & 4) {
            prg_16k(0, block | inner);
            prg_16k(2, block | inner);
        } else {
            prg_32k((block | inner) >> 1);
        }
        if (!chr_8k_mapped(r[3])) {
            chr_8k(r[3]);
        }
        return;
    }
    if (r[0] & 2) {
        prg_16k(0, block | inner);
        prg_16k(2, block | ((r[0] & 4) ? (6 | (r[1] & 1)) : 7));
    } else if (r[0] & 4) {
        prg_16k(0, block | inner);
        prg_16k(2, block | inner);
    } else {
        prg_16k(0, block | inner);
        prg_16k(2, (block | inner) + 1);
    }
    if (WRAM != NULL) {
        uint32_t banks = WRAM_size / 0x2000;
        set_wram_bank_pointer(WRAM + ((r[3] & 3) % banks) * 0x2000);
    }
    set_ppu_nametable_mirroring((r[0] & 1) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
}

static void waixing178_write_low(uint16_t address, uint8_t data) {
    if ((address & 0xf800) == 0x4800) {
        WAIXING178_REGISTERS[address & 3] = data;
        ppu_sync();
        waixing178_apply();
    }
}

/*
 * Waixing's mapper 164 (Final Fantasy V / VII, the Pokemon and Dragon Quest ports; the nesdev wiki's registers, Mesen
 * 2's Waixing164 to compare): registers by address & $FF00 at $5000-$5300 (board_low). $5000: bits 0-3 and 5 PRG
 * A14-A17 and A18, bit 4 the mode, bit 6 the fixed bank's choice; $5100 bits 0-1 PRG A19-A20; $5300 bit 7 the
 * mirroring in mode 1 (set vertical). Mode 0: 16 kB at $8000 from them, at $C000 bank $1F (bit 6 set: $1C, or $1E
 * with bit 0), vertical mirroring; mode 1: 32 kB, $5000 bits 0-3 A15-A18. 8 kB of CHR RAM. Not emulated: the 1 bpp
 * mode ($5000 bit 7, CHR A3 / A12 from PPU A0 / A9 on the pattern fetches) and the 93C66 EEPROM ($5200 / $5500: the
 * saves), neither in Mesen 2. In the register page (+0x1c0-+0x1c3, zeroed at each load, saved with a state)
 */
#define WAIXING164_REGISTERS ((uint8_t*)cpu_registers + 0x1c0)

/*
 * Mapper 240 (C&E / 卡聖's 吞食天地 / 封神榜 / 生死搏殺 ports; Mesen 2's Mapper240 to compare, the nesdev wiki's): a write to
 * $4020-$5FFF (board_low), bits 4-7 the 32 kB PRG bank, bits 0-3 the 8 kB CHR bank; the WRAM the header's
 */
static void board240_write_low(uint16_t address, uint8_t data) {
    (void)address;
    ppu_sync();
    prg_32k(data >> 4);
    chr_8k(data & 0x0f);
}

static void waixing164_apply(void) {
    const uint8_t* r = WAIXING164_REGISTERS;
    uint32_t high = (r[1] & 3) << 5;
    if (r[0] & 0x10) {
        prg_32k((high >> 1) | (r[0] & 0x0f));
        set_ppu_nametable_mirroring((r[3] & 0x80) ? NAMETABLE_MIRRORING_VERTICAL : NAMETABLE_MIRRORING_HORIZONTAL);
    } else {
        prg_16k(0, high | ((r[0] & 0x20) >> 1) | (r[0] & 0x0f));
        prg_16k(2, high | ((r[0] & 0x40) ? (0x1c | ((r[0] & 1) << 1)) : 0x1f));
        set_ppu_nametable_mirroring(NAMETABLE_MIRRORING_VERTICAL);
    }
}

/*
 * Mapper 558 (the YC-03-09 boards: Yancheng's and Waixing's later RPG ports, 164's later revision; the nesdev wiki's
 * registers) on 164's code: by address & $FF00 at $5000-$5300, one 32 kB PRG bank, $5000 bits 0-3 PRG A15-A18 (A15-A16
 * held high until $5300's bit 2 is set: bank 3 at power on), $5100 bits 0-1 A19-A20; until $5300's bit 0 is set,
 * $5000-$5200 writes have bits 0 and 1 swapped. 8 kB of CHR RAM, the header's mirroring. Not emulated: the 93C66
 * EEPROM ($5200 / $5500, the Yancheng boards' saves; the Waixing boards' are battery WRAM). Board 88 (BOARD_YC0309)
 */
static void m558_apply(void) {
    const uint8_t* r = WAIXING164_REGISTERS;
    uint32_t bank = ((r[1] & 3) << 4) | (r[0] & 0x0f);
    prg_32k((r[3] & 4) ? bank : (bank | 3));
}

static void waixing164_write_low(uint16_t address, uint8_t data) {
    if ((address < 0x5000) || (address >= 0x5400)) {
        return;
    }
    if (board_number == BOARD_YC0309) {
        if (((address & 0x0300) != 0x0300) && !(WAIXING164_REGISTERS[3] & 1)) {
            data = (data & ~3) | ((data & 1) << 1) | ((data >> 1) & 1);
        }
        WAIXING164_REGISTERS[(address >> 8) & 3] = data;
        m558_apply();
        return;
    }
    if ((address & 0x0300) != 0x0200) {
        WAIXING164_REGISTERS[(address >> 8) & 3] = data;
        ppu_sync();
        waixing164_apply();
    }
}

__attribute__((cold, noinline)) static void latch_boards_init() {
    switch (board_number) {
        case 58: case 59:
            D1038_DIP_READ = 0;
            latch58_59_write(0x8000, 0);
            break;
        case 241: case 240:
            prg_32k(0);
            chr_8k(0);
            break;
        case 178: case BOARD_JNCOTA:
            memset(WAIXING178_REGISTERS, 0, 4);
            resize_wram((board_number == BOARD_JNCOTA) ? 0x2000 : 0x8000);
            waixing178_apply();
            chr_8k(0);
            break;
        case 164:
            memset(WAIXING164_REGISTERS, 0, 4);
            chr_8k(0);
            waixing164_apply();
            break;
        case BOARD_YC0309:
            memset(WAIXING164_REGISTERS, 0, 4);
            chr_8k(0);
            m558_apply();
            break;
    }
}

/**
 * J.Y. Company's ASIC (mappers 90, 209, 211: Mortal Kombat II, Tiny Toon Adventures 6, Aladdin, Donkey Kong Country 4...;
 * Mesen 2's JyCompany as the reference), a cold board: $8000-$8003 the 8 kB PRG registers, $9000-$9007 / $A000-$A007
 * the CHR registers' low / high bytes, $B000-$B007 the nametable registers, $C000-$C007 the IRQ, $D000 the modes (bits
 * 0-1 PRG: 32 / 16 / 8 kB, 3 with the bank bits reversed; bit 2 the last bank from the registers; bits 3-4 CHR: 8 / 4
 * / 2 / 1 kB; bit 5 the nametable registers, always on 211 and off on 90; bit 6 every nametable from CHR ROM; bit 7 PRG
 * ROM at $6000), $D001 the mirroring, $D002 bit 7 the nametable registers' bit 7 that picks CIRAM (its page from bit 0;
 * else the register pair's CHR ROM page, a copy in VRAM page 4-7), $D003 the CHR block
 * (bits 0 and 3-4, with bit 5 clear) and bit 7 the CHR registers 2-3 as 0-1; $5000 the DIP switches (0), $5800 /
 * $5801 a multiplier (the product's low / high byte read back), $5803 a byte of RAM.
 * The IRQ: a prescaler (8 or 3 bits) and counter clocked up or down, the IRQ when the counter wraps, by CPU cycles
 * (source 0: counted in jumps on the board timer) or, the games' usual sources, PPU A12 rises (1) and PPU reads (2):
 * those a line at a time, while the IRQ is on, at dot 260 of each rendered line (0-239 and the pre-render line, as the
 * MMC3's clock; none with rendering off), the line's rises from PPUCTRL's pattern tables (8 from the sprites at $1000,
 * 34 from the background there) or ~170 reads, an event of the board timer each (jy_line_*). Not emulated: the CPU
 * write source (3), 209's MMC2 style CHR latch (its latches stay at 0 / 4), writes to a ROM nametable reaching CIRAM
 * (they change the copy), and the ROM nametables of a cartridge with no CHR ROM (CIRAM's then). State in the register page (+0x100-+0x13f, with a state).
 * Mapper 282 (the JY-0xx Super HiK 4-in-1s, 1 MB of PRG and of CHR; runs as 209) has its own $D003 (the nesdev wiki's
 * 282 page): bits 1-2 PRG A18-A19 over a 256 kB inner PRG bank; bit 5 the inner CHR bank 512 kB (else 256 kB, bit 0
 * then CHR A18), bit 3 CHR A19 when bit 4 is set. Mappers 281 and 295 (the other JY-0xx multicarts; runs as 209 too):
 * $D003 bits 0-1 (281) / 0-2 (295) a 256 kB / 128 kB outer bank of PRG and of CHR, the inner banks masked to its size.
 * Mapper 358 (JY-016 1997 Super Game 7-in-1, JY-113, JY-117; the nesdev wiki's, runs as 209) as 386 but for its outer
 * PRG bank, $D003 bits 1-2 (PRG A18-A19)
 * Mappers 386 / 387 (the JY-087 ... JY-094 Super HiK 4 / 5-in-1s; the nesdev wiki's, runs as 209): $D003 bits 1 and 3
 * the outer PRG bank (256 kB on 386, 128 kB on 387), bits 0 and 2 CHR A18-A19 over a 256 kB inner CHR bank, bit 5 a
 * 512 kB one (bit 0 then ignored). Mapper 388 (the JY-056 Super HiK 5-in-1...; runs as 90, no ROM nametables): $D003
 * bits 2-3 PRG A18-A19, bits 0-1 CHR A18-A19 (bit 5 the 512 kB inner CHR bank, bit 0 then ignored)
 */
typedef struct {
    uint8_t prg[4], chr_low[8], chr_high[8], nt_low[4], nt_high[4];
    uint8_t mode, mirroring, d003, irq_enabled, irq_mode, prescaler, counter, xor_reg;
    uint8_t mult1, mult2, ram, next_line_set;
    uint16_t next_line;
    uint8_t d002, pad;
    uint32_t timer_base;
} jy_state_t;
#define JY ((jy_state_t*)((uint8_t*)cpu_registers + 0x100))
// the CHR ROM pages copied to VRAM pages 4-7 for the ROM nametables (jy_nametables), after the state (+0x130-+0x138)
typedef struct {
    uint16_t page[4];
    uint8_t valid;
} jy_nametables_t;
#define JY_NT ((jy_nametables_t*)((uint8_t*)cpu_registers + 0x130))

static uint32_t jy_prg_bits(uint32_t value) {
    // (PRG mode 3: the bank number's bits reversed)
    if ((JY -> mode & 3) != 3) {
        return value;
    }
    return ((value & 0x01) << 6) | ((value & 0x02) << 4) | ((value & 0x04) << 2) | ((value & 0x10) >> 2) |
        ((value & 0x20) >> 4) | ((value & 0x40) >> 6);
}

// 281's and 295's outer bank: its size as a shift of 8 kB PRG / CHR banks (256 / 128 kB), 0 for the others
static uint32_t jy_outer_shift(void) {
    return (cartridge_mapper_number == 281) ? 5 : (cartridge_mapper_number == 295) ? 4 : 0;
}

static uint32_t jy_outer_bank(void) {
    return JY -> d003 & ((cartridge_mapper_number == 281) ? 3 : 7);
}

// an 8 kB PRG page through 282's, 281's or 295's outer bank
static uint8_t* jy_prg_page(uint32_t page) {
    uint32_t shift = jy_outer_shift();
    if (cartridge_mapper_number == 282) {
        page = (page & 0x1f) | ((JY -> d003 & 6) << 4);
    } else if ((cartridge_mapper_number == 386) || (cartridge_mapper_number == 387)) {
        uint32_t bits = (cartridge_mapper_number == 386) ? 5 : 4, d = JY -> d003;
        page = (page & ((1u << bits) - 1)) | ((((d >> 1) & 1) | ((d >> 2) & 2)) << bits);
    } else if (cartridge_mapper_number == 358) {
        page = (page & 0x1f) | (((JY -> d003 >> 1) & 3) << 5);
    } else if (cartridge_mapper_number == 388) {
        page = (page & 0x1f) | (((JY -> d003 >> 2) & 3) << 5);
    } else if (shift) {
        page = (page & ((1u << shift) - 1)) | (jy_outer_bank() << shift);
    }
    return prg_page(page);
}

static uint32_t jy_chr_reg(uint32_t index) {
    const jy_state_t* j = JY;
    uint32_t chr_mode = (j -> mode >> 3) & 3;
    if ((chr_mode >= 2) && (j -> d003 & 0x80) && ((index == 2) || (index == 3))) {
        index -= 2;
    }
    if (jy_outer_shift()) {
        // (bank numbers in the mode's units: 8 kB banks << the mode)
        uint32_t shift = jy_outer_shift() + chr_mode;
        return ((j -> chr_low[index] | (j -> chr_high[index] << 8)) & ((1u << shift) - 1)) | (jy_outer_bank() << shift);
    }
    if ((cartridge_mapper_number == 358) || (cartridge_mapper_number == 386) || (cartridge_mapper_number == 387) ||
        (cartridge_mapper_number == 388)) {
        // (in the mode's units: 256 kB is 32 8 kB banks, 64 4 kB...; bit 5 512 kB, CHR A18 from the bank then)
        uint32_t d = j -> d003, bits = ((d & 0x20) ? 6 : 5) + chr_mode;
        uint32_t outer = (cartridge_mapper_number == 388) ? ((d & 0x20) ? ((d >> 1) & 1) : (d & 3)) :
            (d & 0x20) ? ((d >> 2) & 1) : ((d & 1) | ((d >> 1) & 2));
        return ((j -> chr_low[index] | (j -> chr_high[index] << 8)) & ((1u << bits) - 1)) | (outer << bits);
    }
    if (cartridge_mapper_number == 282) {
        // (bank numbers in the mode's units: 256 kB is 32 8 kB banks, 64 4 kB...)
        uint32_t d = j -> d003, size = (d & 0x20) ? (64u << chr_mode) : (32u << chr_mode);
        uint32_t outer = ((d & 0x10) ? ((d & 0x08) << (3 + chr_mode)) : 0) | ((d & 0x20) ? 0 : ((d & 1) << (5 + chr_mode)));
        return ((j -> chr_low[index] | (j -> chr_high[index] << 8)) & (size - 1)) | outer;
    }
    if (!(j -> d003 & 0x20)) {
        uint32_t block = ((j -> d003 & 0x18) >> 2) | (j -> d003 & 1);
        static const uint8_t masks[4] FRNES_COLD_RODATA = { 0x1f, 0x3f, 0x7f, 0xff };
        return (j -> chr_low[index] & masks[chr_mode]) | (block << (5 + chr_mode));
    }
    return j -> chr_low[index] | (j -> chr_high[index] << 8);
}

// the nametables (jy_apply's)
static void jy_nametables(void) {
    const jy_state_t* j = JY;
    if (((j -> mode & 0x20) || (cartridge_mapper_number == 211)) && (cartridge_mapper_number != 90)) {
        uint8_t vram[4];
        for (uint32_t i = 0; i < 4; i++) {
            vram[i] = j -> nt_low[i] & 1;
            if (num_1k_VROM_pages && ((j -> mode & 0x40) || ((j -> nt_low[i] ^ j -> d002) & 0x80))) {
                // (a copy of the CHR ROM page in VRAM page 4 + i, made again only when the slot's page changes;
                // set_ppu_nametable_rom from here would change the link time optimized memory.c's code)
                uint16_t page = (uint16_t)page_wrap(j -> nt_low[i] | (j -> nt_high[i] << 8), num_1k_VROM_pages);
                if (!(JY_NT -> valid & (1 << i)) || (JY_NT -> page[i] != page)) {
                    uint8_t* copy = &VRAM[(4 + i) << 10];
                    memcpy(copy, VROM_pages[page], 0x400);
                    dcache_wback_range((uintptr_t)copy, 0x400);
                    JY_NT -> page[i] = page;
                    JY_NT -> valid |= 1 << i;
                }
                vram[i] = 4 + i;
            }
        }
        set_ppu_nametable_pages(vram[0], vram[1], vram[2], vram[3]);
    } else {
        static const uint8_t mirrorings[4] FRNES_COLD_RODATA = { NAMETABLE_MIRRORING_VERTICAL, NAMETABLE_MIRRORING_HORIZONTAL,
            NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW, NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH };
        set_ppu_nametable_mirroring(mirrorings[j -> mirroring & 3]);
    }
}

static void jy_apply(void) {
    const jy_state_t* j = JY;
    uint32_t r[4];
    for (uint32_t i = 0; i < 4; i++) {
        r[i] = jy_prg_bits(j -> prg[i]);
    }
    uint32_t at_6000;
    switch (j -> mode & 3) {
        case 0:
            for (uint32_t i = 0; i < 4; i++) {
                set_6502_bank_pointer(i, jy_prg_page(((j -> mode & 4) ? r[3] : 0x0f) * 4 + i));
            }
            at_6000 = r[3] * 4 + 3;
            break;
        case 1:
            set_6502_bank_pointer(0, jy_prg_page(r[1] * 2));
            set_6502_bank_pointer(1, jy_prg_page(r[1] * 2 + 1));
            // (the upper 16 kB from register 3 as an 8 kB bank number, its pair after it)
            set_6502_bank_pointer(2, jy_prg_page((j -> mode & 4) ? r[3] : 0x3e));
            set_6502_bank_pointer(3, jy_prg_page(((j -> mode & 4) ? r[3] : 0x3e) + 1));
            at_6000 = r[3] * 2 + 1;
            break;
        default:
            for (uint32_t i = 0; i < 3; i++) {
                set_6502_bank_pointer(i, jy_prg_page(r[i]));
            }
            set_6502_bank_pointer(3, jy_prg_page((j -> mode & 4) ? r[3] : 0x3f));
            at_6000 = r[3];
            break;
    }
    if (j -> mode & 0x80) {
        mapper_6000_mode = MAPPER_6000_ROM;
        set_wram_bank_pointer(jy_prg_page(at_6000));
    } else {
        mapper_6000_mode = 0;
        if (WRAM != NULL) {
            set_wram_bank_pointer(WRAM);
        }
    }
    switch ((j -> mode >> 3) & 3) {
        case 0:
            chr_8k(jy_chr_reg(0));
            break;
        case 1:
            chr_4k(0, jy_chr_reg(0));
            chr_4k(1, jy_chr_reg(4));
            break;
        case 2:
            for (uint32_t i = 0; i < 4; i++) {
                chr_2k(i * 2, jy_chr_reg(i * 2) * 2);
            }
            break;
        case 3:
            for (uint32_t i = 0; i < 8; i++) {
                chr_1k(i, jy_chr_reg(i));
            }
            break;
    }
    jy_nametables();
}

// the IRQ: the source (bits 0-1), the direction (bits 6-7: 1 up, 2 down), the prescaler's size (bit 2: 3 bits)
static uint32_t jy_direction(void) {
    return (JY -> irq_mode >> 6) & 3;
}

static uint32_t jy_cycle_source(void) {
    return ((JY -> irq_mode & 3) == 0) && ((jy_direction() == 1) || (jy_direction() == 2));
}

static uint32_t jy_line_source(void) {
    uint32_t source = JY -> irq_mode & 3;
    return JY -> irq_enabled && ((source == 1) || (source == 2)) && ((jy_direction() == 1) || (jy_direction() == 2));
}

static uint32_t jy_prescaler_mask(void) {
    return (JY -> irq_mode & 0x04) ? 0x07 : 0xff;
}

// ticks to the prescaler's next counter clock
static uint32_t jy_first_clock(void) {
    uint32_t mask = jy_prescaler_mask(), p = JY -> prescaler & mask;
    if (jy_direction() == 1) {
        return mask + 1 - p;
    }
    return p ? p : mask + 1;
}

// the counter clocks to its wrap (the IRQ)
static uint32_t jy_clocks_to_wrap(void) {
    uint32_t c = JY -> counter;
    return (jy_direction() == 1) ? (c ? 256 - c : 256) : c + 1;
}

// 'ticks' ticks of the prescaler: 1 when the counter wrapped
static uint32_t jy_ticks(uint32_t ticks) {
    jy_state_t* j = JY;
    uint32_t mask = jy_prescaler_mask(), period = mask + 1, first = jy_first_clock(), up = jy_direction() == 1;
    uint32_t clocks = (ticks >= first) ? 1 + (ticks - first) / period : 0;
    uint32_t p = j -> prescaler & mask;
    p = up ? ((p + ticks) & mask) : ((p - ticks) & mask);
    j -> prescaler = (j -> prescaler & ~mask) | p;
    if (!clocks) {
        return 0;
    }
    uint32_t to_wrap = jy_clocks_to_wrap();
    j -> counter = up ? (uint8_t)(j -> counter + clocks) : (uint8_t)(j -> counter - clocks);
    return clocks >= to_wrap;
}

// a rendered line's A12 rises (or PPU reads), from the pattern tables PPUCTRL picks
static uint32_t jy_line_ticks(void) {
    if ((JY -> irq_mode & 3) == 2) {
        return 170;
    }
    // (the PPU's registers through cpu_registers, as VS_PPU_REGISTERS: ppu_registers from here would change the PPU's code)
    uint8_t ctrl = ((const _PPU_registers_t*)((const uint8_t*)cpu_registers + 32)) -> PPUCTRL;
    uint32_t sprites_high = (ctrl & PPUCTRL_SPRITE_SIZE) || (ctrl & 0x08);
    return ((ctrl & 0x10) ? 34 : 0) + (sprites_high ? 8 : 0);
}

// the line after (scanline, dot)'s clock point: lines 0-239 and the pre-render line, at dot 260
static uint32_t jy_next_line_after(uint32_t scanline, uint32_t dot) {
    if (scanline < 240) {
        return (dot < 260) ? scanline : ((scanline == 239) ? 261 : scanline + 1);
    }
    if (scanline < 261) {
        return 261;
    }
    return (dot < 260) ? 261 : 0;
}

// (the PPU's position from the cycles to its frame's last dot, ppu_dot_cpu_cycle's: ppu_current_scanline / _dot from
// here would take them out of line in the link time optimized code)
static void jy_line_start(void) {
    int32_t to_end = (int32_t)(ppu_dot_cpu_cycle(261, 340) - JY -> timer_base);
    int32_t position = 262 * 341 - 1 - ((to_end > 0) ? to_end * 3 : 0);
    if (position < 0) {
        position = 0;
    }
    JY -> next_line = jy_next_line_after((uint32_t)position / 341, (uint32_t)position % 341);
    JY -> next_line_set = 1;
}

// the board timer's time (mapper.c's mapper_timer_cycle: set by counter_timer_start, advanced by board_cpu_run)
static void jy_cpu_run(uint32_t cycles) {
    jy_state_t* j = JY;
    j -> timer_base += cycles;
    if (jy_cycle_source()) {
        if (jy_ticks(cycles) && j -> irq_enabled) {
            mapper_irq_flag |= CPU_IRQ_MAPPER;
            cpu_irq_update_line();
        }
        return;
    }
    if (!jy_line_source()) {
        return;
    }
    if (!j -> next_line_set) {
        jy_line_start();
    }
    for (uint32_t n = 0; n < 300; n++) {
        uint32_t line = j -> next_line;
        uint32_t at = ppu_dot_cpu_cycle(line, 260);
        if ((int32_t)(at - j -> timer_base) > 0) {
            break;
        }
        if ((((const _PPU_registers_t*)((const uint8_t*)cpu_registers + 32)) -> PPUMASK & 0x18) && jy_ticks(jy_line_ticks())) {
            cpu_irq_raise_at(CPU_IRQ_MAPPER, at);
        }
        j -> next_line = (line == 239) ? 261 : ((line == 261) ? 0 : line + 1);
    }
}

static uint32_t jy_cycles_to_fire(void) {
    const jy_state_t* j = JY;
    if (jy_cycle_source()) {
        if (!j -> irq_enabled) {
            return 0xffffffff;
        }
        return jy_first_clock() + (jy_clocks_to_wrap() - 1) * (jy_prescaler_mask() + 1);
    }
    if (!jy_line_source()) {
        return 0xffffffff;
    }
    if (!j -> next_line_set) {
        jy_line_start();
    }
    int32_t cycles = (int32_t)(ppu_dot_cpu_cycle(j -> next_line, 260) - j -> timer_base);
    return (cycles > 0) ? (uint32_t)cycles : 1;
}

static void jy_irq_changed(void) {
    jy_state_t* j = JY;
    if (!mapper_cpu_timer) {
        // (counter_timer_start's time: the timer's base from here)
        j -> timer_base = cpu_cycle_now;
    }
    counter_timer_start();
    j -> next_line_set = 0;
    mapper_cpu_timer = jy_cycle_source() || jy_line_source();
    cpu_cut_batch_for_event(mapper_cycles_to_event());
}

static void jy_write(uint16_t address, uint8_t data) {
    jy_state_t* j = JY;
    if (((address & 0xf000) == 0xc000) && mapper_cpu_timer) {
        mapper_cpu_run(cpu_cycle_now);
    }
    switch (address & 0xf007) {
        case 0x8000: case 0x8001: case 0x8002: case 0x8003: case 0x8004: case 0x8005: case 0x8006: case 0x8007:
            j -> prg[address & 3] = data & 0x7f;
            break;
        case 0x9000: case 0x9001: case 0x9002: case 0x9003: case 0x9004: case 0x9005: case 0x9006: case 0x9007:
            j -> chr_low[address & 7] = data;
            break;
        case 0xa000: case 0xa001: case 0xa002: case 0xa003: case 0xa004: case 0xa005: case 0xa006: case 0xa007:
            j -> chr_high[address & 7] = data;
            break;
        case 0xb000: case 0xb001: case 0xb002: case 0xb003:
            j -> nt_low[address & 3] = data;
            break;
        case 0xb004: case 0xb005: case 0xb006: case 0xb007:
            j -> nt_high[address & 3] = data;
            break;
        case 0xc000: case 0xc002: case 0xc003:
            j -> irq_enabled = ((address & 7) == 3) || (((address & 7) == 0) && (data & 1));
            if (!j -> irq_enabled) {
                cpu_irq_clear(CPU_IRQ_MAPPER);
            }
            jy_irq_changed();
            return;
        case 0xc001:
            j -> irq_mode = data;
            jy_irq_changed();
            return;
        case 0xc004:
            j -> prescaler = data ^ j -> xor_reg;
            jy_irq_changed();
            return;
        case 0xc005:
            j -> counter = data ^ j -> xor_reg;
            jy_irq_changed();
            return;
        case 0xc006:
            j -> xor_reg = data;
            return;
        case 0xd000:
            j -> mode = data;
            break;
        case 0xd001:
            j -> mirroring = data & 3;
            break;
        case 0xd002:
            j -> d002 = data & 0x80;
            break;
        case 0xd003:
            j -> d003 = data;
            break;
        default:
            return;
    }
    ppu_sync();
    jy_apply();
}

static void jy_write_low(uint16_t address, uint8_t data) {
    switch (address & 0xf803) {
        case 0x5800: JY -> mult1 = data; break;
        case 0x5801: JY -> mult2 = data; break;
        case 0x5803: JY -> ram = data; break;
    }
}

static uint8_t jy_read_low(uint16_t address, uint8_t open_bus) {
    const jy_state_t* j = JY;
    switch (address & 0xf803) {
        case 0x5000: return 0;
        case 0x5800: return (uint8_t)(j -> mult1 * j -> mult2);
        case 0x5801: return (uint8_t)((j -> mult1 * j -> mult2) >> 8);
        case 0x5803: return j -> ram;
    }
    return open_bus;
}

__attribute__((cold, noinline)) static void jy_init() {
    memset(JY, 0, sizeof(jy_state_t));
    jy_apply();
}

/**
 * Cony / Yoko's ASIC, mapper 83 (Fatal Fury 2, Street Fighter II Pro, World Heroes 2, the Dragon Ball Party multicart;
 * the nesdev wiki's registers, Mesen 2's Mapper83 to compare), a cold board. $8000-$FFFF by address: & $8300 $8000
 * the PRG base, $8100 the mode; & $8301 $8200 / $8201 the IRQ counter's low byte (the IRQ acknowledged) / high byte
 * (the counter on if the mode's bit 7 is set); & $8318 $8300-$8303 the 8 kB PRG registers ($8000 / $A000 / $C000 /
 * $6000), $8310-$8317 the 1 kB CHR registers, $8318 the IRQ's source. The mode: bits 0-1 the mirroring (vertical,
 * horizontal, one screen low, high: Mesen 2's order), bits 3-4 the PRG mode (0 UxROM: the base's 16 kB at $8000, its
 * 256 kB block's last at $C000; 1 the base's 16 kB at both; 2-3 the registers, $E000 the block's last 8 kB), bit 5
 * PRG ROM at $6000 (register 3), bit 6 the counter's direction (set down), bit 7 the enable. The base: bits 0-3 PRG
 * A14-A17, 4-5 PRG A18-A19 (and CHR A18-A19); submapper 2 (Dragon Ball Party): bits 6-7 the 8 kB of its 32 kB PRG
 * RAM at $6000. Submapper 1: 2 kB CHR banks from registers 0, 1, 6, 7. $5000 reads the DIP switch (0), $5100-$5103 four
 * bytes of RAM. The IRQ counter counts CPU cycles (source 0, an event of the board timer at its end) or PPU A12 rises
 * (source 1: J.Y.'s line events at dot 260, the line's rises from PPUCTRL, only while the counter is on); at 0 it
 * stops and raises the IRQ. In the register page (+0x1c4-+0x1e3, zeroed at each load, saved with a state).
 * Mapper 264 (Yoko's NC-14 boards: Master Fighter VI', Mortal Kombat II+V Pro) is the same ASIC with CPU A11-A10 at its
 * A9-A8 (its registers $0400 apart), as Mesen 2's Yoko has it: 2 kB CHR banks from registers 0, 1, 6, 7; mode bit 4
 * the 8 kB registers (bits 0-3) with the base's bit 3 as PRG A17, $E000 that block's last; else bit 3 32 kB from the
 * base's bits 1-7; else the base's 16 kB and the last 16 kB; mode bit 0 the mirroring (set horizontal); the reset
 * button clears the mode and the base. Board 85 (BOARD_YOKO)
 */
typedef struct {
    uint8_t prg[4], chr[8], base, mode, source, ram[4], enabled, next_line_set, pad;
    uint16_t counter, next_line;
    uint32_t timer_base;
} cony_state_t;
#define CONY ((cony_state_t*)((uint8_t*)cpu_registers + 0x1c4))

static void cony_apply(void) {
    const cony_state_t* c = CONY;
    uint32_t outer16 = c -> base & 0x30, outer8 = outer16 << 1;
    if (board_number == BOARD_YOKO) {
        if (c -> mode & 0x10) {
            uint32_t outer = (c -> base & 0x08) << 1;
            for (uint32_t i = 0; i < 3; i++) {
                set_6502_bank_pointer(i, prg_page(outer | (c -> prg[i] & 0x0f)));
            }
            set_6502_bank_pointer(3, prg_page(outer | 0x0f));
        } else if (c -> mode & 0x08) {
            prg_32k(c -> base >> 1);
        } else {
            prg_16k(0, c -> base);
            prg_16k(2, num_8k_ROM_pages / 2 - 1);
        }
        for (uint32_t i = 0; i < 4; i++) {
            chr_2k(i * 2, c -> chr[(i < 2) ? i : i + 4] << 1);
        }
        set_ppu_nametable_mirroring((c -> mode & 1) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
        return;
    }
    switch ((c -> mode >> 3) & 3) {
        case 0:
            prg_16k(0, c -> base & 0x3f);
            prg_16k(2, outer16 | 0x0f);
            break;
        case 1:
            prg_16k(0, c -> base & 0x3f);
            prg_16k(2, c -> base & 0x3f);
            break;
        default:
            for (uint32_t i = 0; i < 3; i++) {
                set_6502_bank_pointer(i, prg_page(outer8 | (c -> prg[i] & 0x1f)));
            }
            set_6502_bank_pointer(3, prg_page(outer8 | 0x1f));
            break;
    }
    if ((board_submapper == 2) && (WRAM != NULL)) {
        mapper_6000_mode = 0;
        set_wram_bank_pointer(WRAM + ((c -> base >> 6) % (WRAM_size / 0x2000)) * 0x2000);
    } else if (c -> mode & 0x20) {
        mapper_6000_mode = MAPPER_6000_ROM;
        set_wram_bank_pointer(prg_page(outer8 | (c -> prg[3] & 0x1f)));
    } else {
        mapper_6000_mode = 0;
        if (WRAM != NULL) {
            set_wram_bank_pointer(WRAM);
        }
    }
    uint32_t chr_outer = (c -> base & 0x30) << 4;
    if ((board_submapper == 1) || (board_number == BOARD_YOKO)) {
        static const uint8_t regs[4] FRNES_COLD_RODATA = { 0, 1, 6, 7 };
        for (uint32_t i = 0; i < 4; i++) {
            chr_2k(i * 2, c -> chr[regs[i]] << 1);
        }
    } else {
        for (uint32_t i = 0; i < 8; i++) {
            chr_1k(i, chr_outer | c -> chr[i]);
        }
    }
    static const uint8_t mirrorings[4] FRNES_COLD_RODATA = { NAMETABLE_MIRRORING_VERTICAL, NAMETABLE_MIRRORING_HORIZONTAL,
        NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW, NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH };
    set_ppu_nametable_mirroring(mirrorings[c -> mode & 3]);
}

// clocks to the counter's 0 (a full turn from 0)
static uint32_t cony_clocks_to_zero(void) {
    const cony_state_t* c = CONY;
    if (c -> mode & 0x40) {
        return c -> counter ? c -> counter : 0x10000;
    }
    return 0x10000 - c -> counter;
}

// 'clocks' clocks of the counter: 1 when it reached 0 (it stops there)
static uint32_t cony_clock(uint32_t clocks) {
    cony_state_t* c = CONY;
    if (clocks >= cony_clocks_to_zero()) {
        c -> counter = 0;
        c -> enabled = 0;
        return 1;
    }
    c -> counter = (c -> mode & 0x40) ? (uint16_t)(c -> counter - clocks) : (uint16_t)(c -> counter + clocks);
    return 0;
}

static uint32_t cony_line_source(void) {
    return CONY -> enabled && (CONY -> source & 1);
}

// a rendered line's A12 rises, from the pattern tables PPUCTRL picks (as jy_line_ticks)
static uint32_t cony_line_ticks(void) {
    uint8_t ctrl = ((const _PPU_registers_t*)((const uint8_t*)cpu_registers + 32)) -> PPUCTRL;
    uint32_t sprites_high = (ctrl & PPUCTRL_SPRITE_SIZE) || (ctrl & 0x08);
    return ((ctrl & 0x10) ? 34 : 0) + (sprites_high ? 8 : 0);
}

static void cony_line_start(void) {
    int32_t to_end = (int32_t)(ppu_dot_cpu_cycle(261, 340) - CONY -> timer_base);
    int32_t position = 262 * 341 - 1 - ((to_end > 0) ? to_end * 3 : 0);
    if (position < 0) {
        position = 0;
    }
    CONY -> next_line = (uint16_t)jy_next_line_after((uint32_t)position / 341, (uint32_t)position % 341);
    CONY -> next_line_set = 1;
}

static void cony_cpu_run(uint32_t cycles) {
    cony_state_t* c = CONY;
    c -> timer_base += cycles;
    if (!c -> enabled) {
        mapper_cpu_timer = 0;
        return;
    }
    if (!(c -> source & 1)) {
        if (cony_clock(cycles)) {
            mapper_cpu_timer = 0;
            mapper_irq_flag |= CPU_IRQ_MAPPER;
            cpu_irq_update_line();
        }
        return;
    }
    if (!c -> next_line_set) {
        cony_line_start();
    }
    for (uint32_t n = 0; n < 300; n++) {
        uint32_t line = c -> next_line;
        uint32_t at = ppu_dot_cpu_cycle(line, 260);
        if ((int32_t)(at - c -> timer_base) > 0) {
            break;
        }
        if ((((const _PPU_registers_t*)((const uint8_t*)cpu_registers + 32)) -> PPUMASK & 0x18) && cony_clock(cony_line_ticks())) {
            cpu_irq_raise_at(CPU_IRQ_MAPPER, at);
            mapper_cpu_timer = 0;
            return;
        }
        c -> next_line = (line == 239) ? 261 : ((line == 261) ? 0 : line + 1);
    }
}

static uint32_t cony_cycles_to_fire(void) {
    cony_state_t* c = CONY;
    if (!c -> enabled) {
        return 0xffffffff;
    }
    if (!(c -> source & 1)) {
        return cony_clocks_to_zero();
    }
    if (!c -> next_line_set) {
        cony_line_start();
    }
    int32_t cycles = (int32_t)(ppu_dot_cpu_cycle(c -> next_line, 260) - c -> timer_base);
    return (cycles > 0) ? (uint32_t)cycles : 1;
}

static void cony_irq_changed(void) {
    cony_state_t* c = CONY;
    if (!mapper_cpu_timer) {
        c -> timer_base = cpu_cycle_now;
    }
    counter_timer_start();
    c -> next_line_set = 0;
    mapper_cpu_timer = c -> enabled;
    cpu_cut_batch_for_event(mapper_cycles_to_event());
}

static void cony_write(uint16_t address, uint8_t data) {
    cony_state_t* c = CONY;
    if (board_number == BOARD_YOKO) {
        address = (address & 0xf0ff) | ((address >> 2) & 0x0300);
    }
    if (mapper_cpu_timer) {
        mapper_cpu_run(cpu_cycle_now);
    }
    if ((address & 0x8318) == 0x8318) {
        c -> source = data;
        cony_irq_changed();
        return;
    }
    if ((address & 0x8318) == 0x8310) {
        c -> chr[address & 7] = data;
    } else if ((address & 0x8318) == 0x8300) {
        c -> prg[address & 3] = data;
    } else if ((address & 0x8301) == 0x8200) {
        c -> counter = (c -> counter & 0xff00) | data;
        cpu_irq_clear(CPU_IRQ_MAPPER);
        cony_irq_changed();
        return;
    } else if ((address & 0x8301) == 0x8201) {
        c -> counter = (uint16_t)((c -> counter & 0x00ff) | (data << 8));
        c -> enabled = (c -> mode & 0x80) != 0;
        cony_irq_changed();
        return;
    } else if ((address & 0x8300) == 0x8100) {
        c -> mode = data;
    } else if ((address & 0x8300) == 0x8000) {
        c -> base = data;
    } else {
        return;
    }
    ppu_sync();
    cony_apply();
}

static void cony_write_low(uint16_t address, uint8_t data) {
    if ((address & 0xff00) == 0x5100) {
        CONY -> ram[address & 3] = data;
    }
}

static uint8_t cony_read_low(uint16_t address, uint8_t open_bus) {
    if ((address & 0xff00) == 0x5100) {
        return CONY -> ram[address & 3];
    }
    if ((address & 0xff00) == 0x5000) {
        // (the DIP switch, bits 0-1: 0)
        return open_bus & 0xfc;
    }
    return open_bus;
}

__attribute__((cold, noinline)) static void cony_init(void) {
    memset(CONY, 0, sizeof(cony_state_t));
    if (board_submapper == 2) {
        resize_wram(0x8000);
    }
    cony_apply();
}

/**
 * Two 16 kB PRG boards (nesdev's INES Mapper 094 / 097):
 *   94 (UN1ROM: Senjou no Ookami, with bus conflicts): a write to $8000-$FFFF selects the bank at $8000 from bits 2-4,
 *      the last fixed at $C000; 8 kB of CHR RAM
 *   97 (Irem TAM-S1: Kaiketsu Yanchamaru): a write to $8000-$BFFF selects the bank at $C000 from bits 0-4, the last
 *      fixed at $8000, and bit 7 the mirroring (0 horizontal, 1 vertical); no bus conflicts
 */
static void un1rom94_write(uint16_t address, uint8_t data) {
    data &= NES_WINDOW_BYTE(address);
    prg_16k(0, (data >> 2) & 7);
}

static void tams1_97_write(uint16_t address, uint8_t data) {
    if (address >= 0xc000) {
        return;
    }
    prg_16k(2, data & 0x1f);
    ppu_sync();
    set_ppu_nametable_mirroring((data & 0x80) ? NAMETABLE_MIRRORING_VERTICAL : NAMETABLE_MIRRORING_HORIZONTAL);
}

__attribute__((cold, noinline)) static void prg16_boards_init() {
    uint32_t last = num_8k_ROM_pages / 2 - 1;
    if (board_number == 97) {
        prg_16k(0, last);
        prg_16k(2, 0);
    } else {
        prg_16k(0, 0);
        prg_16k(2, last);
    }
    chr_8k(0);
}

/**
 * Action 53 (mapper 28, INL's multicart board): a write to $5000-$5FFF selects a register (bit 7 and bit 0), writes to
 * $8000-$FFFF load it. 0: CHR RAM bank (bits 0-1; bit 4 one screen mirroring), 1: inner PRG bank (bits 0-3; bit 4
 * mirroring), 2: mode (bits 0-1 mirroring, bits 2-3 PRG mode, bits 4-5 game size), 3: outer PRG bank. As Mesen 2's
 */
static uint8_t a53_select, a53_regs[4], a53_mirroring;

static void a53_apply() {
    static const uint8_t mirrorings[4] = { NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW, NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH,
                                           NAMETABLE_MIRRORING_VERTICAL, NAMETABLE_MIRRORING_HORIZONTAL };
    static const uint16_t outer_and[4] = { 0x1fe, 0x1fc, 0x1f8, 0x1f0 };
    static const uint8_t inner_and[4] = { 0x01, 0x03, 0x07, 0x0f };
    uint32_t mirroring = a53_regs[2] & 3;
    if (!(mirroring & 2)) {
        mirroring = a53_mirroring;
    }
    uint32_t size = (a53_regs[2] >> 4) & 3, prg_16k_mode = (a53_regs[2] >> 3) & 1, slot = (a53_regs[2] >> 2) & 1;
    uint32_t inner = a53_regs[1] & 0x0f, outer = (uint32_t)a53_regs[3] << 1;
    ppu_sync();
    set_ppu_nametable_mirroring(mirrorings[mirroring]);
    chr_8k(a53_regs[0] & 3);
    if (prg_16k_mode) {
        prg_16k(slot ? 0 : 2, (outer & outer_and[size]) | (inner & inner_and[size]));
        prg_16k(slot ? 2 : 0, (outer & 0x1fe) | slot);
    } else {
        inner <<= 1;
        prg_16k(0, (outer & outer_and[size]) | (inner & inner_and[size]));
        prg_16k(2, (outer & outer_and[size]) | ((inner | 1) & inner_and[size]));
    }
}

static void a53_write(uint8_t data) {
    if (a53_select <= 1) {
        a53_mirroring = (data >> 4) & 1;
    } else if (a53_select == 2) {
        a53_mirroring = data & 1;
    }
    a53_regs[a53_select] = data;
    a53_apply();
}

__attribute__((cold, noinline)) static void a53_init() {
    a53_select = a53_mirroring = 0;
    memset(a53_regs, 0, sizeof(a53_regs));
    prg_16k(2, num_8k_ROM_pages / 2 - 1);
    chr_8k(0);
}

/**
 * Active Enterprises (mapper 228: Action 52, Cheetahmen II): a write to $8000-$FFFF selects with its address, A13 the
 * mirroring (horizontal when set), A12-A11 the 512 kB PRG chip (chip 3 is the file's third, as Mesen 2 maps it), A10-A6
 * the 16 kB PRG bank in it and A5 the mode (16 kB at both halves, else 32 kB), and the 8 kB CHR bank with A3-A0 above
 * the data's bits 1-0. $4020-$5FFF is 4 nibbles of RAM (A1-A0), the upper bits open bus
 */
static uint8_t action52_ram[4] FRNES_COLD_BSS;

static void action52_write(uint16_t address, uint8_t data) {
    uint32_t chip = (address >> 11) & 3;
    if (chip == 3) {
        chip = 2;
    }
    uint32_t page = ((address >> 6) & 0x1f) | (chip << 5);
    if (address & 0x20) {
        prg_16k(0, page);
        prg_16k(2, page);
    } else {
        prg_16k(0, page & ~1u);
        prg_16k(2, page | 1);
    }
    ppu_sync();
    chr_8k(((address & 0x0f) << 2) | (data & 3));
    set_ppu_nametable_mirroring((address & 0x2000) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
}

__attribute__((cold, noinline)) static void action52_init() {
    memset(action52_ram, 0, sizeof(action52_ram));
    action52_write(0x8000, 0);
}

/**
 * AVE NINA-03 / NINA-06 (mapper 79: Blackjack, Deathbots, Krazy Kreatures, Tiles of Fate): a write to $4100-$5FFF with
 * A8 set (A13-A15 clear: (address & 0xe100) == 0x4100) selects bit 3 the 32 kB PRG bank, bits 0-2 the 8 kB CHR bank
 * (Mesen 2's Nina03_06). Mapper 113 (HES NTD-8: the HES multicarts, Mind Blower Pak, the Sachen ones) the same register
 * with more bits, the nesdev wiki's: bits 3-5 the 32 kB PRG bank, bits 0-2 and 6 the 8 kB CHR bank, bit 7 the
 * mirroring (vertical when set)
 */
static void nina0306_write_low(uint16_t address, uint8_t data) {
    if (((address & 0xe100) == 0x4100) && (board_number == 113)) {
        ppu_sync();
        prg_32k((data >> 3) & 7);
        chr_8k((data & 7) | ((data >> 3) & 8));
        set_ppu_nametable_mirroring((data & 0x80) ? NAMETABLE_MIRRORING_VERTICAL : NAMETABLE_MIRRORING_HORIZONTAL);
    } else if ((address & 0xe100) == 0x4100) {
        prg_32k((data >> 3) & 1);
        if (!chr_8k_mapped(data & 7)) {
            ppu_sync();
            chr_8k(data & 7);
        }
    }
}

__attribute__((cold, noinline)) static void nina0306_init() {
    prg_32k(0);
    chr_8k(0);
}

/**
 * NTDEC's mapper 112 (Asder: Three Kingdoms - Battle of Red Cliffs, Huang Di, Fighting Hero III...; the nesdev wiki's
 * registers, Mesen 2's Mapper112 to compare): by address & $E001, $8000 the index (bits 0-2) and $A000 the value of
 * eight registers: 0 / 1 the 8 kB PRG banks at $8000 / $A000 ($C000-$FFFF the last 16 kB), 2 / 3 the 2 kB CHR banks
 * at $0000 / $0800, 4-7 the 1 kB CHR banks at $1000-$1C00; $C000 bits 4-7 CHR A18 for those four; $E000 bit 0 the
 * mirroring (set horizontal). In the register page (+0x1f0-+0x1fa: the registers, the index, $C000, $E000)
 */
#define NTDEC112 ((uint8_t*)cpu_registers + 0x1f0)
// (the 2 kB banks' values in 1 kB units, their low bit ignored, as the Namco 108's)
#define CHR112_2K(value) (value)

static void ntdec112_apply(void) {
    const uint8_t* r = NTDEC112;
    set_6502_bank_pointer(0, prg_page(r[0]));
    set_6502_bank_pointer(1, prg_page(r[1]));
    set_6502_bank_pointer(2, prg_page(num_8k_ROM_pages - 2));
    set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
    chr_2k(0, CHR112_2K(r[2]));
    chr_2k(2, CHR112_2K(r[3]));
    for (uint32_t i = 0; i < 4; i++) {
        chr_1k(4 + i, r[4 + i] | (((r[9] >> (4 + i)) & 1) << 8));
    }
    set_ppu_nametable_mirroring((r[10] & 1) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
}

static void ntdec112_write(uint16_t address, uint8_t data) {
    uint8_t* r = NTDEC112;
    switch (address & 0xe001) {
        case 0x8000: r[8] = data & 7; return;
        case 0xa000: r[r[8]] = data; break;
        case 0xc000: r[9] = data; break;
        case 0xe000: r[10] = data; break;
        default: return;
    }
    ppu_sync();
    ntdec112_apply();
}

__attribute__((cold, noinline)) static void ntdec112_init(void) {
    memset(NTDEC112, 0, 11);
    ntdec112_apply();
}

/**
 * TXC's 05-00002-010 ASIC, mapper 132 (UNL-22211: Mahjong Block, Rad Racket, Creatom, Qi Wang; the nesdev wiki's
 * registers, Mesen 2's TxcChip to compare): $4100-$4103 by address & $E103, the low 4 bits written: $4100 the
 * accumulator from the staging bits (inverted while the invert bit is set) or, in increment mode, one more; $4101
 * bit 0 the invert bit; $4102 bits 0-2 the staging bits, bit 3 a bit read back; $4103 bit 0 increment mode. A write
 * to $8000-$FFFF latches the output (the accumulator's bits 0-2, the staged bit 3 as bit 4): bit 2 the 32 kB PRG bank,
 * bits 0-1 the 8 kB CHR bank. $4100 (& $E103) reads give bits 0-2 the accumulator's, bit 3 the staged bit (inverted
 * with the invert bit), bits 4-7 open bus. In the register page (+0x1fb-+0x1ff: the accumulator, the staged high
 * bits, the staging bits, the output, the modes: bit 0 increment, bit 1 invert, bit 2 the Y flag). The same chip:
 *   36 (TXC 01-22000-200: Strike Wolf, Policeman, Mi Hun Che; Mesen 2's Txc22000 to compare): the chip takes the
 *     value's bits 4-5, its output's bits 0-1 the 32 kB PRG bank; $4200 (& $F200) the 8 kB CHR bank (+0x205); $4100
 *     reads give the chip's bits 0-1 as bits 4-5
 *   173 (TXC 01-22211-x: Mahjong Block (Idea-Tek), 小瑪琍; Mesen 2's Txc22211C to compare): PRG fixed; the CHR bank
 *     the output's bit 0, the Y flag (not inverting, or the written value's bit 4) and the output's bit 1 (one 8 kB
 *     CHR ROM: always mapped, its disable by Y not emulated)
 *   136 (Sachen 3011: Mahjong Trap, Jovial Race) and 172 (TXC 22211B: Mahjong Block (Super Mega), 1991 Du Ma Racing;
 *     Mesen 2's Sachen_136 / Txc22211B to compare): the chip's JV001 form (4 staging bits, the staged high nibble as
 *     the output's, invert on at power on); 136 the value's bits 0-5 and $4100 reads' bits 0-5, 172 those bits
 *     reversed both ways and the mirroring vertical while inverting; PRG fixed, the output the 8 kB CHR bank
 */
#define TXC132 ((uint8_t*)cpu_registers + 0x1fb)
#define TXC36_CHR (*((uint8_t*)cpu_registers + 0x205))

// the JV001 form (136, 172)
static uint32_t txc132_jv001(void) {
    return (board_number == 136) || (board_number == 172);
}

// 172's data lines: bits 0-5 reversed
static uint8_t txc172_reverse(uint8_t v) {
    return (uint8_t)(((v & 1) << 5) | ((v & 2) << 3) | ((v & 4) << 1) | ((v & 8) >> 1) | ((v & 0x10) >> 3) | ((v & 0x20) >> 5));
}

static void txc132_apply(void) {
    uint8_t output = TXC132[3];
    if (txc132_jv001()) {
        prg_32k(0);
        chr_8k(output);
        if (board_number == 172) {
            set_ppu_nametable_mirroring((TXC132[4] & 2) ? NAMETABLE_MIRRORING_VERTICAL : NAMETABLE_MIRRORING_HORIZONTAL);
        }
        return;
    }
    if (board_number == 36) {
        prg_32k(output & 3);
        chr_8k(TXC36_CHR);
        return;
    }
    if (board_number == 173) {
        prg_32k(0);
        chr_8k((output & 1) | ((TXC132[4] & 4) >> 1) | ((output & 2) << 1));
        return;
    }
    prg_32k((output >> 2) & 1);
    chr_8k(output & 3);
}

// (173's Y flag after a write or read: not inverting, or the value's bit 4)
static void txc132_y(uint8_t value) {
    uint8_t* t = TXC132;
    t[4] = (t[4] & ~4) | (((!(t[4] & 2)) || (value & 0x10)) ? 4 : 0);
}

static void txc132_write(uint16_t address, uint8_t data) {
    uint8_t* t = TXC132;
    if (board_number == 36) {
        if ((address & 0xf200) == 0x4200) {
            TXC36_CHR = data;
            ppu_sync();
            txc132_apply();
        }
        data = (data >> 4) & 3;
    }
    uint32_t jv001 = txc132_jv001(), mask = jv001 ? 0x0f : 0x07;
    // (the chip sees the value's low 4 bits, JV001's 6: a write's Y is the invert bit's)
    data = (board_number == 172) ? txc172_reverse(data) : (data & (jv001 ? 0x3f : 0x0f));
    uint8_t value = data;
    if (address >= 0x8000) {
        t[3] = jv001 ? ((t[0] & 0x0f) | (t[1] & 0xf0)) : ((t[0] & 0x0f) | ((t[1] & 0x08) << 1));
        txc132_y(value);
        ppu_sync();
        txc132_apply();
        return;
    }
    switch (address & 0xe103) {
        case 0x4100:
            if (t[4] & 1) {
                t[0]++;
            } else {
                t[0] = (t[0] & ~mask) | (t[2] & mask);
                if (t[4] & 2) {
                    t[0] = ~t[0];
                }
            }
            break;
        case 0x4101: t[4] = (t[4] & ~2) | ((data & 1) << 1); break;
        case 0x4102: t[2] = data & mask; t[1] = data & ~mask; break;
        case 0x4103: t[4] = (t[4] & ~1) | (data & 1); break;
    }
    txc132_y(value);
    if ((board_number == 173) || jv001) {
        ppu_sync();
        txc132_apply();
    }
}

static uint8_t txc132_read(uint16_t address, uint8_t open_bus) {
    uint8_t* t = TXC132;
    if ((address & 0xe103) != 0x4100) {
        return open_bus;
    }
    uint32_t mask = txc132_jv001() ? 0x0f : 0x07;
    uint8_t value = (t[0] & mask) | ((t[1] ^ ((t[4] & 2) ? 0xff : 0)) & ~mask);
    if (board_number == 173) {
        txc132_y(value);
        txc132_apply();
    }
    if (board_number == 36) {
        return (open_bus & 0xcf) | ((value << 4) & 0x30);
    }
    if (board_number == 136) {
        return (open_bus & 0xc0) | (value & 0x3f);
    }
    if (board_number == 172) {
        return (open_bus & 0xc0) | txc172_reverse(value);
    }
    return (open_bus & 0xf0) | (value & 0x0f);
}

__attribute__((cold, noinline)) static void txc132_init(void) {
    memset(TXC132, 0, 5);
    TXC36_CHR = 0;
    if (txc132_jv001()) {
        TXC132[4] = 2;
    }
    txc132_apply();
}

/**
 * Daou Infosys's mapper 156 (Metal Force, Buzz & Waldog, Koko Adventure; the nesdev wiki's registers, Mesen 2's
 * DaouInfosys to compare): $C000-$C014 by address: the 1 kB CHR banks' low bytes at $C000-$C003 (slots 0-3) and
 * $C008-$C00B (4-7), their high bytes 4 above; $C010 the 16 kB PRG bank at $8000 (the last at $C000); $C014 bit 0 the
 * mirroring (horizontal when set; one screen low at power on). In the register page (+0x206-+0x216)
 */
#define DAOU156 ((uint8_t*)cpu_registers + 0x206)

static void daou156_chr(void) {
    const uint8_t* r = DAOU156;
    for (uint32_t slot = 0; slot < 8; slot++) {
        uint32_t low = (slot & 3) + ((slot >> 2) << 3);
        chr_1k(slot, r[low] | (r[low + 4] << 8));
    }
}

static void daou156_write(uint16_t address, uint8_t data) {
    uint8_t* r = DAOU156;
    if ((address >= 0xc000) && (address < 0xc010)) {
        r[address & 0x0f] = data;
        ppu_sync();
        daou156_chr();
    } else if (address == 0xc010) {
        r[16] = data;
        prg_16k(0, data);
    } else if (address == 0xc014) {
        ppu_sync();
        set_ppu_nametable_mirroring((data & 1) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
    }
}

__attribute__((cold, noinline)) static void daou156_init(void) {
    memset(DAOU156, 0, 17);
    prg_16k(0, 0);
    prg_16k(2, num_8k_ROM_pages / 2 - 1);
    daou156_chr();
    set_ppu_nametable_mirroring(NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW);
}

/**
 * AGCI 50282 (mapper 144: Death Race): Color Dreams (mapper 11, PRG 32 kB bank bits 0-1, CHR 8 kB bank bits 4-7) with
 * a resistor on the CPU's D0, so in its bus conflict the ROM's bit 0 always wins: the latch takes
 * ROM[address] & (data | 1) (nesdev's INES Mapper 144)
 */
static void agci144_write(uint16_t address, uint8_t data) {
    uint8_t value = NES_WINDOW_BYTE(address) & (data | 1);
    prg_32k(value & 3);
    // (a write that maps the CHR bank already there changes nothing the PPU sees: no catch-up, as mapper 11's)
    for (uint32_t i = 0; i < 8; i++) {
        if (ppu_pattern_table_bank_pointer(i) != chr_page((value >> 4) * 8 + i)) {
            ppu_sync();
            chr_8k(value >> 4);
            break;
        }
    }
}

/**
 * CNROM with a copy protection diode (mapper 185: B-Wings, Bird Week, Mighty Bomb Jack (J), Seicross, Spy vs Spy (J)):
 * the CHR ROM only answers the written value it is wired for, otherwise the PPU reads garbage, which the games check
 * at boot. Submappers 4-7 give that value (bits 0-1 equal to the submapper's); submapper 0 as Mesen 2: enabled when
 * bits 0-3 aren't 0 and the value isn't $13. Disabled, a $2007 read gets the low byte of the address after it (the PPU's
 * multiplexed bus, as Mesen 2: Mighty Bomb Jack (J) waits for a nonzero read at $0000): the pattern tables point at a
 * buffer of those, allocated once (no static data)
 */
// the board's allocated state, for the boards that keep some (185's disabled CHR, 47 / 37's page tables): on the heap,
// so new boards add no .bss (its size moves the heap and the ROM pages on it onto other cache sets)
static void* board_state FRNES_COLD_BSS;

// (a new block at each board's set up: the previous ROM's board may have been another one)
static void* board_state_alloc(size_t size) {
    free(board_state);
    board_state = aligned_alloc(0x400, (size + 0x3ff) & ~0x3ffu);
    return board_state;
}

// (board_state for the code before it: 236's)
static void* board_buffer(void) {
    return board_state;
}

static void* board_buffer_alloc(size_t size, size_t align) {
    free(board_state);
    board_state = aligned_alloc(align, (size + align - 1) & ~(align - 1));
    return board_state;
}

/**
 * Front Fareast's copier boards (the Magic Card dumps; Mesen 2's FrontFareast): registers at $42FE-$4517. $42FE bit 4
 * the one-screen nametable (bit 7 clear: the "alternative" mode), $42FF bit 4 the mirroring (set for horizontal);
 * $4501 stops the IRQ counter, $4502 / $4503 load its low / high byte ($4503 starts it): it counts CPU cycles up and
 * raises the IRQ when it wraps to 0.
 *   6 (FFE F4): writes to $8000-$FFFF select the 8 kB CHR bank (bits 0-1, the 16 kB PRG bank at $8000 from bits 2-7,
 *     with CHR RAM or in the alternative mode); $C000 its bank 7
 *   8 (FFE F3): writes to $8000-$FFFF select the 16 kB PRG bank at $8000 (bits 3-7) and the 8 kB CHR bank (bits 0-2)
 *   17 (FFE F8): $4504-$4507 the 8 kB PRG banks, $4510-$4517 the 1 kB CHR banks
 * Most dumps have a trainer, loaded at $7000 (the copier's): load_nes_file skips it, so cartridge_convert keeps it in
 * board_state for ffe_init. State in the register page (+0xcc: the counter, its on flag, the mode; saved with a state)
 */
typedef struct {
    uint16_t counter;
    uint8_t enabled, alternative;
} ffe_state_t;
#define FFE ((ffe_state_t*)((uint8_t*)cpu_registers + 0xcc))
#define FFE_TRAINER_SIZE 512

static void ffe_write_low(uint16_t address, uint8_t data) {
    ffe_state_t* f = FFE;
    switch (address) {
        case 0x42fe:
            f -> alternative = !(data & 0x80);
            ppu_sync();
            set_ppu_nametable_mirroring((data & 0x10) ? NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH : NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW);
            return;
        case 0x42ff:
            ppu_sync();
            set_ppu_nametable_mirroring((data & 0x10) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
            return;
        case 0x4501: case 0x4502: case 0x4503:
            if (mapper_cpu_timer) {
                mapper_cpu_run(cpu_cycle_now);
            }
            counter_timer_start();
            if (address == 0x4501) {
                f -> enabled = 0;
            } else if (address == 0x4502) {
                f -> counter = (f -> counter & 0xff00) | data;
            } else {
                f -> counter = (f -> counter & 0x00ff) | (data << 8);
                f -> enabled = 1;
            }
            cpu_irq_clear(CPU_IRQ_MAPPER);
            mapper_cpu_timer = f -> enabled;
            cpu_cut_batch_for_event(mapper_cycles_to_event());
            return;
    }
    if (board_number != 17) {
        return;
    }
    if ((address >= 0x4504) && (address <= 0x4507)) {
        set_6502_bank_pointer(address - 0x4504, prg_page(data));
    } else if ((address >= 0x4510) && (address <= 0x4517) && !chr_1k_mapped(address - 0x4510, data)) {
        ppu_sync();
        chr_1k(address - 0x4510, data);
    }
}

static void ffe_write(uint16_t address, uint8_t data) {
    (void)address;
    uint32_t chr = data;
    if (board_number == 8) {
        prg_16k(0, data >> 3);
        chr = data & 7;
    } else if (!num_1k_VROM_pages || FFE -> alternative) {
        prg_16k(0, data >> 2);
        chr = data & 3;
    }
    if (!chr_8k_mapped(chr)) {
        ppu_sync();
        chr_8k(chr);
    }
}

static void ffe_cpu_run(uint32_t cycles) {
    ffe_state_t* f = FFE;
    if (!f -> enabled) {
        return;
    }
    uint32_t to_wrap = 0x10000 - f -> counter;
    if (cycles >= to_wrap) {
        f -> counter = 0;
        f -> enabled = 0;
        mapper_cpu_timer = 0;
        mapper_irq_flag |= CPU_IRQ_MAPPER;
        cpu_irq_update_line();
    } else {
        f -> counter += cycles;
    }
}

// a trainer in the file (cartridge_convert, before load_nes_file skips it): kept for ffe_init
static void ffe_keep_trainer(const uint8_t* data, uint32_t size) {
    if ((size >= 16 + FFE_TRAINER_SIZE) && !memcmp(data, "NES\x1a", 4) && (data[6] & 4)) {
        memcpy(board_state_alloc(FFE_TRAINER_SIZE), data + 16, FFE_TRAINER_SIZE);
    } else {
        free(board_state);
        board_state = NULL;
    }
}

static void ffe_load_trainer(void) {
    if (board_state && (NesHeader.byInfo1 & 4) && WRAM && (WRAM_size >= 0x2000)) {
        // (through the CPU's window, as its writes land: $6000-$7FFF's first 8 kB of WRAM)
        for (uint32_t i = 0; i < FFE_TRAINER_SIZE; i++) {
            NES_WINDOW_BYTE(0x7000 + i) = ((const uint8_t*)board_state)[i];
        }
        printf(" ...the trainer at $7000\n");
    }
}

__attribute__((cold, noinline)) static void ffe_init() {
    memset(FFE, 0, sizeof(ffe_state_t));
    FFE -> alternative = 1;
    ffe_load_trainer();
    if (board_number == 17) {
        for (uint32_t i = 0; i < 4; i++) {
            set_6502_bank_pointer(i, prg_page(num_8k_ROM_pages - 4 + i));
        }
    } else if (board_number == 8) {
        prg_32k(0);
    } else {
        prg_16k(0, 0);
        prg_16k(2, 7);
    }
    chr_8k(0);
}

/**
 * Mapper 42 (the FDS conversions' boards: Mario Baby (Bio Miracle Bokutte Upa), 愛戦士 Nicol (KS-7050), Green Beret
 * (LH09); the nesdev wiki's, Mesen 2's Mapper42 to compare): the last 32 kB at $8000, writes decoded with $E003:
 * $8000 bits 0-3 the 8 kB CHR bank (CHR ROM only), $E000 bits 0-3 the 8 kB PRG bank at $6000, $E001 bit 3 the
 * mirroring (horizontal when set), $E002 the IRQ counter: $02 starts it, anything else stops it, clears it and drops
 * the IRQ. The counter (15 bits) counts CPU cycles, the IRQ up while it is $6000-$7FFF (8192 of every 32768 cycles).
 * State in the register page (+0x220: the counter, its on flag, the PRG bank; saved with a state)
 * Mapper 40 (NTDEC 2722: the SMB2J conversions, 1990 Super Mario Bros. 4; the nesdev wiki's, Mesen 2's Mapper40 to
 * compare) on the same counter: 8 kB banks 6 at $6000, 4 / 5 at $8000 / $A000, 7 at $E000, writes decoded with $E000:
 * $E000 the 8 kB bank at $C000, $8000 stops the counter (cleared, the IRQ dropped), $A000 starts it; a CD4020, its 13
 * bits counting CPU cycles, the IRQ up while bit 12 is set (4096 cycles after the start, for 4096; the wiki's: Mesen 2
 * keeps it up until $8000)
 * Mapper 43 (the SMB2J conversions: 1990 Super Mario Bros. 2 (TONY-I)...; the nesdev wiki's, Mesen 2's Mapper43 to
 * compare), writes decoded with $F1FF: $4022 bits 0-2 the 8 kB bank at $C000 (4 / 5 / 6 / 7 for 0 / 2 / 4 / 6, 3 for
 * the odd ones), $4120 bit 0 the swap ($6000 bank 0 and $E000 8, else 2 and 9), $4122 / $8122 bit 0 the counter on
 * (each write clears it and drops the IRQ): 4096 CPU cycles, then the IRQ and the counter off. Banks 1 / 0 at $8000 /
 * $A000, bank 8's first 4 kB at $5000-$5FFF (board_read_low). The counter in 42's (+0x220), its swap and bank after it
 * Mapper 304 (Whirlwind's SMB2J board; Mesen 2's Smb2j to compare) on the same counter as 43: $4122 bits 0-1 the
 * counter on (each write clears it and drops the IRQ; 4096 CPU cycles, then the IRQ and the counter off), $4022 bit 0
 * (64 kB or more of PRG) the 16 kB banks at $8000 / $C000 (0 / 1 or 1 / 2); the PRG's last 8 kB at $6000 and the 4 kB
 * before it at $5000-$5FFF (board_read_low): of the ROM's data (40 kB in an exponent-multiplier size, which
 * nes2_exponent_cartridge pads to 48 kB with $FF), its last 8 kB page kept in the register page (+0x2b0)
 * Mapper 368 (the Yung-08 SMB2J conversion, N-32's parent; the nesdev wiki's) on the same counter as 43: $4022 (decoded
 * with $F1FF) bits 0-2 the 8 kB bank at $C000 by 43's table; $4122 bit 0 the counter on (0 stops and clears it and drops
 * the IRQ), bits 5, 4, 2 and 0 a latch $4122 reads back ($8A with them); banks 2 at $6000, 1 / 0 at $8000 / $A000, 8
 * (the 2 kB chip) at $E000. The latch in 43's swap byte
 * Mapper 50 (the N-32 SMB2J conversion; Mesen 2's Mapper50 to compare) on the same counter as 43: $4020-$5FFF decoded
 * with $4120: $4020 bits 0-3 the 8 kB bank at $C000 (bit 3, then bit 0, then bits 1-2), $4120 bit 0 the counter on (0
 * stops and clears it and drops the IRQ; 4096 CPU cycles, then the IRQ and the counter off); banks 15 at $6000, 8 / 9
 * at $8000 / $A000, 11 at $E000
 */
#define M304_LAST (*((uint8_t*)cpu_registers + 0x2b0))
typedef struct {
    uint16_t counter;
    uint8_t enabled, prg;
} m42_state_t;
#define M42 ((m42_state_t*)((uint8_t*)cpu_registers + 0x220))

static void m42_cpu_run(uint32_t cycles) {
    m42_state_t* s = M42;
    if (!s -> enabled) {
        return;
    }
    if ((board_number == 43) || (board_number == 304) || (board_number == 50) || (board_number == 368)) {
        // (a count to 4096, then the IRQ and the counter off)
        if (cycles >= 0x1000u - s -> counter) {
            s -> counter = 0;
            s -> enabled = 0;
            mapper_cpu_timer = 0;
            mapper_irq_flag |= CPU_IRQ_MAPPER;
            cpu_irq_update_line();
        } else {
            s -> counter += cycles;
        }
        return;
    }
    uint32_t counter = (s -> counter + cycles) & ((board_number == 40) ? 0x1fff : 0x7fff);
    s -> counter = counter;
    if (counter >= ((board_number == 40) ? 0x1000u : 0x6000u)) {
        mapper_irq_flag |= CPU_IRQ_MAPPER;
        cpu_irq_update_line();
    } else {
        cpu_irq_clear(CPU_IRQ_MAPPER);
    }
}

// the cycles to the IRQ's next change (the counter reaching $6000, or wrapping)
static uint32_t m42_cycles_to_fire(void) {
    if ((board_number == 43) || (board_number == 304) || (board_number == 50) || (board_number == 368)) {
        return 0x1000 - M42 -> counter;
    }
    uint32_t counter = M42 -> counter, high = (board_number == 40) ? 0x1000 : 0x6000;
    return ((counter < high) ? high : ((board_number == 40) ? 0x2000 : 0x8000)) - counter;
}

static void m42_counter(int enabled) {
    m42_state_t* s = M42;
    if (mapper_cpu_timer) {
        mapper_cpu_run(cpu_cycle_now);
    }
    counter_timer_start();
    s -> enabled = enabled;
    if (!enabled) {
        s -> counter = 0;
        cpu_irq_clear(CPU_IRQ_MAPPER);
    }
    mapper_cpu_timer = enabled;
    cpu_cut_batch_for_event(mapper_cycles_to_event());
}

#define M43_SWAP (*((uint8_t*)cpu_registers + 0x224))

static void m43_apply(void) {
    static const uint8_t banks[8] FRNES_COLD_RODATA = { 4, 3, 5, 3, 6, 3, 7, 3 };
    set_6502_bank_pointer(2, prg_page(banks[M42 -> prg & 7]));
    set_6502_bank_pointer(3, prg_page(M43_SWAP ? 8 : 9));
    set_wram_bank_pointer(prg_page(M43_SWAP ? 0 : 2));
}

static void m43_write(uint16_t address, uint8_t data) {
    switch (address & 0xf1ff) {
        case 0x4022:
            M42 -> prg = data;
            m43_apply();
            break;
        case 0x4120:
            M43_SWAP = data & 1;
            m43_apply();
            break;
        case 0x4122: case 0x8122:
            // (each write clears the count and the IRQ; the timer caught up first)
            if (mapper_cpu_timer) {
                mapper_cpu_run(cpu_cycle_now);
            }
            M42 -> counter = 0;
            cpu_irq_clear(CPU_IRQ_MAPPER);
            m42_counter(data & 1);
            break;
    }
}

static void m304_write(uint16_t address, uint8_t data) {
    if (address == 0x4122) {
        if (mapper_cpu_timer) {
            mapper_cpu_run(cpu_cycle_now);
        }
        M42 -> counter = 0;
        cpu_irq_clear(CPU_IRQ_MAPPER);
        m42_counter((data & 3) != 0);
    } else if ((address == 0x4022) && (M304_LAST >= 7)) {
        prg_16k(0, data & 1);
        prg_16k(2, (data & 1) + 1);
    }
}

static void m368_write(uint16_t address, uint8_t data) {
    static const uint8_t banks[8] FRNES_COLD_RODATA = { 4, 3, 5, 3, 6, 3, 7, 3 };
    if ((address & 0xf1ff) == 0x4022) {
        set_6502_bank_pointer(2, prg_page(banks[data & 7]));
    } else if ((address & 0xf1ff) == 0x4122) {
        M43_SWAP = data & 0x35;
        if (!(data & 1) || !M42 -> enabled) {
            m42_counter(data & 1);
        }
    }
}

static void m50_write(uint16_t address, uint8_t data) {
    if ((address & 0x4120) == 0x4020) {
        set_6502_bank_pointer(2, prg_page((data & 8) | ((data & 1) << 2) | ((data >> 1) & 3)));
    } else if ((address & 0x4120) == 0x4120) {
        if (!(data & 1) || !M42 -> enabled) {
            m42_counter(data & 1);
        }
    }
}

static void m40_write(uint16_t address, uint8_t data) {
    switch (address & 0xe000) {
        case 0x8000: m42_counter(0); break;
        case 0xa000: m42_counter(1); break;
        case 0xe000: set_6502_bank_pointer(2, prg_page(data)); break;
    }
}

static void m42_write(uint16_t address, uint8_t data) {
    m42_state_t* s = M42;
    switch (address & 0xe003) {
        case 0x8000:
            if (num_1k_VROM_pages && !chr_8k_mapped(data & 0x0f)) {
                ppu_sync();
                chr_8k(data & 0x0f);
            }
            break;
        case 0xe000:
            s -> prg = data & 0x0f;
            set_wram_bank_pointer(prg_page(s -> prg));
            break;
        case 0xe001:
            ppu_sync();
            set_ppu_nametable_mirroring((data & 0x08) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
            break;
        case 0xe002:
            m42_counter(data == 0x02);
            break;
    }
}

__attribute__((cold, noinline)) static void m42_init() {
    memset(M42, 0, sizeof(m42_state_t));
    if (board_number == 368) {
        M43_SWAP = 0;
        set_6502_bank_pointer(0, prg_page(1));
        set_6502_bank_pointer(1, prg_page(0));
        set_6502_bank_pointer(2, prg_page(4));
        set_6502_bank_pointer(3, prg_page(8));
        if (WRAM == NULL) {
            resize_wram(0x2000);
        }
        mapper_6000_mode = MAPPER_6000_ROM;
        set_wram_bank_pointer(prg_page(2));
        chr_8k(0);
        return;
    }
    if (board_number == 50) {
        set_6502_bank_pointer(0, prg_page(8));
        set_6502_bank_pointer(1, prg_page(9));
        set_6502_bank_pointer(2, prg_page(0));
        set_6502_bank_pointer(3, prg_page(11));
        if (WRAM == NULL) {
            resize_wram(0x2000);
        }
        mapper_6000_mode = MAPPER_6000_ROM;
        set_wram_bank_pointer(prg_page(15));
        chr_8k(0);
        return;
    }
    if (board_number == 304) {
        // (the last page with data: nes2_exponent_cartridge pads the 40 kB to 48 kB with $FF, and the header then says 48)
        uint32_t last = num_8k_ROM_pages - 1;
        while (last > 1) {
            const uint8_t* page = prg_page(last);
            uint32_t i = 0;
            while ((i < 0x2000) && (page[i] == 0xff)) {
                i++;
            }
            if (i < 0x2000) {
                break;
            }
            last--;
        }
        M304_LAST = (uint8_t)last;
        prg_16k(0, 0);
        prg_16k(2, 1);
        if (WRAM == NULL) {
            resize_wram(0x2000);
        }
        mapper_6000_mode = MAPPER_6000_ROM;
        set_wram_bank_pointer(prg_page(M304_LAST));
        chr_8k(0);
        return;
    }
    if (board_number == 43) {
        set_6502_bank_pointer(0, prg_page(1));
        set_6502_bank_pointer(1, prg_page(0));
        set_6502_bank_pointer(2, prg_page(0));
        if (WRAM == NULL) {
            resize_wram(0x2000);
        }
        mapper_6000_mode = MAPPER_6000_ROM;
        M43_SWAP = 0;
        set_6502_bank_pointer(3, prg_page(9));
        set_wram_bank_pointer(prg_page(2));
        chr_8k(0);
        return;
    }
    if (board_number == 40) {
        prg_32k(1);
        set_6502_bank_pointer(2, prg_page(0));
        if (WRAM == NULL) {
            resize_wram(0x2000);
        }
        mapper_6000_mode = MAPPER_6000_ROM;
        set_wram_bank_pointer(prg_page(6));
        chr_8k(0);
        return;
    }
    prg_32k(num_8k_ROM_pages / 4 - 1);
    // (the $6000 window reads its ROM bank only with WRAM allocated: 6502_readbyte.c gives open bus without it)
    if (WRAM == NULL) {
        resize_wram(0x2000);
    }
    mapper_6000_mode = MAPPER_6000_ROM;
    set_wram_bank_pointer(prg_page(0));
    chr_8k(0);
}

/**
 * Kaiser's KS202 / KS7032 (the nesdev wiki's, Mesen 2's Kaiser202 to compare): $8000-$BFFF the IRQ reload's four
 * nibbles (one register each, low first), $C000 the IRQ control (bit 1 starts the counter from the reload) and $D000
 * the IRQ's acknowledge (a $C000 write's too), $E000 bits 0-2 a register for $F000 (1-3 the 8 kB PRG banks at $8000 /
 * $A000 / $C000, 4 the one at $6000, 5 bit 2 ROM at $6000 (clear: PRG RAM)), $F000 bits 0-3 its value; the last 8 kB
 * at $E000. The counter counts CPU cycles up; the cycle it reaches $FFFF raises the IRQ, reloads it and stops it.
 *   142 (KS-7032: 1988 金牌瑪莉, Exciting Soccer, Final Command, Bubble Bobble): the header's mirroring, 8 kB of CHR RAM
 *   56 (Super Mario Bros. 3's KS-xxxx): also $F000-$F3FF the PRG register (address bits 0-1) bit 4, $F800-$FBFF bit 0
 *     the mirroring (vertical when set), $FC00-$FFFF the 1 kB CHR bank (address bits 0-2)
 * State in the register page (+0x224: the reload, the counter, the control, the selected register, the PRG registers,
 * the $6000 mode; saved with a state)
 */
typedef struct {
    uint16_t reload, counter;
    uint8_t control, select, prg[4], rom_6000;
} ks202_state_t;
#define KS202 ((ks202_state_t*)((uint8_t*)cpu_registers + 0x224))

static void ks202_apply(void) {
    ks202_state_t* k = KS202;
    for (uint32_t i = 0; i < 3; i++) {
        set_6502_bank_pointer(i, prg_page(k -> prg[i]));
    }
    if (k -> rom_6000) {
        mapper_6000_mode = MAPPER_6000_ROM;
        set_wram_bank_pointer(prg_page(k -> prg[3]));
    } else {
        mapper_6000_mode = 0;
        set_wram_bank_pointer(WRAM);
    }
}

static void ks202_cpu_run(uint32_t cycles) {
    ks202_state_t* k = KS202;
    if (!(k -> control & 2)) {
        return;
    }
    uint32_t to_fire = (k -> counter == 0xffff) ? 0x10000 : (0xffffu - k -> counter);
    if (cycles >= to_fire) {
        k -> counter = k -> reload;
        k -> control &= ~2;
        mapper_cpu_timer = 0;
        mapper_irq_flag |= CPU_IRQ_MAPPER;
        cpu_irq_update_line();
    } else {
        k -> counter += cycles;
    }
}

static uint32_t ks202_cycles_to_fire(void) {
    uint32_t counter = KS202 -> counter;
    return (counter == 0xffff) ? 0x10000 : (0xffffu - counter);
}

static void ks202_write(uint16_t address, uint8_t data) {
    ks202_state_t* k = KS202;
    switch (address & 0xf000) {
        case 0x8000: case 0x9000: case 0xa000: case 0xb000: {
            uint32_t shift = ((address >> 12) - 8) * 4;
            k -> reload = (k -> reload & ~(0x0f << shift)) | ((data & 0x0f) << shift);
            return;
        }
        case 0xc000: case 0xd000:
            if (mapper_cpu_timer) {
                mapper_cpu_run(cpu_cycle_now);
            }
            if ((address & 0xf000) == 0xc000) {
                counter_timer_start();
                k -> control = data;
                if (data & 2) {
                    k -> counter = k -> reload;
                }
                mapper_cpu_timer = (data & 2) != 0;
            }
            cpu_irq_clear(CPU_IRQ_MAPPER);
            cpu_cut_batch_for_event(mapper_cycles_to_event());
            return;
        case 0xe000:
            k -> select = (data & 7) - 1;
            return;
    }
    if (k -> select < 4) {
        k -> prg[k -> select] = (k -> prg[k -> select] & 0x10) | (data & 0x0f);
    } else if (k -> select == 4) {
        k -> rom_6000 = data & 4;
    }
    if (board_number == 56) {
        switch (address & 0xfc00) {
            case 0xf000:
                k -> prg[address & 3] = (data & 0x10) | (k -> prg[address & 3] & 0x0f);
                break;
            case 0xf800:
                ppu_sync();
                set_ppu_nametable_mirroring((data & 1) ? NAMETABLE_MIRRORING_VERTICAL : NAMETABLE_MIRRORING_HORIZONTAL);
                break;
            case 0xfc00:
                if (!chr_1k_mapped(address & 7, data)) {
                    ppu_sync();
                    chr_1k(address & 7, data);
                }
                break;
        }
    }
    ks202_apply();
}

__attribute__((cold, noinline)) static void ks202_init() {
    memset(KS202, 0, sizeof(ks202_state_t));
    // (PRG RAM at $6000 until the game maps ROM there)
    if (WRAM == NULL) {
        resize_wram(0x2000);
    }
    ks202_apply();
    set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
    chr_8k(0);
}

/**
 * Small boards of their own:
 *   193 (NTDEC TC-112: Fighting Hero, War in the Gulf; the nesdev wiki's, Mesen 2's NtdecTc112 to compare): writes to
 *     $6000-$7FFF (board_6000), address bits 0-1 the register: 0 the 2 kB CHR banks at $0000 / $0800 (bits 1-7 the
 *     first, the next one after it), 1 / 2 the 2 kB CHR bank at $1000 / $1800 (bits 1-7), 3 the 8 kB PRG bank at $8000;
 *     $A000-$FFFF the last 24 kB, the header's mirroring
 *   235 (the Golden Game 100 / 150 / 210 / 260 / 1500-in-1s; the nesdev wiki's latch, Mesen 2's Bmc235 to compare): the
 *     written address, bits 0-4 the 32 kB PRG bank, bits 8-9 the ROM chip (its bank bits 5-6: on 2 MB carts chip 2 bank
 *     bit 5, chips 1 and 3 unpopulated), bit 11 16 kB at both halves (bit 12 the half), bit 10 one screen (else bit 13:
 *     horizontal when set); 8 kB of CHR RAM. An unpopulated chip reads as open bus (board_state's page: each byte its
 *     address's high byte; WINDOW_BOARD in a state)
 *   396 (BMC-830752C: the J.Y. 8-in-1s, Realtec's Golden 16-in-1 and GN-51 2-in-1; the nesdev wiki's, no Mesen 2 board):
 *     UNROM in a 128 kB outer bank: $A000-$BFFF (submapper 3: $8000-$BFFF) bits 0-2 the outer bank, the mirroring
 *     (vertical when set) its bit 6 (submapper 1), bit 5 (2 and 3) or either (0); the rest bits 0-2 the 16 kB bank at
 *     $8000, the outer bank's last at $C000; 8 kB of CHR RAM. In the register page (+0x238: outer, inner; with a state)
 *   314 (PB011003-3x1: the Y2K 32 / 64 / 76-in-1s, Super HiK 42-in-1; the nesdev wiki's, Mesen 2's Bmc64in1NoRepeat to
 *     compare): $5000-$5FFF (board_low) decoded with $F003 (the wiki's guess; the 42-in-1's menu starts its games with
 *     writes to $5180, $5231...: Mesen 2's $5000-$5003 only miss them, its menu starting over): $5000 bit 0 PRG A21, bit 7 NROM (clear: UNROM), bits 1-2 the 8 kB CHR bank's low bits, bit 5
 *     the mirroring (horizontal when set); $5001 bits 0-5 the 32 kB bank, bit 6 the 16 kB half, bit 7 (NROM) 32 kB;
 *     $5002 the CHR bank's bits 2-7; UNROM: $8000-$FFFF bits 0-2 the 16 kB bank at $8000 inside $5001's 128 kB, its
 *     last at $C000 (Mesen 2 maps $5001's bank at $C000). Power on and reset: $5000 $80, $5001 $43. In the register
 *     page (+0x23a-+0x23d)
 *   286 (Benshieng's BS-5 4-in-1s, Shudu 5-in-1; the nesdev wiki's, Mesen 2's Bs5 to compare): the written address,
 *     $8000-$8FFF bits 10-11 the 2 kB CHR slot and bits 0-4 its bank, $A000-$AFFF bits 10-11 the 8 kB PRG slot and bits
 *     0-3 its bank when bit 4 is set (the DIP switch's bit, at 0); all banks the last at power on
 *   374 (the reset-based MMC1 4-in-1s: the J.Y. 1995-1997 Super HiK 4-in-1s, F-034; the nesdev wiki's, no Mesen 2
 *     board): an MMC1 (the NWC's serial port: a write with bit 7 set resets it, the fifth loads the register $8000 /
 *     $A000 / $C000 / $E000 selects) inside a 128 kB outer bank of PRG and of CHR, the next at each reset. In the
 *     register page (+0x23e-+0x244: the port, the registers, the game; with a state)
 *   403 (the 89433 board: the Sachen Superpacks, Tetris Family 19-in-1; the nesdev wiki's, no Mesen 2 board): writes to
 *     $4100-$5FFF decoded with $E103 (board_low): $4100 bits 0-5 the 16 kB PRG bank (NROM-128) or bits 1-5 the 32 kB
 *     one, $4101 bits 0-1 the 8 kB CHR RAM bank (of 32 kB; also $8000-$FFFF writes with $4102 bit 2), $4102 bit 0
 *     NROM-128, bit 4 the mirroring (horizontal when set). All 0 at power on. In the register page (+0x247-+0x249)
 *   434 (S-009: the 1403 / 1408 / YH-4327 4-in-1s, 1994 超值金卡 8-in-1; the nesdev wiki's, no Mesen 2 board): UNROM in a
 *     128 kB outer bank, $6000-$7FFF (board_6000) bits 0-2 the outer bank and bit 5 the mirroring (vertical when
 *     set), $8000-$FFFF bits 0-2 the 16 kB bank at $8000, the outer bank's last at $C000. In the register page
 *     (+0x245-+0x246)
 *   283 (GS-2004 / GS-2013: the RCM Tetris Family 5 / 6-in-1s, 方塊全集 12-in-1; the nesdev wiki's, Mesen 2's Gs2004 /
 *     Gs2013 to compare): writes to $8000-$FFFF the 32 kB PRG bank (bits 0-2 with GS-2004's extra 8 kB chip, the
 *     PRG a power of two and 16 kB; else bits 0-3), the last at power on; at $6000 the extra chip, or the first
 *     256 kB's last 8 kB; 8 kB of CHR RAM
 *   439 (YS2309: the DS-05 / 08 / 26 / 32 4-in-1s; the nesdev wiki's, no Mesen 2 board): UxROM, $8000-$FFFF bits 0-5 the
 *     16 kB bank at $8000 ($3F at $C000), bit 7 the mirroring (horizontal when set) unless $6001 bit 7 locks it;
 *     $6000-$7FFF (board_6000, address bit 0): $6001 bits 4-6 PRG A17-A19 from $6000's bits 4-6 instead (both
 *     halves). All 0 at power on. In the register page (+0x24a-+0x24c: the latch, $6000, $6001)
 *   29 (Sealie Computing's RET-CUFROM: the Glider games; the nesdev wiki's, Mesen 2's SealieComputing to compare):
 *     $8000-$FFFF bits 2-4 the 16 kB PRG bank at $8000 (the last at $C000), bits 0-1 the 8 kB bank of 32 kB CHR RAM
 *   133 (Sachen 72008: Jovial Race, Master Chu; the nesdev wiki's, Mesen 2's Sachen_133 to compare): writes to $4100
 *     (decoded with $E100, board_low) bit 2 the 32 kB PRG bank, bits 0-1 the 8 kB CHR bank
 *   202 (the Well 93 150 / 300 / 450-in-1s; Mesen 2's Mapper202 to compare): the written address, bits 1-3 the 16 kB
 *     PRG bank at both halves and the 8 kB CHR bank, bit 0 the mirroring (horizontal when set), bits 0 and 3 both set
 *     the bank and the next one (32 kB)
 *   203 (the 35-in-1 / 42-in-1 game cards; Mesen 2's Mapper203 to compare): the written value, bits 2-7 the 16 kB PRG
 *     bank at both halves, bits 0-1 the 8 kB CHR bank
 *   216 (Bonza, Magic Jewelry 2; Mesen 2's Mapper216 to compare): the written address ($8000-$FFFF; $5000 as its own
 *     address), bit 0 the 32 kB PRG bank, bits 1-3 the 8 kB CHR bank; $5000 reads 0 (board_low)
 *   230 (Tsang Hai's 20 / 22-in-1+Contra, 23-in-1; Mesen 2's Mapper230 to compare): each reset (and power on) switches
 *     between Contra (the first 128 kB: the written value's bits 0-2 the bank at $8000, 7 at $C000, vertical) and the
 *     multicart (the rest: bits 0-4 + 8 the 16 kB bank at both halves with bit 5, else the 32 kB pair; bit 6 the
 *     mirroring, vertical when set). In the register page (+0x24d: the mode)
 *   285 (A65AS: the N068 3-in-1s...; Mesen 2's A65AS to compare): the written value, bit 6 32 kB (bits 1-4), else
 *     bits 4-5 a 128 kB block with bits 0-2 the 16 kB bank at $8000 and its last at $C000; bit 7 one screen (bit 5 the
 *     page), else bit 3 the mirroring (horizontal when set)
 *   336 (BMC-K-3046: the YH-218 2-in-1s...; Mesen 2's BmcK3046 to compare): the written value, bits 3-5 a 128 kB block,
 *     bits 0-2 the 16 kB bank at $8000, the block's last at $C000
 *   219 (A9746: Toy Story, 狮子王...; Mesen 2's MMC3_219 to compare): an MMC3 for $A000-$FFFF (the mirroring, the IRQ)
 *     with its own banks: $8000 the command (and the PRG latch 0), $8002 the PRG latch (the command 0), $8001 the
 *     data: with the PRG latch $23-$26 the 8 kB bank of slot $26 - latch from the value's bits 2-5 reversed; commands
 *     $08 / $0A / $0E / $12 / $16 / $1A / $1E the CHR banks' high nibble (the value's low), the others a 1 kB CHR bank
 *     from the value's bits 1-4 (table below) with it. The last 32 kB at power on. In the register page (+0x254-+0x256)
 *   246 (C&E's 封神榜 / 楚漢爭霸; Mesen 2's Mapper246 to compare): $6000-$67FF (board_6000, address bits 0-2): 0-3 the
 *     8 kB PRG banks, 4-7 the 2 kB CHR banks; $6800-$7FFF the WRAM's. The last 8 kB at $E000 at power on and reset
 *   171 (Kaiser KS-7058: 推倒胡麻將...; Mesen 2's Kaiser7058 to compare): $F000-$FFFF writes the 4 kB CHR bank at $0000
 *     (address bit 7 clear) or $1000; 32 kB of PRG
 *   290 (BMC-NTD-03: the Asder 20-in-1; Mesen 2's BmcNtd03 to compare): the written address, bits 11-14 the 32 kB PRG
 *     bank, with bit 7 bit 6 the 16 kB half at both halves; bits 8-9 and 0-2 the 8 kB CHR bank (bits 3-4, 0-2), bit 10
 *     the mirroring (horizontal when set)
 *   81 (NTDEC's N715021: Super Gun; the nesdev wiki's, no Mesen 2 board): the written address, bits 2-3 the 16 kB PRG
 *     bank at $8000 (the last at $C000), bits 0-1 the 8 kB CHR bank; the header's mirroring
 *   62 (the Super 190 / 700-in-1s; Mesen 2's Mapper62 to compare): the written address, bits 8-13 and 6 the 16 kB PRG bank
 *     (bit 5 at both halves, else the 32 kB pair), bits 0-4 and the value's bits 0-1 the 8 kB CHR bank (bits 2-6, 0-1),
 *     bit 7 the mirroring (horizontal when set)
 *   261 (BMC-810544-C-A1: the Elfland 200-in-1s; Mesen 2's Bmc810544CA1 to compare): the written address, bits 7-15 the
 *     32 kB PRG bank (bit 6 clear: 16 kB, with bit 5, at both halves), bits 0-3 the 8 kB CHR bank, bit 4 the mirroring
 *   143 (Sachen's TC-A001-72P: Dancing Blocks, 麻將方塊; Mesen 2's Sachen_143 to compare): NROM; $4100-$5FFF reads
 *     (board_low) give the address's bits 0-5 inverted, bit 6 set
 *   149 (Sachen's SA-0036: 臺灣 16 麻雀...; Mesen 2's Sachen_149 to compare): $8000-$FFFF writes bit 7 the 8 kB CHR bank
 *   51 (the 11-in-1 Ball Games JY-010...; Mesen 2's Bmc51 to compare): $8000-$FFFF the bank (bits 0-3; $C000-$DFFF
 *     also mode bit 1 from bit 4), $6000-$7FFF the mode (bit 1 from bit 4, bit 0 from bit 1): mode bit 0 the bank's
 *     32 kB (8 kB page $23 | bank x 4 at $6000), else 16 kB from page bank x 4 | mode at $8000, bank x 4 | $0E at
 *     $C000, $2F | bank x 4 at $6000; mode 3 horizontal. Bank 0, mode 1 at power on. In the register page (+0x279:
 *     the bank, the mode)
 *   61 (the RCM Tetris Family 9-in-1, 20-in-1...; Mesen 2's Mapper61 to compare): the written address, bits 0-3 the
 *     32 kB PRG bank (bit 4 set: 16 kB, with bit 5 the half, at both halves), bit 7 the mirroring (horizontal when set)
 *   190 (Magic Kid Googoo; Mesen 2's MagicKidGooGoo to compare): $8000-$9FFF bits 0-2 the 16 kB PRG bank at $8000,
 *     $C000-$DFFF the same with bit 3 set (bank 0 at $C000); $A000-$BFFF / $E000-$FFFF (address & $A000) address bits
 *     0-1 a 2 kB CHR slot, the value its bank; vertical
 *   120 (Tobidase Daisakusen's FDS conversion, LH15; Mesen 2's Mapper120 to compare): $41FF writes (board_low) the 8 kB
 *     PRG bank at $6000; banks 8-11 at $8000-$FFFF
 *   346 (Kaiser KS-7012: Zanac A.I.; Mesen 2's Kaiser7012 to compare): a write to exactly $E0A0 the 32 kB PRG bank 0,
 *     $EE36 bank 1 (bank 1 at power on)
 *   312 (Kaiser KS-7013B: Highway Star; Mesen 2's Kaiser7013B to compare): $6000-$7FFF writes (board_6000) the 16 kB PRG
 *     bank at $8000 (the last at $C000), $8000-$FFFF bit 0 the mirroring (horizontal when set); vertical at power on
 *   306 (Kaiser KS-7016: Exciting Basket; Mesen 2's Kaiser7016 to compare): the written address, & $D943: $D943 with
 *     bits 4-5 set bank $0B, else bits 2-5 the bank; $D903 with them set $08 | bits 2-3, else $0B: the 8 kB bank at
 *     $6000 ($08 at power on); banks $0C-$0F at $8000-$FFFF. In the register page (+0x28b)
 *   309 (Whirlwind Manu's LH51: 愛戦士 Nicol; Mesen 2's Lh51 to compare): $8000-$9FFF bits 0-3 the 8 kB PRG bank at
 *     $8000 (banks 13-15 after it), $E000-$FFFF bit 3 the mirroring (horizontal when set)
 *   521 (DreamTech01: 장두진 바둑교실; Mesen 2's DreamTech01 to compare): $5020 writes (board_low) bits 0-2 the 16 kB PRG
 *     bank at $8000 (bank 8 at $C000)
 *   530 (AX5705: Super Mario Bros. Pocker Mali; Mesen 2's Ax5705 to compare): writes decoded with $F00F: $8000 / $A000
 *     the 8 kB PRG banks at $8000 / $A000 (bits 1 and 3 swapped), $8008 bit 0 the mirroring (horizontal when set); from
 *     $A008 by $F00E: $A008 / $A00A / $C000 / $C002 / $C008 / $C00A / $E000 / $E002 the 1 kB CHR banks, address bit 0
 *     the high nibble (bits 1 and 2 swapped); the last 16 kB at $C000. In the register page (+0x2a4-+0x2ab: CHR)
 *   46 (Color Dreams' RumbleStation 15-in-1; Mesen 2's ColorDreams46 to compare): $6000-$7FFF (board_6000) bits 0-3
 *     the 32 kB PRG bank's bits 1-4 and bits 4-7 the 8 kB CHR bank's bits 3-6; $8000-$FFFF bit 0 the PRG bank's bit 0
 *     and bits 4-6 the CHR bank's bits 0-2. Both 0 at power on and reset. In the register page (+0x2b1-+0x2b2)
 *   415 (Lucky Rabbit's FDS conversion; the nesdev wiki's, no Mesen 2 board): $8000-$FFFF bits 0-3 the 8 kB PRG bank
 *     at $6000, bit 4 the mirroring (horizontal when set); the last 32 kB at $8000; 8 kB of CHR RAM
 *   125 (Whirlwind Manu's LH32: Monty on the Run; Mesen 2's Lh32 to compare): $6000-$7FFF writes (board_6000, decoded
 *     with $E000) bits 0-3 the 8 kB PRG bank at $6000; banks 12 / 13 at $8000 / $A000, 8 kB of PRG RAM at $C000
 *     (board_write writes it through the NES window), bank 15 at $E000; 8 kB of CHR RAM
 *   522 (Whirlwind Manu's LH10: 風雲少林拳; Mesen 2's Lh10 to compare): writes decoded with $E001: $8000 bits 0-2 the
 *     register, $8001 its value (6 / 7: the 8 kB PRG banks at $8000 / $A000); the PRG's second last 8 kB at $6000, 8 kB
 *     of PRG RAM at $C000 (as 125's), the last at $E000; 8 kB of CHR RAM. The register in the register page (+0x2b3)
 *   103 (Whirlwind Manu's LH30: 夢工場 ドキドキ Panic; Mesen 2's Mapper103 to compare): the last 32 kB at $8000; PRG RAM at
 *     $6000-$7FFF and $B800-$D7FF unless $F000 bit 4 is set, which maps the 8 kB PRG bank $8000 bits 0-3 choose at $6000
 *     and ROM under $B800-$D7FF; the RAM is written either way. $E000 bit 3 the mirroring (horizontal when set). WRAM's
 *     second and third 8 kB are $A000's and $C000's pages with the RAM in them (m103_apply), written through the NES
 *     window while mapped and else directly, each write's cache line then purged (the CPU reads them through the
 *     window's addresses, another set of cache lines: the window's lines are purged before it maps ROM). The bank and
 *     the switch in the register page (+0x2b4-+0x2b5)
 *   535 (Whirlwind Manu's LH53: 謎の村雨城; the nesdev wiki's and FCEUX's lh53, no Mesen 2 board): $F000-$FFFF the 8 kB PRG
 *     bank at $6000; the last 32 kB at $8000 with 8 kB of PRG RAM over $B800-$D7FF (WRAM's two pages are $A000's and
 *     $C000's with the RAM in them, always mapped: written through the NES window); $E000-$EFFF bit 1 the counter on
 *     (each write clears it and drops the IRQ): CPU cycles, the IRQ held from the 7561st (FCEUX's count past 7560).
 *     8 kB of CHR RAM. State in the register page (+0x2b6-+0x2b9)
 *   305 (Kaiser's KS7031: Dracula II 呪いの封印; the nesdev wiki's, Mesen 2's Kaiser7031 to compare): 2 kB PRG banks, the
 *     8 kB slots WRAM's pages put together from them: $8000-$FFFF writes, bits 11-12 the register, the 2 kB bank at
 *     $6000 / $6800 / $7000 / $7800 (copied into the window's page through the NES window, mapper_6000_mode
 *     MAPPER_6000_REGISTERS keeping the CPU's writes out); $8000-$FFFF the PRG's 2 kB banks 15 down to 0 (copied at power
 *     on). Vertical mirroring; 8 kB of CHR RAM. The banks in the register page (+0x2ba-+0x2bd)
 *   302 (Kaiser's KS7057: Gyruss; the nesdev wiki's, Mesen 2's Kaiser7057 to compare): 2 kB PRG banks as 305's, at
 *     $6000-$7FFF and $8000-$9FFF (WRAM's two pages, the banks copied in through the NES window): writes decoded with
 *     $F003 from $B000 to $E003, address bits 12-14 and 1 the register ($B000 / $B002 / $C000 / $C002 the banks at
 *     $8000-$9FFF, $D000 / $D002 / $E000 / $E002 at $6000-$7FFF), bit 0 the nibble (high when set); $8000-$9FFF bit 0
 *     the mirroring (vertical when set); 8 kB banks 13 at $A000, 14 / 15 at $C000. 8 kB of CHR RAM. The banks in the
 *     register page (+0x2be-+0x2c5)
 *   307 (Kaiser's KS7037: Metroid; the nesdev wiki's, Mesen 2's Kaiser7037 to compare): $8000-$9FFF even the register,
 *     odd its value (2-5: bit 0 the CIRAM page at $2000 / $2800 / $2400 / $2C00; 6 / 7: the 8 kB PRG banks at $8000 /
 *     $C000); 8 kB of PRG RAM in 4 kB halves beside 4 kB ROM halves, WRAM's two pages: $6000 the RAM's first half
 *     and the PRG's 4 kB bank 15, $A000 the 4 kB bank 28 and the RAM's second half (written through the NES window:
 *     $6000-$6FFF by board_write_6000, mapper_6000_mode MAPPER_6000_REGISTERS keeping the ROM half's writes out;
 *     $B000-$BFFF by board_write); 8 kB bank 15 at $E000. 8 kB of CHR RAM. The register and the registers in the
 *     register page (+0x2c6-+0x2ce)
 *   167 (Subor's 中英文电脑学习机 IV, the Russian Сюбор Обучающий Компьютер; the nesdev wiki's, Mesen 2's Subor166 to
 *     compare): four registers by $E000, each bank bit twice and XORed: $8000 bit 4 and $A000 bit 4 the 16 kB bank's bit
 *     5, $C000 and $E000 bits 0-4 its bits 0-4; $A000 bits 2-3 the mode (0: the bank at $8000 and $20 at $C000; 1: $1F
 *     at $8000 and the bank at $C000; 2 / 3: 32 kB, the bank's bit 0 set at $8000 and clear at $C000); $8000 bit 0 the
 *     mirroring (vertical when set); 8 kB of CHR RAM. In the register page (+0x2ec-+0x2ef)
 *   519 (UNL-EH8813A: 小学生数学 CAI...; the nesdev wiki's, Mesen 2's Eh8813A to compare): $8000-$FFFF writes latch the
 *     address and the data: A0-A5 the 16 kB PRG bank, A7 its mode (32 kB, bit 0 ignored, or 16 kB twice), A6 the DIP
 *     switch read mode, D0-D6 the 8 kB CHR bank, D7 the mirroring (horizontal when set), A8 locking all but D0-D1. In
 *     the DIP read mode a $8000-$FFFF read takes its 16 byte row's byte the switches choose (0 here): WRAM's last 32 kB
 *     then hold the ROM so read, built at each change and mapped instead (the window's lines purged first, as the CPU
 *     reads it through them). Power on and reset: all 0. In the register page (+0x2f0-+0x2f3)
 *   438 (K-3071 multicarts: the 70-in-1...; the nesdev wiki's): $8000-$FFFF writes latch A1-A5 the 16 kB PRG bank, A0
 *     its mode (32 kB with bit 0 ignored, or 16 kB twice), D1-D5 the 8 kB CHR bank, D0 the mirroring (horizontal when
 *     set); all 0 at power on and reset
 *   449 (the Super Games King multicart, 1 MB of PRG and 32 kB of CHR RAM; the nesdev wiki's): $8000-$FFFF writes latch
 *     the address: A2-A6 and A8 PRG A14-A19 (A2 A14, A3-A4 A15-A16, A5-A6 A17-A18, A8 A19), A7 the mode (0: the bank at
 *     $8000 and its 128 kB's bank 7 at $C000; 1: 16 kB twice, or 32 kB with A0 set), A1 the mirroring (horizontal when
 *     set), A9 the solder pads' read mode (each 16 byte row's byte the pads choose, 0 here: WRAM's pages as 519's), D0-D1
 *     the 8 kB CHR RAM bank; 0 at power on and reset. In the register page (+0x2fa-+0x2fd)
 *   60 (the reset-based 4-in-1s; the nesdev wiki's, Mesen 2's Mapper60 to compare): each reset the next game, its 16 kB
 *     of PRG at both halves and its 8 kB of CHR (+0x22f in the register page: the resets, saved with a state)
 */
static void tc112_write(uint16_t address, uint8_t data) {
    ppu_sync();
    switch (address & 3) {
        case 0:
            chr_2k(0, (data >> 1) * 2);
            chr_2k(2, ((data >> 1) + 1) * 2);
            break;
        case 1:
            chr_2k(4, (data >> 1) * 2);
            break;
        case 2:
            chr_2k(6, (data >> 1) * 2);
            break;
        case 3:
            set_6502_bank_pointer(0, prg_page(data));
            break;
    }
}

// the ROM chips a 235 cart has by its size: 1 MB chip 0, 2 MB chips 0 and 2, 4 MB 0, 2 and 3, any other size all four
static int bmc235_chip_missing(uint32_t chip) {
    switch (num_8k_ROM_pages) {
        case 128: return chip != 0;
        case 256: return (chip & 1) != 0;
        case 512: return chip == 1;
        default: return 0;
    }
}

static void bmc235_write(uint16_t address, uint8_t data) {
    (void)data;
    uint32_t chip = (address >> 8) & 3;
    // (the chips' 32 kB banks one after another: chip 2 bank 32 on a 2 MB cart, 3 bank 64 on a 4 MB one)
    uint32_t high = (num_8k_ROM_pages == 256) ? ((chip & 2) << 4) : (num_8k_ROM_pages == 512) ? ((chip == 3) ? 0x40 : (chip & 2) << 4) :
        (chip << 5);
    uint32_t bank = high | (address & 0x1f);
    ppu_sync();
    if (address & 0x0400) {
        set_ppu_nametable_mirroring(NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW);
    } else {
        set_ppu_nametable_mirroring((address & 0x2000) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
    }
    if (bmc235_chip_missing(chip) && board_state) {
        // (an unpopulated chip: board_state's open bus page, each byte its address's high byte, what an absolute read
        // leaves on the bus)
        for (uint32_t i = 0; i < 4; i++) {
            set_6502_bank_pointer(i, (uint8_t*)board_state + i * 0x2000);
        }
    } else if (address & 0x0800) {
        uint32_t half = bank * 2 + ((address >> 12) & 1);
        prg_16k(0, half);
        prg_16k(2, half);
    } else {
        prg_32k(bank);
    }
}

#define M396 ((uint8_t*)cpu_registers + 0x238)

static void m396_apply(void) {
    uint32_t outer = (M396[0] & 7) << 3;
    prg_16k(0, outer | (M396[1] & 7));
    prg_16k(2, outer | 7);
}

static void m396_write(uint16_t address, uint8_t data) {
    int outer = (board_submapper == 3) ? ((address & 0xc000) == 0x8000) : ((address & 0xe000) == 0xa000);
    if (outer) {
        uint32_t bits = (board_submapper == 1) ? 0x40 : (board_submapper == 0) ? 0x60 : 0x20;
        M396[0] = data;
        ppu_sync();
        set_ppu_nametable_mirroring((data & bits) ? NAMETABLE_MIRRORING_VERTICAL : NAMETABLE_MIRRORING_HORIZONTAL);
    } else {
        M396[1] = data;
    }
    m396_apply();
}

#define M314 ((uint8_t*)cpu_registers + 0x23a)

static void m314_apply(void) {
    const uint8_t* r = M314;
    // (the 16 kB bank: A14 $5001 bit 6, A15-A20 its bits 0-5, A21 $5000 bit 0)
    uint32_t bank16 = ((r[0] & 1) << 7) | ((r[1] & 0x3f) << 1) | ((r[1] >> 6) & 1);
    if (!(r[0] & 0x80)) {
        prg_16k(0, (bank16 & ~7u) | (r[3] & 7));
        prg_16k(2, bank16 | 7);
    } else if (r[1] & 0x80) {
        prg_32k(bank16 >> 1);
    } else {
        prg_16k(0, bank16);
        prg_16k(2, bank16);
    }
    uint32_t chr = (r[2] << 2) | ((r[0] >> 1) & 3);
    if (!chr_8k_mapped(chr)) {
        ppu_sync();
        chr_8k(chr);
    }
    set_ppu_nametable_mirroring((r[0] & 0x20) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
}

static void m314_reset(void) {
    uint8_t* r = M314;
    r[0] = 0x80;
    r[1] = 0x43;
    r[2] = r[3] = 0;
    m314_apply();
}

static void bs5_write(uint16_t address) {
    uint32_t slot = (address >> 10) & 3;
    if ((address & 0xf000) == 0x8000) {
        if (!chr_1k_mapped(slot * 2, (address & 0x1f) * 2) || !chr_1k_mapped(slot * 2 + 1, (address & 0x1f) * 2 + 1)) {
            ppu_sync();
            chr_2k(slot * 2, (address & 0x1f) * 2);
        }
    } else if (((address & 0xf000) == 0xa000) && (address & 0x10)) {
        set_6502_bank_pointer(slot, prg_page(address & 0x0f));
    }
}

typedef struct {
    uint8_t shift, count, control, chr0, chr1, prg, game;
} m374_state_t;
#define M374 ((m374_state_t*)((uint8_t*)cpu_registers + 0x23e))

static void m374_apply(void) {
    const m374_state_t* m = M374;
    static const uint8_t mirrorings[4] FRNES_COLD_RODATA = { NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW,
        NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH, NAMETABLE_MIRRORING_VERTICAL, NAMETABLE_MIRRORING_HORIZONTAL };
    uint32_t prg = (m -> game & 3) * 8, chr = (m -> game & 3) * 32;
    switch ((m -> control >> 2) & 3) {
        case 0: case 1:
            prg_32k((prg | (m -> prg & 6)) >> 1);
            break;
        case 2:
            prg_16k(0, prg);
            prg_16k(2, prg | (m -> prg & 7));
            break;
        case 3:
            prg_16k(0, prg | (m -> prg & 7));
            prg_16k(2, prg | 7);
            break;
    }
    ppu_sync();
    if (m -> control & 0x10) {
        chr_4k(0, chr | (m -> chr0 & 0x1f));
        chr_4k(1, chr | (m -> chr1 & 0x1f));
    } else {
        chr_8k((chr | (m -> chr0 & 0x1e)) >> 1);
    }
    set_ppu_nametable_mirroring(mirrorings[m -> control & 3]);
}

static void m374_write(uint16_t address, uint8_t data) {
    m374_state_t* m = M374;
    if (data & 0x80) {
        m -> shift = m -> count = 0;
        m -> control |= 0x0c;
        m374_apply();
        return;
    }
    m -> shift |= (data & 1) << m -> count;
    if (++m -> count < 5) {
        return;
    }
    uint8_t value = m -> shift;
    m -> shift = m -> count = 0;
    switch (address & 0xe000) {
        case 0x8000: m -> control = value; break;
        case 0xa000: m -> chr0 = value; break;
        case 0xc000: m -> chr1 = value; break;
        case 0xe000: m -> prg = value; break;
    }
    m374_apply();
}

#define M434 ((uint8_t*)cpu_registers + 0x245)

static void m434_apply(void) {
    uint32_t outer = (M434[0] & 7) << 3;
    prg_16k(0, outer | (M434[1] & 7));
    prg_16k(2, outer | 7);
}

#define M403 ((uint8_t*)cpu_registers + 0x247)

static void m403_apply(void) {
    const uint8_t* r = M403;
    if (r[2] & 1) {
        prg_16k(0, r[0] & 0x3f);
        prg_16k(2, r[0] & 0x3f);
    } else {
        prg_32k((r[0] & 0x3f) >> 1);
    }
    if (!chr_8k_mapped(r[1] & 3)) {
        ppu_sync();
        chr_8k(r[1] & 3);
    }
    set_ppu_nametable_mirroring((r[2] & 0x10) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
}

static void m403_write_low(uint16_t address, uint8_t data) {
    uint32_t reg = address & 0xe103;
    if ((reg >= 0x4100) && (reg <= 0x4102)) {
        M403[reg & 3] = data;
        m403_apply();
    }
}

// GS-2004's extra 8 kB chip: PRG a power of two and 16 kB (the chip's 8 kB twice)
static int gs2004_extra_chip(void) {
    uint32_t main = num_8k_ROM_pages - 2;
    return (num_8k_ROM_pages > 2) && !(main & (main - 1));
}

static void gs2004_write(uint8_t data) {
    prg_32k(data & (gs2004_extra_chip() ? 7 : 0x0f));
}

#define M439 ((uint8_t*)cpu_registers + 0x24a)

static void m439_apply(void) {
    const uint8_t* r = M439;
    uint32_t mask = (r[2] & 0x70) >> 1, outer = (r[1] & 0x70) >> 1;
    prg_16k(0, ((r[0] & 0x3f) & ~mask) | (outer & mask));
    prg_16k(2, (0x3f & ~mask) | (outer & mask));
}

static void m29_write(uint8_t data) {
    prg_16k(0, (data >> 2) & 7);
    if (!chr_8k_mapped(data & 3)) {
        ppu_sync();
        chr_8k(data & 3);
    }
}

static void latch202_write(uint16_t address) {
    uint32_t bank = (address >> 1) & 7;
    prg_16k(0, bank);
    prg_16k(2, ((address & 9) == 9) ? bank + 1 : bank);
    ppu_sync();
    chr_8k(bank);
    set_ppu_nametable_mirroring((address & 1) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
}

static void latch216_write(uint16_t address) {
    prg_32k(address & 1);
    if (!chr_8k_mapped((address >> 1) & 7)) {
        ppu_sync();
        chr_8k((address >> 1) & 7);
    }
}

#define M230_CONTRA (*((uint8_t*)cpu_registers + 0x24d))

static void m230_mode(void) {
    ppu_sync();
    if (M230_CONTRA) {
        prg_16k(0, 0);
        prg_16k(2, 7);
        set_ppu_nametable_mirroring(NAMETABLE_MIRRORING_VERTICAL);
    } else {
        prg_16k(0, 8);
        prg_16k(2, 9);
        set_ppu_nametable_mirroring(NAMETABLE_MIRRORING_HORIZONTAL);
    }
}

static void m230_write(uint8_t data) {
    if (M230_CONTRA) {
        prg_16k(0, data & 7);
        return;
    }
    if (data & 0x20) {
        prg_16k(0, (data & 0x1f) + 8);
        prg_16k(2, (data & 0x1f) + 8);
    } else {
        prg_16k(0, (data & 0x1e) + 8);
        prg_16k(2, (data & 0x1e) + 9);
    }
    ppu_sync();
    set_ppu_nametable_mirroring((data & 0x40) ? NAMETABLE_MIRRORING_VERTICAL : NAMETABLE_MIRRORING_HORIZONTAL);
}

static void a65as_write(uint8_t data) {
    if (data & 0x40) {
        prg_32k((data & 0x1e) >> 1);
    } else {
        uint32_t block = (data & 0x30) >> 1;
        prg_16k(0, block | (data & 7));
        prg_16k(2, block | 7);
    }
    ppu_sync();
    if (data & 0x80) {
        set_ppu_nametable_mirroring((data & 0x20) ? NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH : NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW);
    } else {
        set_ppu_nametable_mirroring((data & 0x08) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
    }
}

#define M219 ((uint8_t*)cpu_registers + 0x254)

static void mmc3_board_write(uint16_t address, uint8_t data);

static void a9746_write(uint16_t address, uint8_t data) {
    uint8_t* r = M219;
    if (address >= 0xa000) {
        mmc3_board_write(address, data);
        return;
    }
    switch (address & 0xe003) {
        case 0x8000:
            r[0] = 0;
            r[1] = data;
            return;
        case 0x8002:
            r[0] = data;
            r[1] = 0;
            return;
        case 0x8001:
            break;
        default:
            return;
    }
    if ((r[0] >= 0x23) && (r[0] <= 0x26)) {
        uint32_t bank = ((data >> 5) & 1) | ((data >> 3) & 2) | ((data >> 1) & 4) | ((data << 1) & 8);
        set_6502_bank_pointer(0x26 - r[0], prg_page(bank));
    }
    // (command -> 1 kB slot: 0x80 the high nibble latch, 0x40 the slot's value OR 1, 0x10 AND $0E, else AND $0F)
    static const uint8_t slots[0x20] FRNES_COLD_RODATA = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x10, 0x80, 0x41, 0x12, 0x12, 0x80, 0x43,
        0x04, 0x04, 0x80, 0xff, 0x05, 0x05, 0x80, 0xff, 0x06, 0x06, 0x80, 0xff, 0x07, 0x07, 0x80, 0xff };
    uint32_t slot = (r[1] < 0x20) ? slots[r[1]] : 0xff;
    if (slot == 0xff) {
        return;
    }
    if (slot == 0x80) {
        r[2] = (uint8_t)(data << 4);
        return;
    }
    uint32_t value = data >> 1;
    value = (slot & 0x40) ? (value | 1) : ((slot & 0x10) ? (value & 0x0e) : (value & 0x0f));
    ppu_sync();
    chr_1k(slot & 7, r[2] | value);
}

static void ntd03_write(uint16_t address) {
    uint32_t bank16 = (address >> 10) & 0x1e, chr = ((address >> 5) & 0x18) | (address & 7);
    if (address & 0x80) {
        prg_16k(0, bank16 | ((address >> 6) & 1));
        prg_16k(2, bank16 | ((address >> 6) & 1));
    } else {
        prg_32k(bank16 >> 1);
    }
    ppu_sync();
    chr_8k(chr);
    set_ppu_nametable_mirroring((address & 0x400) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
}

static void latch62_write(uint16_t address, uint8_t data) {
    uint32_t bank = ((address >> 8) & 0x3f) | (address & 0x40);
    if (address & 0x20) {
        prg_16k(0, bank);
        prg_16k(2, bank);
    } else {
        prg_32k(bank >> 1);
    }
    ppu_sync();
    chr_8k(((address & 0x1f) << 2) | (data & 3));
    set_ppu_nametable_mirroring((address & 0x80) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
}

static void latch261_write(uint16_t address) {
    uint32_t bank = (address >> 6) & 0x3fe;
    if (address & 0x40) {
        prg_32k(bank >> 1);
    } else {
        prg_16k(0, bank | ((address >> 5) & 1));
        prg_16k(2, bank | ((address >> 5) & 1));
    }
    ppu_sync();
    chr_8k(address & 0x0f);
    set_ppu_nametable_mirroring((address & 0x10) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
}

#define M51 ((uint8_t*)cpu_registers + 0x279)

static void m51_apply(void) {
    uint32_t base = (M51[0] & 0x0f) << 2, mode = M51[1] & 3;
    if (mode & 1) {
        prg_32k(base >> 2);
        set_wram_bank_pointer(prg_page(0x23 | base));
    } else {
        set_6502_bank_pointer(0, prg_page(base | mode));
        set_6502_bank_pointer(1, prg_page((base | mode) + 1));
        set_6502_bank_pointer(2, prg_page(base | 0x0e));
        set_6502_bank_pointer(3, prg_page((base | 0x0e) + 1));
        set_wram_bank_pointer(prg_page(0x2f | base));
    }
    ppu_sync();
    set_ppu_nametable_mirroring((mode == 3) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
}

static void latch61_write(uint16_t address) {
    uint32_t bank16 = ((address & 0x0f) << 1) | ((address >> 5) & 1);
    if (address & 0x10) {
        prg_16k(0, bank16);
        prg_16k(2, bank16);
    } else {
        prg_32k(address & 0x0f);
    }
    ppu_sync();
    set_ppu_nametable_mirroring((address & 0x80) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
}

#define KS7016_PRG (*((uint8_t*)cpu_registers + 0x28b))

static void ks7016_write(uint16_t address) {
    uint32_t mode = (address & 0x30) == 0x30;
    switch (address & 0xd943) {
        case 0xd943: KS7016_PRG = mode ? 0x0b : ((address >> 2) & 0x0f); break;
        case 0xd903: KS7016_PRG = mode ? (0x08 | ((address >> 2) & 3)) : 0x0b; break;
        default: return;
    }
    set_wram_bank_pointer(prg_page(KS7016_PRG));
}

#define AX5705_CHR ((uint8_t*)cpu_registers + 0x2a4)

static void ax5705_write(uint16_t address, uint8_t data) {
    if (address < 0xa008) {
        switch (address & 0xf00f) {
            case 0x8000: case 0xa000:
                set_6502_bank_pointer((address >> 13) & 1, prg_page((data & 5) | ((data >> 2) & 2) | ((data << 2) & 8)));
                break;
            case 0x8008:
                ppu_sync();
                set_ppu_nametable_mirroring((data & 1) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
                break;
        }
        return;
    }
    uint32_t reg;
    switch (address & 0xf00e) {
        case 0xa008: reg = 0; break;
        case 0xa00a: reg = 1; break;
        case 0xc000: reg = 2; break;
        case 0xc002: reg = 3; break;
        case 0xc008: reg = 4; break;
        case 0xc00a: reg = 5; break;
        case 0xe000: reg = 6; break;
        case 0xe002: reg = 7; break;
        default: return;
    }
    uint8_t* c = AX5705_CHR;
    if (address & 1) {
        uint32_t high = (data & 9) | ((data >> 1) & 2) | ((data << 1) & 4);
        c[reg] = (uint8_t)((c[reg] & 0x0f) | (high << 4));
    } else {
        c[reg] = (c[reg] & 0xf0) | (data & 0x0f);
    }
    ppu_sync();
    chr_1k(reg, c[reg]);
}

#define M46 ((uint8_t*)cpu_registers + 0x2b1)

static void m46_apply(void) {
    const uint8_t* r = M46;
    prg_32k(((r[0] & 0x0f) << 1) | (r[1] & 1));
    uint32_t chr = ((r[0] & 0xf0) >> 1) | ((r[1] & 0x70) >> 4);
    if (!chr_8k_mapped(chr)) {
        ppu_sync();
        chr_8k(chr);
    }
}

#define M522_REGISTER (*((uint8_t*)cpu_registers + 0x2b3))
#define M103 ((uint8_t*)cpu_registers + 0x2b4)
#define M167 ((uint8_t*)cpu_registers + 0x2ec)

static void m167_apply(void) {
    const uint8_t* r = M167;
    uint32_t bank = (((r[0] ^ r[1]) & 0x10) << 1) | ((r[2] ^ r[3]) & 0x1f), mode = (r[1] >> 2) & 3;
    if (mode >= 2) {
        prg_16k(0, bank | 1);
        prg_16k(2, bank & ~1u);
    } else if (mode == 1) {
        prg_16k(0, 0x1f);
        prg_16k(2, bank);
    } else {
        prg_16k(0, bank);
        prg_16k(2, 0x20);
    }
}

static void m103_purge(uintptr_t address, uint32_t size) {
#ifdef _arch_dreamcast
    for (uintptr_t line = address & ~(uintptr_t)31; line < address + size; line += 32) {
        __asm__ volatile("ocbp @%0" :: "r"(line) : "memory");
    }
#else
    (void)address;
    (void)size;
#endif
}

#define M449 ((uint8_t*)cpu_registers + 0x2fa)

static void m449_apply(void) {
    uint8_t* r = M449;
    uint32_t a = r[0] | ((uint32_t)r[1] << 8);
    uint32_t bank = ((a >> 2) & 0x1f) | (((a >> 8) & 1) << 5), pages[4];
    uint32_t first = bank, second = (a & 0x80) ? bank : (bank | 7);
    if ((a & 0x80) && (a & 1)) {
        first = bank & ~1u;
        second = bank | 1;
    }
    pages[0] = first << 1;
    pages[1] = (first << 1) | 1;
    pages[2] = second << 1;
    pages[3] = (second << 1) | 1;
    if (r[3]) {
        m103_purge(ROMBANK0_ADDR, 0x8000);
        r[3] = 0;
    }
    for (uint32_t i = 0; i < 4; i++) {
        set_6502_bank_pointer(i, prg_page(pages[i]));
    }
    if (a & 0x200) {
        uint8_t* pad_pages = WRAM + 0x2000;
        for (uint32_t i = 0; i < 4; i++) {
            const uint8_t* from = prg_page(pages[i]);
            for (uint32_t row = 0; row < 0x2000; row += 16) {
                memset(pad_pages + i * 0x2000 + row, from[row], 16);
            }
        }
        m103_purge((uintptr_t)pad_pages, 0x8000);
        for (uint32_t i = 0; i < 4; i++) {
            set_6502_bank_pointer(i, pad_pages + i * 0x2000);
        }
        r[3] = 1;
    }
    ppu_sync();
    for (uint32_t i = 0; i < 8; i++) {
        set_ppu_pattern_table_bank_pointer(i, chr_ram_page((r[2] & 3) * 8 + i));
    }
    set_ppu_nametable_mirroring((a & 2) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
}

static void m438_write(uint16_t address, uint8_t data) {
    uint32_t bank = (address >> 1) & 0x1f;
    if (address & 1) {
        prg_32k(bank >> 1);
    } else {
        prg_16k(0, bank);
        prg_16k(2, bank);
    }
    if (!chr_8k_mapped((data >> 1) & 0x1f)) {
        ppu_sync();
        chr_8k((data >> 1) & 0x1f);
    }
    ppu_sync();
    set_ppu_nametable_mirroring((data & 1) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
}

#define M519 ((uint8_t*)cpu_registers + 0x2f0)

static void m519_apply(void) {
    uint8_t* r = M519;
    uint32_t bank = r[0] & 0x3f;
    uint32_t pages[4];
    for (uint32_t i = 0; i < 4; i++) {
        pages[i] = (r[0] & 0x80) ? ((bank << 1) | (i & 1)) : (((bank & ~1u) << 1) | i);
    }
    if (r[3]) {
        // (the DIP read pages were mapped: their lines through the window go before the pages change)
        m103_purge(ROMBANK0_ADDR, 0x8000);
        r[3] = 0;
    }
    for (uint32_t i = 0; i < 4; i++) {
        set_6502_bank_pointer(i, prg_page(pages[i]));
    }
    if (r[0] & 0x40) {
        uint8_t* pages_read = WRAM + 0x2000;
        for (uint32_t i = 0; i < 4; i++) {
            const uint8_t* from = prg_page(pages[i]);
            uint8_t* to = pages_read + i * 0x2000;
            for (uint32_t row = 0; row < 0x2000; row += 16) {
                memset(to + row, from[row], 16);
            }
        }
        m103_purge((uintptr_t)pages_read, 0x8000);
        for (uint32_t i = 0; i < 4; i++) {
            set_6502_bank_pointer(i, pages_read + i * 0x2000);
        }
        r[3] = 1;
    }
    if (!chr_8k_mapped(r[1] & 0x7f)) {
        ppu_sync();
        chr_8k(r[1] & 0x7f);
    }
    ppu_sync();
    set_ppu_nametable_mirroring((r[1] & 0x80) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
}

static void m519_write(uint16_t address, uint8_t data) {
    uint8_t* r = M519;
    if (r[2]) {
        r[1] = (r[1] & ~3) | (data & 3);
    } else {
        r[0] = address & 0xff;
        r[1] = data;
        r[2] = (address >> 8) & 1;
    }
    m519_apply();
}

static void m103_apply(void) {
    if (M103[1]) {
        if (mapper_6000_mode != MAPPER_6000_REGISTERS) {
            // (the RAM's lines through the window, before it maps ROM)
            m103_purge(WRAM_ADDR, 0x2000);
            m103_purge(ROMBANK0_ADDR + 0x2000, 0x4000);
        }
        mapper_6000_mode = MAPPER_6000_REGISTERS;
        set_wram_bank_pointer(prg_page(M103[0]));
        set_6502_bank_pointer(1, prg_page(num_8k_ROM_pages - 3));
        set_6502_bank_pointer(2, prg_page(num_8k_ROM_pages - 2));
    } else {
        mapper_6000_mode = 0;
        set_wram_bank_pointer(WRAM);
        set_6502_bank_pointer(1, WRAM + 0x2000);
        set_6502_bank_pointer(2, WRAM + 0x4000);
    }
}

typedef struct {
    uint16_t counter;
    uint8_t enabled, prg;
} m535_state_t;
#define M535 ((m535_state_t*)((uint8_t*)cpu_registers + 0x2b6))
#define M535_IRQ_AT 7561

static void m535_cpu_run(uint32_t cycles) {
    m535_state_t* m = M535;
    if (!m -> enabled || (m -> counter >= M535_IRQ_AT)) {
        return;
    }
    if (cycles >= (uint32_t)(M535_IRQ_AT - m -> counter)) {
        m -> counter = M535_IRQ_AT;
        mapper_cpu_timer = 0;
        mapper_irq_flag |= CPU_IRQ_MAPPER;
        cpu_irq_update_line();
    } else {
        m -> counter += cycles;
    }
}

static uint32_t m535_cycles_to_fire(void) {
    const m535_state_t* m = M535;
    return (m -> enabled && (m -> counter < M535_IRQ_AT)) ? (uint32_t)(M535_IRQ_AT - m -> counter) : 0xffffffff;
}

static void m535_irq_write(uint8_t data) {
    m535_state_t* m = M535;
    if (mapper_cpu_timer) {
        mapper_cpu_run(cpu_cycle_now);
    }
    counter_timer_start();
    m -> counter = 0;
    m -> enabled = (data & 2) != 0;
    cpu_irq_clear(CPU_IRQ_MAPPER);
    mapper_cpu_timer = m -> enabled;
    cpu_cut_batch_for_event(mapper_cycles_to_event());
}

#define M305 ((uint8_t*)cpu_registers + 0x2ba)

static const uint8_t* prg_2k(uint32_t bank) {
    return prg_page((bank >> 2) % num_8k_ROM_pages) + (bank & 3) * 0x800;
}

static void m305_bank(uint32_t slot) {
    memcpy((uint8_t*)WRAM_ADDR + slot * 0x800, prg_2k(M305[slot]), 0x800);
}

#define M302 ((uint8_t*)cpu_registers + 0x2be)

static void m302_bank(uint32_t r) {
    uint8_t* to = (r < 4) ? (uint8_t*)ROMBANK0_ADDR + r * 0x800 : (uint8_t*)WRAM_ADDR + (r - 4) * 0x800;
    memcpy(to, prg_2k(M302[r]), 0x800);
}

static void m302_write(uint16_t address, uint8_t data) {
    if (address < 0xa000) {
        ppu_sync();
        set_ppu_nametable_mirroring((data & 1) ? NAMETABLE_MIRRORING_VERTICAL : NAMETABLE_MIRRORING_HORIZONTAL);
        return;
    }
    if ((address < 0xb000) || (address >= 0xf000)) {
        return;
    }
    uint32_t r = ((((uint32_t)address >> 12) - 0xb) << 1) | ((address >> 1) & 1);
    M302[r] = (address & 1) ? (uint8_t)((M302[r] & 0x0f) | (data << 4)) : (uint8_t)((M302[r] & 0xf0) | (data & 0x0f));
    m302_bank(r);
}

#define M307 ((uint8_t*)cpu_registers + 0x2c6)

static void m307_write(uint16_t address, uint8_t data) {
    if (address >= 0xa000) {
        if ((address & 0xf000) == 0xb000) {
            NES_WINDOW_BYTE(address) = data;
        }
        return;
    }
    if (!(address & 1)) {
        M307[0] = data & 7;
        return;
    }
    uint32_t r = M307[0];
    M307[1 + r] = data;
    if (r == 6) {
        set_6502_bank_pointer(0, prg_page(data));
    } else if (r == 7) {
        set_6502_bank_pointer(2, prg_page(data));
    } else if (r >= 2) {
        const uint8_t* n = M307 + 1;
        ppu_sync();
        set_ppu_nametable_pages(n[2] & 1, n[4] & 1, n[3] & 1, n[5] & 1);
    }
}

static void m103_ram_write(uint8_t* p, uint8_t data) {
    *p = data;
    m103_purge((uintptr_t)p, 1);
}
#define M60_RESETS (*((uint8_t*)cpu_registers + 0x22f))

static void m60_apply(void) {
    ppu_sync();
    prg_16k(0, M60_RESETS);
    prg_16k(2, M60_RESETS);
    chr_8k(M60_RESETS);
}

__attribute__((cold, noinline)) static void small_boards_init() {
    switch (board_number) {
        case 193:
            mapper_6000_mode = MAPPER_6000_REGISTERS;
            board_6000 = 1;
            set_6502_bank_pointer(0, prg_page(0));
            for (uint32_t i = 1; i < 4; i++) {
                set_6502_bank_pointer(i, prg_page(num_8k_ROM_pages - 4 + i));
            }
            chr_8k(0);
            break;
        case 235: {
            uint8_t* open_bus = board_buffer_alloc(0x8000, 32);
            if (open_bus) {
                for (uint32_t i = 0; i < 0x8000; i++) {
                    open_bus[i] = (uint8_t)((0x8000 + i) >> 8);
                }
            }
            prg_32k(0);
            chr_8k(0);
            break;
        }
        case 60:
            M60_RESETS = 0;
            m60_apply();
            break;
        case BOARD_830752:
            M396[0] = M396[1] = 0;
            m396_apply();
            chr_8k(0);
            break;
        case BOARD_PB011003:
            m314_reset();
            break;
        case BOARD_S009:
            mapper_6000_mode = MAPPER_6000_REGISTERS;
            board_6000 = 1;
            M434[0] = M434[1] = 0;
            m434_apply();
            chr_8k(0);
            break;
        case BOARD_89433:
            memset(M403, 0, 3);
            m403_apply();
            break;
        case 202: case 203:
            prg_16k(0, 0);
            prg_16k(2, 0);
            chr_8k(0);
            break;
        case 246:
            mapper_6000_mode = MAPPER_6000_REGISTERS;
            board_6000 = 1;
            if (WRAM == NULL) {
                resize_wram(0x2000);
            }
            prg_32k(num_8k_ROM_pages / 4 - 1);
            chr_8k(0);
            break;
        case 171:
            prg_32k(0);
            chr_8k(0);
            break;
        case 81:
            prg_16k(0, 0);
            prg_16k(2, num_8k_ROM_pages / 2 - 1);
            chr_8k(0);
            break;
        case 62:
            prg_32k(0);
            chr_8k(0);
            break;
        case 143: case 149: case 61:
            prg_32k(0);
            chr_8k(0);
            break;
        case BOARD_KS7012:
            prg_32k(1);
            chr_8k(0);
            break;
        case 307:
            resize_wram(0x4000);
            mapper_6000_mode = MAPPER_6000_REGISTERS;
            board_6000 = 1;
            set_wram_bank_pointer(WRAM);
            memcpy((uint8_t*)WRAM_ADDR + 0x1000, prg_page(7) + 0x1000, 0x1000);
            memcpy(WRAM + 0x2000, prg_page(14), 0x1000);
            m103_purge((uintptr_t)WRAM + 0x2000, 0x1000);
            memset(M307, 0, 9);
            set_6502_bank_pointer(0, prg_page(0));
            set_6502_bank_pointer(1, WRAM + 0x2000);
            set_6502_bank_pointer(2, prg_page(0));
            set_6502_bank_pointer(3, prg_page(15));
            set_ppu_nametable_pages(0, 0, 0, 0);
            chr_8k(0);
            break;
        case 302:
            resize_wram(0x4000);
            set_6502_bank_pointer(0, WRAM + 0x2000);
            set_6502_bank_pointer(1, prg_page(13));
            set_6502_bank_pointer(2, prg_page(14));
            set_6502_bank_pointer(3, prg_page(15));
            mapper_6000_mode = MAPPER_6000_REGISTERS;
            board_6000 = 1;
            set_wram_bank_pointer(WRAM);
            for (uint32_t i = 0; i < 8; i++) {
                M302[i] = 0;
                m302_bank(i);
            }
            chr_8k(0);
            break;
        case 305:
            resize_wram(0xa000);
            for (uint32_t i = 0; i < 16; i++) {
                memcpy(WRAM + 0x2000 + i * 0x800, prg_2k(15 - i), 0x800);
            }
            m103_purge((uintptr_t)WRAM + 0x2000, 0x8000);
            for (uint32_t i = 0; i < 4; i++) {
                set_6502_bank_pointer(i, WRAM + 0x2000 + i * 0x2000);
            }
            mapper_6000_mode = MAPPER_6000_REGISTERS;
            board_6000 = 1;
            set_wram_bank_pointer(WRAM);
            for (uint32_t i = 0; i < 4; i++) {
                M305[i] = 0;
                m305_bank(i);
            }
            set_ppu_nametable_mirroring(NAMETABLE_MIRRORING_VERTICAL);
            chr_8k(0);
            break;
        case 535:
            resize_wram(0x4000);
            // (ROM at $6000 first: the host's window would copy itself back over WRAM's first page)
            mapper_6000_mode = MAPPER_6000_ROM;
            set_wram_bank_pointer(prg_page(0));
            memcpy(WRAM, prg_page(13), 0x1800);
            memcpy(WRAM + 0x2000 + 0x1800, prg_page(14) + 0x1800, 0x800);
            m103_purge((uintptr_t)WRAM, 0x4000);
            memset(M535, 0, sizeof(m535_state_t));
            set_6502_bank_pointer(0, prg_page(12));
            set_6502_bank_pointer(1, WRAM);
            set_6502_bank_pointer(2, WRAM + 0x2000);
            set_6502_bank_pointer(3, prg_page(15));
            chr_8k(0);
            break;
        case 103:
            resize_wram(0x6000);
            memcpy(WRAM + 0x2000, prg_page(num_8k_ROM_pages - 3), 0x1800);
            memcpy(WRAM + 0x4000 + 0x1800, prg_page(num_8k_ROM_pages - 2) + 0x1800, 0x800);
            m103_purge((uintptr_t)WRAM, 0x6000);
            board_6000 = 1;
            set_6502_bank_pointer(0, prg_page(num_8k_ROM_pages - 4));
            set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
            M103[0] = M103[1] = 0;
            m103_apply();
            chr_8k(0);
            break;
        case 522:
            if (WRAM == NULL) {
                resize_wram(0x2000);
            }
            mapper_6000_mode = MAPPER_6000_ROM;
            set_wram_bank_pointer(prg_page(num_8k_ROM_pages - 2));
            set_6502_bank_pointer(0, prg_page(0));
            set_6502_bank_pointer(1, prg_page(0));
            set_6502_bank_pointer(2, WRAM);
            set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
            M522_REGISTER = 0;
            chr_8k(0);
            break;
        case 438:
            m438_write(0x8000, 0);
            break;
        case 449:
            resize_wram(0xa000);
            memset(M449, 0, 4);
            m449_apply();
            break;
        case 519:
            resize_wram(0xa000);
            memset(M519, 0, 4);
            m519_apply();
            break;
        case 167:
            memset(M167, 0, 4);
            m167_apply();
            chr_8k(0);
            break;
        case 125:
            mapper_6000_mode = MAPPER_6000_REGISTERS;
            board_6000 = 1;
            if (WRAM == NULL) {
                resize_wram(0x2000);
            }
            set_6502_bank_pointer(0, prg_page(12));
            set_6502_bank_pointer(1, prg_page(13));
            set_6502_bank_pointer(2, WRAM);
            set_6502_bank_pointer(3, prg_page(15));
            set_wram_bank_pointer(prg_page(0));
            chr_8k(0);
            break;
        case 415:
            prg_32k(num_8k_ROM_pages / 4 - 1);
            if (WRAM == NULL) {
                resize_wram(0x2000);
            }
            mapper_6000_mode = MAPPER_6000_ROM;
            set_wram_bank_pointer(prg_page(0));
            chr_8k(0);
            break;
        case 46:
            mapper_6000_mode = MAPPER_6000_REGISTERS;
            board_6000 = 1;
            M46[0] = M46[1] = 0;
            m46_apply();
            break;
        case 309:
            set_6502_bank_pointer(0, prg_page(0));
            set_6502_bank_pointer(1, prg_page(13));
            set_6502_bank_pointer(2, prg_page(14));
            set_6502_bank_pointer(3, prg_page(15));
            chr_8k(0);
            break;
        case 521:
            prg_16k(0, 0);
            prg_16k(2, 8);
            chr_8k(0);
            break;
        case 530:
            memset(AX5705_CHR, 0, 8);
            prg_16k(2, num_8k_ROM_pages / 2 - 1);
            chr_8k(0);
            break;
        case BOARD_KS7013B:
            mapper_6000_mode = MAPPER_6000_REGISTERS;
            board_6000 = 1;
            prg_16k(0, 0);
            prg_16k(2, num_8k_ROM_pages / 2 - 1);
            chr_8k(0);
            set_ppu_nametable_mirroring(NAMETABLE_MIRRORING_VERTICAL);
            break;
        case BOARD_KS7016:
            for (uint32_t i = 0; i < 4; i++) {
                set_6502_bank_pointer(i, prg_page(0x0c + i));
            }
            if (WRAM == NULL) {
                resize_wram(0x2000);
            }
            mapper_6000_mode = MAPPER_6000_ROM;
            KS7016_PRG = 8;
            set_wram_bank_pointer(prg_page(8));
            chr_8k(0);
            break;
        case 120:
            set_6502_bank_pointer(0, prg_page(8));
            set_6502_bank_pointer(1, prg_page(9));
            set_6502_bank_pointer(2, prg_page(10));
            set_6502_bank_pointer(3, prg_page(11));
            if (WRAM == NULL) {
                resize_wram(0x2000);
            }
            mapper_6000_mode = MAPPER_6000_ROM;
            set_wram_bank_pointer(prg_page(0));
            chr_8k(0);
            break;
        case 190:
            prg_16k(0, 0);
            prg_16k(2, 0);
            chr_8k(0);
            set_ppu_nametable_mirroring(NAMETABLE_MIRRORING_VERTICAL);
            break;
        case 51:
            mapper_6000_mode = MAPPER_6000_REGISTERS;
            board_6000 = 1;
            if (WRAM == NULL) {
                resize_wram(0x2000);
            }
            M51[0] = 0;
            M51[1] = 1;
            m51_apply();
            chr_8k(0);
            break;
        case BOARD_810544:
            latch261_write(0x8000);
            break;
        case BOARD_NTD03:
            ntd03_write(0x8000);
            break;
        case 219:
            mmc3_board_init();
            memset(M219, 0, 3);
            prg_32k(num_8k_ROM_pages / 4 - 1);
            chr_8k(0);
            break;
        case BOARD_A65AS:
            a65as_write(0);
            chr_8k(0);
            break;
        case BOARD_K3046:
            prg_16k(0, 0);
            prg_16k(2, 7);
            chr_8k(0);
            break;
        case 216:
            latch216_write(0x8000);
            break;
        case 230:
            M230_CONTRA = 1;
            m230_mode();
            chr_8k(0);
            break;
        case 29:
            prg_16k(0, 0);
            prg_16k(2, num_8k_ROM_pages / 2 - 1);
            chr_8k(0);
            break;
        case 133:
            prg_32k(0);
            chr_8k(0);
            break;
        case BOARD_YS2309:
            mapper_6000_mode = MAPPER_6000_REGISTERS;
            board_6000 = 1;
            memset(M439, 0, 3);
            m439_apply();
            set_ppu_nametable_mirroring(NAMETABLE_MIRRORING_VERTICAL);
            chr_8k(0);
            break;
        case BOARD_GS2004: {
            uint32_t main = gs2004_extra_chip() ? (num_8k_ROM_pages - 2) : num_8k_ROM_pages;
            prg_32k(main / 4 - 1);
            if (WRAM == NULL) {
                resize_wram(0x2000);
            }
            mapper_6000_mode = MAPPER_6000_ROM;
            set_wram_bank_pointer(prg_page(gs2004_extra_chip() ? main : 0x1f));
            chr_8k(0);
            break;
        }
        case BOARD_MMC1_RESET:
            memset(M374, 0, sizeof(m374_state_t));
            M374 -> control = 0x0c;
            m374_apply();
            break;
        case BOARD_BS5:
            for (uint32_t i = 0; i < 4; i++) {
                set_6502_bank_pointer(i, prg_page(0x0f));
                chr_2k(i * 2, 0x1f * 2);
            }
            break;
    }
}

/**
 * Mapper 222 (the CTC-31 Dragon Ninja, Super Bros. 8 (JY211); Mesen 2's Mapper222 to compare, the nesdev wiki has no
 * registers): a VRC2 with a scanline IRQ, writes decoded with $F003: $8000 / $A000 the 8 kB PRG banks there (the last
 * 16 kB at $C000), $9000 bit 0 the mirroring (horizontal when set), $B000 / $B002 / $C000 ... $E002 the 1 kB CHR banks
 * 0-7, $F000 the IRQ counter (and its acknowledge). The counter counts the PPU's A12 rises while it isn't 0, the IRQ
 * when it reaches 240 (back to 0); as the J.Y. boards' line source, one at each rendering line's dot 260 (lines 0-239
 * and the pre-render line) on the board timer. State in the register page (+0x230: the counter, the next line, the
 * timer's base; saved with a state)
 */
typedef struct {
    uint8_t counter, next_line_set;
    uint16_t next_line;
    uint32_t timer_base;
} m222_state_t;
#define M222 ((m222_state_t*)((uint8_t*)cpu_registers + 0x230))

static void m222_line_start(void) {
    int32_t to_end = (int32_t)(ppu_dot_cpu_cycle(261, 340) - M222 -> timer_base);
    int32_t position = 262 * 341 - 1 - ((to_end > 0) ? to_end * 3 : 0);
    if (position < 0) {
        position = 0;
    }
    M222 -> next_line = jy_next_line_after((uint32_t)position / 341, (uint32_t)position % 341);
    M222 -> next_line_set = 1;
}

static void m222_cpu_run(uint32_t cycles) {
    m222_state_t* m = M222;
    m -> timer_base += cycles;
    if (!m -> counter) {
        return;
    }
    if (!m -> next_line_set) {
        m222_line_start();
    }
    for (uint32_t n = 0; (n < 300) && m -> counter; n++) {
        uint32_t line = m -> next_line;
        uint32_t at = ppu_dot_cpu_cycle(line, 260);
        if ((int32_t)(at - m -> timer_base) > 0) {
            break;
        }
        if (((const _PPU_registers_t*)((const uint8_t*)cpu_registers + 32)) -> PPUMASK & 0x18) {
            m -> counter++;
            if (m -> counter >= 240) {
                m -> counter = 0;
                mapper_cpu_timer = 0;
                cpu_irq_raise_at(CPU_IRQ_MAPPER, at);
            }
        }
        m -> next_line = (line == 239) ? 261 : ((line == 261) ? 0 : line + 1);
    }
}

static uint32_t m222_cycles_to_fire(void) {
    if (!M222 -> counter) {
        return 0xffffffff;
    }
    if (!M222 -> next_line_set) {
        m222_line_start();
    }
    int32_t cycles = (int32_t)(ppu_dot_cpu_cycle(M222 -> next_line, 260) - M222 -> timer_base);
    return (cycles > 0) ? (uint32_t)cycles : 1;
}

static void m222_write(uint16_t address, uint8_t data) {
    uint32_t reg = address & 0xf003;
    switch (reg) {
        case 0x8000:
            set_6502_bank_pointer(0, prg_page(data));
            return;
        case 0xa000:
            set_6502_bank_pointer(1, prg_page(data));
            return;
        case 0x9000:
            ppu_sync();
            set_ppu_nametable_mirroring((data & 1) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
            return;
        case 0xb000: case 0xb002: case 0xc000: case 0xc002: case 0xd000: case 0xd002: case 0xe000: case 0xe002: {
            uint32_t slot = (((reg >> 12) - 0x0b) << 1) | ((reg >> 1) & 1);
            if (!chr_1k_mapped(slot, data)) {
                ppu_sync();
                chr_1k(slot, data);
            }
            return;
        }
        case 0xf000:
            if (mapper_cpu_timer) {
                mapper_cpu_run(cpu_cycle_now);
            }
            if (!mapper_cpu_timer) {
                M222 -> timer_base = cpu_cycle_now;
            }
            counter_timer_start();
            M222 -> counter = data;
            M222 -> next_line_set = 0;
            cpu_irq_clear(CPU_IRQ_MAPPER);
            mapper_cpu_timer = (data != 0);
            cpu_cut_batch_for_event(mapper_cycles_to_event());
            return;
    }
}

__attribute__((cold, noinline)) static void m222_init() {
    memset(M222, 0, sizeof(m222_state_t));
    prg_16k(0, 0);
    prg_16k(2, num_8k_ROM_pages / 2 - 1);
    chr_8k(0);
}

/**
 * Mapper 359 (BMC-SB-5013 / GCL8050 / 841242C: the Super Boy 6-in-1 / Top 5-in-1s, 96 Supreme 3-in-1; the nesdev
 * wiki's, no Mesen 2 board): writes decoded with $F003: $8000-$8003 the 8 kB banks at $8000 / $A000 / $C000 / $6000
 * (the outer bank's last at $E000), $9000 bits 3-5 the 128 kB outer PRG bank, $9001 bits 0-1 the inner PRG mask (512 /
 * 256 / - / 128 kB) and bit 6 the inner CHR one (256 kB, else 128), $9002 the mirroring (V, H, one screen low / high),
 * $9003 bits 0-1 the 128 kB outer CHR bank, $A000-$B003 the 1 kB CHR banks (CHR ROM; CHR RAM unbanked). The IRQ:
 * $C002 bit 0 on, bit 1 the source (CPU cycles: a 16-bit count down from $C000 / $C001 to 0; else the PPU's A12 rises
 * as the MMC3's, here at each rendering line's dot 260 as the J.Y. boards' line source), bit 2 $C000 off and $C001 on
 * ($C003 bit 0 otherwise); in A12 mode $C000 the reload, $C001 the latch. State in the register page (+0x258-+0x277)
 */
typedef struct {
    uint8_t prg[4], chr[8];
    uint8_t outer_prg, sizes, mirroring, outer_chr;
    uint8_t mode, enabled, latch, a12_counter, reload, next_line_set;
    uint16_t counter, next_line;
} sb5013_state_t;
#define SB5013 ((sb5013_state_t*)((uint8_t*)cpu_registers + 0x258))
#define SB5013_BASE (*((uint32_t*)((uint8_t*)cpu_registers + 0x274)))

static void sb5013_apply(void) {
    const sb5013_state_t* b = SB5013;
    static const uint8_t masks[4] FRNES_COLD_RODATA = { 0x3f, 0x1f, 0x2f, 0x0f };
    static const uint8_t mirrorings[4] FRNES_COLD_RODATA = { NAMETABLE_MIRRORING_VERTICAL, NAMETABLE_MIRRORING_HORIZONTAL,
        NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW, NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH };
    uint32_t mask = masks[b -> sizes & 3], outer = (b -> outer_prg & 0x38) << 1;
    for (uint32_t i = 0; i < 3; i++) {
        set_6502_bank_pointer(i, prg_page(outer | (b -> prg[i] & mask)));
    }
    set_6502_bank_pointer(3, prg_page(outer | mask));
    if (WRAM != NULL) {
        set_wram_bank_pointer(prg_page(outer | (b -> prg[3] & mask)));
    }
    ppu_sync();
    if (num_1k_VROM_pages) {
        uint32_t chr_mask = (b -> sizes & 0x40) ? 0xff : 0x7f, chr_outer = (b -> outer_chr & 3) << 7;
        for (uint32_t i = 0; i < 8; i++) {
            chr_1k(i, chr_outer | (b -> chr[i] & chr_mask));
        }
    }
    set_ppu_nametable_mirroring(mirrorings[b -> mirroring & 3]);
}

static void sb5013_line_start(void) {
    int32_t to_end = (int32_t)(ppu_dot_cpu_cycle(261, 340) - SB5013_BASE);
    int32_t position = 262 * 341 - 1 - ((to_end > 0) ? to_end * 3 : 0);
    if (position < 0) {
        position = 0;
    }
    SB5013 -> next_line = jy_next_line_after((uint32_t)position / 341, (uint32_t)position % 341);
    SB5013 -> next_line_set = 1;
}

static void sb5013_cpu_run(uint32_t cycles) {
    sb5013_state_t* b = SB5013;
    SB5013_BASE += cycles;
    if (!(b -> mode & 2)) {
        if (!b -> enabled || !b -> counter) {
            return;
        }
        if (cycles >= b -> counter) {
            b -> counter = 0;
            mapper_cpu_timer = 0;
            mapper_irq_flag |= CPU_IRQ_MAPPER;
            cpu_irq_update_line();
        } else {
            b -> counter -= cycles;
        }
        return;
    }
    if (!b -> next_line_set) {
        sb5013_line_start();
    }
    for (uint32_t n = 0; n < 300; n++) {
        uint32_t line = b -> next_line;
        uint32_t at = ppu_dot_cpu_cycle(line, 260);
        if ((int32_t)(at - SB5013_BASE) > 0) {
            break;
        }
        if (((const _PPU_registers_t*)((const uint8_t*)cpu_registers + 32)) -> PPUMASK & 0x18) {
            // (the MMC3's: reloaded when 0 or asked, else counted down; the IRQ at 0)
            if (!b -> a12_counter || b -> reload) {
                b -> a12_counter = b -> latch;
                b -> reload = 0;
            } else {
                b -> a12_counter--;
            }
            if (!b -> a12_counter && b -> enabled) {
                cpu_irq_raise_at(CPU_IRQ_MAPPER, at);
            }
        }
        b -> next_line = (line == 239) ? 261 : ((line == 261) ? 0 : line + 1);
    }
}

static uint32_t sb5013_cycles_to_fire(void) {
    const sb5013_state_t* b = SB5013;
    if (!(b -> mode & 2)) {
        return (b -> enabled && b -> counter) ? b -> counter : 0xffffffff;
    }
    if (!b -> next_line_set) {
        sb5013_line_start();
    }
    int32_t cycles = (int32_t)(ppu_dot_cpu_cycle(b -> next_line, 260) - SB5013_BASE);
    return (cycles > 0) ? (uint32_t)cycles : 1;
}

static void sb5013_irq_changed(void) {
    sb5013_state_t* b = SB5013;
    if (!mapper_cpu_timer) {
        SB5013_BASE = cpu_cycle_now;
    }
    counter_timer_start();
    b -> next_line_set = 0;
    mapper_cpu_timer = (b -> mode & 2) ? 1 : (b -> enabled && b -> counter);
    cpu_cut_batch_for_event(mapper_cycles_to_event());
}

static void sb5013_write(uint16_t address, uint8_t data) {
    sb5013_state_t* b = SB5013;
    uint32_t reg = address & 0xf003;
    if ((reg & 0xf000) == 0xc000) {
        if (mapper_cpu_timer) {
            mapper_cpu_run(cpu_cycle_now);
        }
        switch (reg) {
            case 0xc000:
                if (b -> mode & 2) {
                    b -> reload = 1;
                } else {
                    b -> counter = (b -> counter & 0xff00) | data;
                }
                if (b -> mode & 4) {
                    b -> enabled = 0;
                    cpu_irq_clear(CPU_IRQ_MAPPER);
                }
                break;
            case 0xc001:
                if (b -> mode & 2) {
                    b -> latch = data;
                } else {
                    b -> counter = (b -> counter & 0x00ff) | (data << 8);
                }
                if (b -> mode & 4) {
                    b -> enabled = 1;
                }
                break;
            case 0xc002:
                b -> mode = data;
                b -> enabled = data & 1;
                if (!b -> enabled) {
                    cpu_irq_clear(CPU_IRQ_MAPPER);
                }
                break;
            case 0xc003:
                if (!(b -> mode & 4)) {
                    b -> enabled = data & 1;
                    if (!b -> enabled) {
                        cpu_irq_clear(CPU_IRQ_MAPPER);
                    }
                }
                break;
        }
        sb5013_irq_changed();
        return;
    }
    switch (reg & 0xf000) {
        case 0x8000: b -> prg[reg & 3] = data; break;
        case 0x9000:
            switch (reg & 3) {
                case 0: b -> outer_prg = data; break;
                case 1: b -> sizes = data; break;
                case 2: b -> mirroring = data; break;
                case 3: b -> outer_chr = data; break;
            }
            break;
        case 0xa000: b -> chr[reg & 3] = data; break;
        case 0xb000: b -> chr[4 + (reg & 3)] = data; break;
        default: return;
    }
    sb5013_apply();
}

__attribute__((cold, noinline)) static void sb5013_init() {
    memset(SB5013, 0, sizeof(sb5013_state_t));
    SB5013_BASE = 0;
    if (WRAM == NULL) {
        resize_wram(0x2000);
    }
    mapper_6000_mode = MAPPER_6000_ROM;
    for (uint32_t i = 0; i < 8; i++) {
        SB5013 -> chr[i] = i;
    }
    sb5013_apply();
    if (!num_1k_VROM_pages) {
        chr_8k(0);
    }
}

/**
 * Mapper 117 (Future Media: 蠟筆小新, Crayon Shin-chan...; Mesen 2's Mapper117 to compare): $8000-$8003 the 8 kB PRG
 * banks (the last 32 kB at power on), $A000-$A007 the 1 kB CHR banks, $D000 bit 0 the mirroring (horizontal when
 * set), the IRQ: $C001 the reload, $C003 loads the counter and arms it, $C002 acknowledges, $E000 bit 0 the enable
 * (and acknowledges). The counter counts the PPU's A12 rises while enabled, armed and not 0, the IRQ when it reaches 0
 * (disarmed); as the J.Y. boards' line source, one at each rendering line's dot 260. State in the register page
 * (+0x27c-+0x287)
 */
typedef struct {
    uint8_t counter, reload, enabled, armed, next_line_set, pad;
    uint16_t next_line;
    uint32_t timer_base;
} m117_state_t;
#define M117 ((m117_state_t*)((uint8_t*)cpu_registers + 0x27c))

static void m117_line_start(void) {
    int32_t to_end = (int32_t)(ppu_dot_cpu_cycle(261, 340) - M117 -> timer_base);
    int32_t position = 262 * 341 - 1 - ((to_end > 0) ? to_end * 3 : 0);
    if (position < 0) {
        position = 0;
    }
    M117 -> next_line = jy_next_line_after((uint32_t)position / 341, (uint32_t)position % 341);
    M117 -> next_line_set = 1;
}

static int m117_counting(void) {
    const m117_state_t* m = M117;
    return m -> enabled && m -> armed && m -> counter;
}

static void m117_cpu_run(uint32_t cycles) {
    m117_state_t* m = M117;
    m -> timer_base += cycles;
    if (!m117_counting()) {
        return;
    }
    if (!m -> next_line_set) {
        m117_line_start();
    }
    for (uint32_t n = 0; (n < 300) && m117_counting(); n++) {
        uint32_t line = m -> next_line;
        uint32_t at = ppu_dot_cpu_cycle(line, 260);
        if ((int32_t)(at - m -> timer_base) > 0) {
            break;
        }
        if (((const _PPU_registers_t*)((const uint8_t*)cpu_registers + 32)) -> PPUMASK & 0x18) {
            if (!--m -> counter) {
                m -> armed = 0;
                mapper_cpu_timer = 0;
                cpu_irq_raise_at(CPU_IRQ_MAPPER, at);
            }
        }
        m -> next_line = (line == 239) ? 261 : ((line == 261) ? 0 : line + 1);
    }
}

static uint32_t m117_cycles_to_fire(void) {
    if (!m117_counting()) {
        return 0xffffffff;
    }
    if (!M117 -> next_line_set) {
        m117_line_start();
    }
    int32_t cycles = (int32_t)(ppu_dot_cpu_cycle(M117 -> next_line, 260) - M117 -> timer_base);
    return (cycles > 0) ? (uint32_t)cycles : 1;
}

static void m117_write(uint16_t address, uint8_t data) {
    m117_state_t* m = M117;
    if ((address & 0xf000) == 0x8000) {
        if (address <= 0x8003) {
            set_6502_bank_pointer(address & 3, prg_page(data));
        }
        return;
    }
    if ((address & 0xf000) == 0xa000) {
        if ((address <= 0xa007) && !chr_1k_mapped(address & 7, data)) {
            ppu_sync();
            chr_1k(address & 7, data);
        }
        return;
    }
    if (address == 0xd000) {
        ppu_sync();
        set_ppu_nametable_mirroring((data & 1) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
        return;
    }
    if ((address != 0xc001) && (address != 0xc002) && (address != 0xc003) && (address != 0xe000)) {
        return;
    }
    if (mapper_cpu_timer) {
        mapper_cpu_run(cpu_cycle_now);
    }
    switch (address) {
        case 0xc001: m -> reload = data; break;
        case 0xc002: cpu_irq_clear(CPU_IRQ_MAPPER); break;
        case 0xc003: m -> counter = m -> reload; m -> armed = 1; break;
        case 0xe000: m -> enabled = data & 1; cpu_irq_clear(CPU_IRQ_MAPPER); break;
    }
    if (!mapper_cpu_timer) {
        m -> timer_base = cpu_cycle_now;
    }
    counter_timer_start();
    m -> next_line_set = 0;
    mapper_cpu_timer = m117_counting();
    cpu_cut_batch_for_event(mapper_cycles_to_event());
}

__attribute__((cold, noinline)) static void m117_init() {
    memset(M117, 0, sizeof(m117_state_t));
    prg_32k(num_8k_ROM_pages / 4 - 1);
    chr_8k(0);
}

/**
 * Mapper 106 (Super Mario Bros. 3's bootleg, 0.5 / 1 MB; Mesen 2's Mapper106 to compare): $8000-$FFFF by address bits
 * 0-3: 0-7 the 1 kB CHR banks (0 / 2 even, 1 / 3 odd), 8 / B the 8 kB PRG banks at $8000 / $E000 (bits 0-3 with
 * bit 4 set), 9 / A the ones at $A000 / $C000 (bits 0-4); D stops the counter (cleared, the IRQ dropped), E / F its
 * low / high byte (F starts it): it counts CPU cycles up, the IRQ and its stop when it wraps to 0. State in the
 * register page (+0x288-+0x28a)
 */
typedef struct {
    uint16_t counter;
    uint8_t enabled;
} m106_state_t;
#define M106 ((m106_state_t*)((uint8_t*)cpu_registers + 0x288))

static void m106_cpu_run(uint32_t cycles) {
    m106_state_t* m = M106;
    if (!m -> enabled) {
        return;
    }
    if (cycles >= 0x10000u - m -> counter) {
        m -> counter = 0;
        m -> enabled = 0;
        mapper_cpu_timer = 0;
        mapper_irq_flag |= CPU_IRQ_MAPPER;
        cpu_irq_update_line();
    } else {
        m -> counter += cycles;
    }
}

static uint32_t m106_cycles_to_fire(void) {
    return M106 -> enabled ? (0x10000u - M106 -> counter) : 0xffffffff;
}

static void m106_write(uint16_t address, uint8_t data) {
    m106_state_t* m = M106;
    uint32_t reg = address & 0x0f;
    if (reg < 8) {
        uint32_t bank = (reg >= 4) ? data : ((reg & 1) ? (data | 1) : (data & 0xfe));
        if (!chr_1k_mapped(reg, bank)) {
            ppu_sync();
            chr_1k(reg, bank);
        }
        return;
    }
    switch (reg) {
        case 0x8: case 0xb:
            set_6502_bank_pointer((reg == 8) ? 0 : 3, prg_page((data & 0x0f) | 0x10));
            return;
        case 0x9: case 0xa:
            set_6502_bank_pointer(reg - 8, prg_page(data & 0x1f));
            return;
        case 0xd: case 0xe: case 0xf:
            break;
        default:
            return;
    }
    if (mapper_cpu_timer) {
        mapper_cpu_run(cpu_cycle_now);
    }
    counter_timer_start();
    if (reg == 0xd) {
        m -> enabled = 0;
        m -> counter = 0;
        cpu_irq_clear(CPU_IRQ_MAPPER);
    } else if (reg == 0xe) {
        m -> counter = (m -> counter & 0xff00) | data;
    } else {
        m -> counter = (m -> counter & 0x00ff) | (data << 8);
        m -> enabled = 1;
    }
    mapper_cpu_timer = m -> enabled;
    cpu_cut_batch_for_event(mapper_cycles_to_event());
}

__attribute__((cold, noinline)) static void m106_init() {
    memset(M106, 0, sizeof(m106_state_t));
    for (uint32_t i = 0; i < 4; i++) {
        set_6502_bank_pointer(i, prg_page(num_8k_ROM_pages - 1));
    }
    chr_8k(0);
}

/**
 * Mapper 35 (J.Y.'s Warioland II (JY039)...; Mesen 2's Mapper35 to compare): writes decoded with $F007: $8000-$8003 the
 * 8 kB PRG banks (the last at $E000 at power on), $9000-$9007 the 1 kB CHR banks, $D001 bit 0 the mirroring
 * (horizontal when set); the IRQ: $C005 the counter, $C003 on, $C002 off (acknowledged). The counter counts the PPU's
 * A12 rises down while on, the IRQ and its stop at 0; as the J.Y. boards' line source, one at each rendering line's
 * dot 260. State in the register page (+0x28c-+0x297)
 */
typedef struct {
    uint8_t counter, enabled, next_line_set, pad;
    uint16_t next_line;
    uint16_t pad2;
    uint32_t timer_base;
} m35_state_t;
#define M35 ((m35_state_t*)((uint8_t*)cpu_registers + 0x28c))

static void m35_line_start(void) {
    int32_t to_end = (int32_t)(ppu_dot_cpu_cycle(261, 340) - M35 -> timer_base);
    int32_t position = 262 * 341 - 1 - ((to_end > 0) ? to_end * 3 : 0);
    if (position < 0) {
        position = 0;
    }
    M35 -> next_line = jy_next_line_after((uint32_t)position / 341, (uint32_t)position % 341);
    M35 -> next_line_set = 1;
}

static void m35_cpu_run(uint32_t cycles) {
    m35_state_t* m = M35;
    m -> timer_base += cycles;
    if (!m -> enabled) {
        return;
    }
    if (!m -> next_line_set) {
        m35_line_start();
    }
    for (uint32_t n = 0; (n < 300) && m -> enabled; n++) {
        uint32_t line = m -> next_line;
        uint32_t at = ppu_dot_cpu_cycle(line, 260);
        if ((int32_t)(at - m -> timer_base) > 0) {
            break;
        }
        if (((const _PPU_registers_t*)((const uint8_t*)cpu_registers + 32)) -> PPUMASK & 0x18) {
            if (!--m -> counter) {
                m -> enabled = 0;
                mapper_cpu_timer = 0;
                cpu_irq_raise_at(CPU_IRQ_MAPPER, at);
            }
        }
        m -> next_line = (line == 239) ? 261 : ((line == 261) ? 0 : line + 1);
    }
}

static uint32_t m35_cycles_to_fire(void) {
    if (!M35 -> enabled) {
        return 0xffffffff;
    }
    if (!M35 -> next_line_set) {
        m35_line_start();
    }
    int32_t cycles = (int32_t)(ppu_dot_cpu_cycle(M35 -> next_line, 260) - M35 -> timer_base);
    return (cycles > 0) ? (uint32_t)cycles : 1;
}

static void m35_write(uint16_t address, uint8_t data) {
    m35_state_t* m = M35;
    uint32_t reg = address & 0xf007;
    if ((reg & 0xf000) == 0x8000) {
        if (reg <= 0x8003) {
            set_6502_bank_pointer(reg & 3, prg_page(data));
        }
        return;
    }
    if ((reg & 0xf000) == 0x9000) {
        if (!chr_1k_mapped(reg & 7, data)) {
            ppu_sync();
            chr_1k(reg & 7, data);
        }
        return;
    }
    if (reg == 0xd001) {
        ppu_sync();
        set_ppu_nametable_mirroring((data & 1) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
        return;
    }
    if ((reg != 0xc002) && (reg != 0xc003) && (reg != 0xc005)) {
        return;
    }
    if (mapper_cpu_timer) {
        mapper_cpu_run(cpu_cycle_now);
    }
    if (reg == 0xc002) {
        m -> enabled = 0;
        cpu_irq_clear(CPU_IRQ_MAPPER);
    } else if (reg == 0xc003) {
        m -> enabled = 1;
    } else {
        m -> counter = data;
    }
    if (!mapper_cpu_timer) {
        m -> timer_base = cpu_cycle_now;
    }
    counter_timer_start();
    m -> next_line_set = 0;
    mapper_cpu_timer = m -> enabled;
    cpu_cut_batch_for_event(mapper_cycles_to_event());
}

__attribute__((cold, noinline)) static void m35_init() {
    memset(M35, 0, sizeof(m35_state_t));
    set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
    chr_8k(0);
}

/**
 * Mapper 183 (Shui Guan Pipe; Mesen 2's Mapper183 to compare): $6800-$6FFF (board_6000) the address's bits 0-5 the 8 kB
 * PRG bank at $6000; writes decoded with $F80C: $8800 / $A800 / $A000 the 8 kB banks at $8000 / $A000 / $C000 (the last
 * at $E000), $9800 bits 0-1 the mirroring (V, H, one screen low / high), $B000-$E00C the 1 kB CHR banks' nibbles
 * (address bit 2 the high one; bits 3 and 11-14 the slot); the IRQ: $F000 / $F004 the counter's nibbles, $F008 on
 * (not 0; a write also acknowledges, and off clears the prescaler): every 114 CPU cycles the counter counts up, the
 * IRQ when it wraps to 0. State in the register page (+0x298-+0x2a3)
 */
typedef struct {
    uint8_t chr[8];
    uint8_t prg, counter, scaler, enabled;
} m183_state_t;
#define M183 ((m183_state_t*)((uint8_t*)cpu_registers + 0x298))

static void m183_cpu_run(uint32_t cycles) {
    m183_state_t* m = M183;
    if (!m -> enabled) {
        return;
    }
    uint32_t total = m -> scaler + cycles, steps = total / 114;
    m -> scaler = (uint8_t)(total % 114);
    if (steps >= 256u - m -> counter) {
        m -> counter = (uint8_t)(m -> counter + steps);
        mapper_irq_flag |= CPU_IRQ_MAPPER;
        cpu_irq_update_line();
    } else {
        m -> counter += steps;
    }
}

static uint32_t m183_cycles_to_fire(void) {
    const m183_state_t* m = M183;
    return m -> enabled ? ((114u - m -> scaler) + 114u * (255u - m -> counter)) : 0xffffffff;
}

static void m183_write(uint16_t address, uint8_t data) {
    m183_state_t* m = M183;
    if (address < 0x8000) {
        if ((address & 0xf800) == 0x6800) {
            m -> prg = address & 0x3f;
            set_wram_bank_pointer(prg_page(m -> prg));
        }
        return;
    }
    uint32_t reg = address & 0xf80c;
    if ((reg >= 0xb000) && (reg <= 0xe00c)) {
        uint32_t slot = ((((address >> 11) - 6) | (address >> 3)) & 7);
        m -> chr[slot] = (address & 4) ? ((m -> chr[slot] & 0x0f) | (data << 4)) : ((m -> chr[slot] & 0xf0) | (data & 0x0f));
        ppu_sync();
        chr_1k(slot, m -> chr[slot]);
        return;
    }
    switch (reg) {
        case 0x8800: set_6502_bank_pointer(0, prg_page(data)); return;
        case 0xa800: set_6502_bank_pointer(1, prg_page(data)); return;
        case 0xa000: set_6502_bank_pointer(2, prg_page(data)); return;
        case 0x9800: {
            static const uint8_t mirrorings[4] FRNES_COLD_RODATA = { NAMETABLE_MIRRORING_VERTICAL, NAMETABLE_MIRRORING_HORIZONTAL,
                NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW, NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH };
            ppu_sync();
            set_ppu_nametable_mirroring(mirrorings[data & 3]);
            return;
        }
        case 0xf000: case 0xf004: case 0xf008:
            break;
        default:
            return;
    }
    if (mapper_cpu_timer) {
        mapper_cpu_run(cpu_cycle_now);
    }
    counter_timer_start();
    if (reg == 0xf000) {
        m -> counter = (m -> counter & 0xf0) | (data & 0x0f);
    } else if (reg == 0xf004) {
        m -> counter = (m -> counter & 0x0f) | (data << 4);
    } else {
        m -> enabled = (data != 0);
        if (!m -> enabled) {
            m -> scaler = 0;
        }
        cpu_irq_clear(CPU_IRQ_MAPPER);
    }
    mapper_cpu_timer = m -> enabled;
    cpu_cut_batch_for_event(mapper_cycles_to_event());
}

__attribute__((cold, noinline)) static void m183_init() {
    memset(M183, 0, sizeof(m183_state_t));
    mapper_6000_mode = MAPPER_6000_REGISTERS;
    board_6000 = 1;
    if (WRAM == NULL) {
        resize_wram(0x2000);
    }
    set_wram_bank_pointer(prg_page(0));
    set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
    chr_8k(0);
}

/**
 * Mapper 253 (Waixing's VRC4 clone: Dragon Ball Z 強襲! サイヤ人's localization...; the nesdev wiki's, Mesen 2's Mapper253
 * to compare): a VRC4 with its A0 / A1 on CPU A2 / A3 (VRC4e) and 2 kB of CHR RAM: registers by address bits 12-15
 * and 2-3: $8000 / $A000 the 8 kB PRG banks ($9008 bit 1 swapping $8000 with the second last bank at $C000), $9000
 * bits 0-1 the mirroring (V, H, one screen low / high), $B000-$E00C the 1 kB CHR banks' nibbles (bit 3 the slot's
 * low bit, bit 2 the high nibble: bits 4-8), CHR banks 4 and 5 the CHR RAM's pages (not while slot 0's low byte last
 * became $88: $C8 brings the RAM back); the VRC IRQ: $F000 / $F004 the latch's nibbles, $F008 the control, $F00C the
 * acknowledge, counting CPU cycles (M) or a 341 dot prescaler's clocks up to $FF. State in the register page
 * (+0x2d0-+0x2eb)
 */
typedef struct {
    uint8_t prg[2], mode, force_rom;
    uint8_t chr_low[8], chr_high[8];
    uint8_t latch, counter, control, pad;
    int16_t prescaler;
    uint16_t pad2;
} m253_state_t;
#define M253 ((m253_state_t*)((uint8_t*)cpu_registers + 0x2d0))

static void m253_prg(void) {
    const m253_state_t* m = M253;
    uint32_t second_last = num_8k_ROM_pages - 2;
    set_6502_bank_pointer(0, prg_page((m -> mode & 2) ? second_last : m -> prg[0]));
    set_6502_bank_pointer(1, prg_page(m -> prg[1]));
    set_6502_bank_pointer(2, prg_page((m -> mode & 2) ? m -> prg[0] : second_last));
    set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
}

static void m253_chr(uint32_t slot) {
    const m253_state_t* m = M253;
    uint32_t low = m -> chr_low[slot];
    if (((low == 4) || (low == 5)) && !m -> force_rom && num_1k_VRAM_pages) {
        mapper_chr_ram_slots |= 1 << slot;
        set_ppu_pattern_table_bank_pointer(slot, chr_ram_page(low & 1));
        return;
    }
    mapper_chr_ram_slots &= ~(1 << slot);
    chr_1k(slot, low | ((uint32_t)m -> chr_high[slot] << 8));
}

static void m253_clock(uint32_t clocks) {
    m253_state_t* m = M253;
    while (clocks) {
        uint32_t to_ff = 0xffu - m -> counter;
        if (clocks <= to_ff) {
            m -> counter += clocks;
            return;
        }
        clocks -= to_ff + 1;
        m -> counter = m -> latch;
        mapper_irq_flag |= CPU_IRQ_MAPPER;
        cpu_irq_update_line();
    }
}

static void m253_cpu_run(uint32_t cycles) {
    m253_state_t* m = M253;
    if (!(m -> control & 2)) {
        return;
    }
    if (m -> control & 4) {
        m253_clock(cycles);
        return;
    }
    int32_t p = m -> prescaler - 3 * (int32_t)cycles;
    uint32_t clocks = 0;
    if (p <= 0) {
        clocks = 1 + (uint32_t)(-p) / 341;
        p += 341 * (int32_t)clocks;
    }
    m -> prescaler = (int16_t)p;
    m253_clock(clocks);
}

static uint32_t m253_cycles_to_fire(void) {
    const m253_state_t* m = M253;
    if (!(m -> control & 2)) {
        return 0xffffffff;
    }
    uint32_t clocks = 0x100u - m -> counter;
    if (m -> control & 4) {
        return clocks;
    }
    // (after its first clock the prescaler is back in 339-341, as mapper.c's vrc_irq_cycles_to_event has it)
    int32_t p = (m -> prescaler > 0) ? m -> prescaler : 1;
    return ((uint32_t)p + 341 * (clocks - 1) + 2) / 3;
}

static void m253_irq_write(uint32_t reg, uint8_t data) {
    m253_state_t* m = M253;
    if (reg < 2) {
        // (253's clone drops the IRQ on these too: Mesen 2's and FCEUX's boards)
        m -> latch = (reg == 0) ? ((m -> latch & 0xf0) | (data & 0x0f)) : (uint8_t)((m -> latch & 0x0f) | (data << 4));
        if (board_number == 253) {
            cpu_irq_clear(CPU_IRQ_MAPPER);
        }
        return;
    }
    if (mapper_cpu_timer) {
        mapper_cpu_run(cpu_cycle_now);
    }
    counter_timer_start();
    if (reg == 2) {
        m -> control = data & 7;
        if (data & 2) {
            m -> counter = m -> latch;
            m -> prescaler = 341;
        }
    } else if (board_number != 362) {
        m -> control = (m -> control & ~2) | ((m -> control & 1) << 1);
    }
    cpu_irq_clear(CPU_IRQ_MAPPER);
    mapper_cpu_timer = (m -> control & 2) != 0;
    cpu_cut_batch_for_event(mapper_cycles_to_event());
}

static void m253_write(uint16_t address, uint8_t data) {
    m253_state_t* m = M253;
    uint32_t reg = (address >> 2) & 3;
    switch (address & 0xf000) {
        case 0x8000: case 0xa000:
            m -> prg[(address >> 13) & 1] = data & 0x1f;
            m253_prg();
            break;
        case 0x9000:
            if (reg == 0) {
                static const uint8_t mirrorings[4] FRNES_COLD_RODATA = { NAMETABLE_MIRRORING_VERTICAL,
                    NAMETABLE_MIRRORING_HORIZONTAL, NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW, NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH };
                ppu_sync();
                set_ppu_nametable_mirroring(mirrorings[data & 3]);
            } else if (reg == 2) {
                m -> mode = data & 3;
                m253_prg();
            }
            break;
        case 0xb000: case 0xc000: case 0xd000: case 0xe000: {
            uint32_t slot = ((((uint32_t)address >> 12) - 0xb) << 1) | (reg >> 1);
            ppu_sync();
            if (reg & 1) {
                m -> chr_low[slot] = (uint8_t)((m -> chr_low[slot] & 0x0f) | (data << 4));
                m -> chr_high[slot] = (data >> 4) & 1;
            } else {
                m -> chr_low[slot] = (m -> chr_low[slot] & 0xf0) | (data & 0x0f);
            }
            if (slot == 0) {
                if (m -> chr_low[0] == 0xc8) {
                    m -> force_rom = 0;
                } else if (m -> chr_low[0] == 0x88) {
                    m -> force_rom = 1;
                }
                for (uint32_t i = 0; i < 8; i++) {
                    m253_chr(i);
                }
            } else {
                m253_chr(slot);
            }
        } break;
        case 0xf000:
            m253_irq_write(reg, data);
            break;
    }
}

/**
 * Mapper 529 (the T-230 / YY0807 / J-2148 boards: Datach Dragon Ball Z's and Dragon Ball Z Gaiden's bootlegs; the
 * nesdev wiki's, Mesen 2's T230 to compare), on 253's state and VRC IRQ: a VRC4e clone with $800x without effect and
 * $A00x the 16 kB PRG bank at $8000 ($9002 bit 1 swapping its first half with the second last 8 kB at $C000); $9000 the
 * mirroring; $B000-$E003 the 1 kB CHR banks' nibbles on the CHR ROM board (the T-230's 8 kB of CHR RAM unbanked);
 * $F000-$F003 the IRQ (VRC4's, its latch writes not acknowledging). The YY0807's 93C56 EEPROM is not emulated
 */
static void m529_prg(void) {
    const m253_state_t* m = M253;
    uint32_t bank = (uint32_t)m -> prg[0] << 1, second_last = num_8k_ROM_pages - 2;
    set_6502_bank_pointer(0, prg_page((m -> mode & 2) ? second_last : bank));
    set_6502_bank_pointer(1, prg_page(bank | 1));
    set_6502_bank_pointer(2, prg_page((m -> mode & 2) ? bank : second_last));
    set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
}

static void m529_write(uint16_t address, uint8_t data) {
    m253_state_t* m = M253;
    // (the chip's A0 / A1 from any of CPU A0 / A2 / A4 and A1 / A3 / A5, as Mesen 2 decodes the clone)
    uint32_t reg = ((address & 0x2a) ? 2 : 0) | ((address & 0x15) ? 1 : 0);
    switch (address & 0xf000) {
        case 0x9000:
            if (reg < 2) {
                static const uint8_t mirrorings[4] FRNES_COLD_RODATA = { NAMETABLE_MIRRORING_VERTICAL,
                    NAMETABLE_MIRRORING_HORIZONTAL, NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW, NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH };
                ppu_sync();
                set_ppu_nametable_mirroring(mirrorings[data & 3]);
            } else {
                m -> mode = data & 2;
                m529_prg();
            }
            break;
        case 0xa000:
            m -> prg[0] = data & 0x1f;
            m529_prg();
            break;
        case 0xb000: case 0xc000: case 0xd000: case 0xe000: {
            if (!num_1k_VROM_pages) {
                break;
            }
            uint32_t slot = ((((uint32_t)address >> 12) - 0xb) << 1) | (reg >> 1);
            if (reg & 1) {
                m -> chr_high[slot] = data & 0x1f;
            } else {
                m -> chr_low[slot] = data & 0x0f;
            }
            ppu_sync();
            chr_1k(slot, m -> chr_low[slot] | ((uint32_t)m -> chr_high[slot] << 4));
        } break;
        case 0xf000:
            m253_irq_write(reg, data);
            break;
    }
}

/**
 * Mapper 362 (J.Y.'s 830506C: 1995 Super HiK 4-in-1 JY-005; the nesdev wiki's), on 253's state and VRC IRQ: a VRC4 clone
 * wired as VRC4f (A0 / A1), each game 128 kB of PRG and CHR: the CHR banks' high registers' bits 0-2 CHR A14-A16 and
 * bits 3-4 the 128 kB outer PRG / CHR bank, for the PRG the $0000 slot's (the games keep them all the same; the board
 * switches the PRG by the slot the PPU reads); $F003 acknowledges without moving A to E
 */
static void m362_prg(void) {
    const m253_state_t* m = M253;
    uint32_t outer = ((m -> chr_high[0] >> 3) & 3) << 4, second_last = outer | 14;
    set_6502_bank_pointer(0, prg_page((m -> mode & 2) ? second_last : (outer | (m -> prg[0] & 0x0f))));
    set_6502_bank_pointer(1, prg_page(outer | (m -> prg[1] & 0x0f)));
    set_6502_bank_pointer(2, prg_page((m -> mode & 2) ? (outer | (m -> prg[0] & 0x0f)) : second_last));
    set_6502_bank_pointer(3, prg_page(outer | 15));
}

static void m362_write(uint16_t address, uint8_t data) {
    m253_state_t* m = M253;
    uint32_t reg = address & 3;
    switch (address & 0xf000) {
        case 0x8000: case 0xa000:
            m -> prg[(address >> 13) & 1] = data & 0x1f;
            m362_prg();
            break;
        case 0x9000:
            if (reg < 2) {
                static const uint8_t mirrorings[4] FRNES_COLD_RODATA = { NAMETABLE_MIRRORING_VERTICAL,
                    NAMETABLE_MIRRORING_HORIZONTAL, NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW, NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH };
                ppu_sync();
                set_ppu_nametable_mirroring(mirrorings[data & 3]);
            } else {
                m -> mode = data & 2;
                m362_prg();
            }
            break;
        case 0xb000: case 0xc000: case 0xd000: case 0xe000: {
            uint32_t slot = ((((uint32_t)address >> 12) - 0xb) << 1) | (reg >> 1);
            if (reg & 1) {
                m -> chr_high[slot] = data & 0x1f;
            } else {
                m -> chr_low[slot] = data & 0x0f;
            }
            ppu_sync();
            chr_1k(slot, ((uint32_t)((m -> chr_high[slot] >> 3) & 3) << 7) | ((uint32_t)(m -> chr_high[slot] & 7) << 4) | m -> chr_low[slot]);
            if ((slot == 0) && (reg & 1)) {
                m362_prg();
            }
        } break;
        case 0xf000:
            m253_irq_write(reg, data);
            break;
    }
}

__attribute__((cold, noinline)) static void m362_init() {
    memset(M253, 0, sizeof(m253_state_t));
    M253 -> prescaler = 341;
    m362_prg();
    for (uint32_t i = 0; i < 8; i++) {
        chr_1k(i, 0);
    }
}

__attribute__((cold, noinline)) static void m529_init() {
    memset(M253, 0, sizeof(m253_state_t));
    M253 -> prescaler = 341;
    m529_prg();
    chr_8k(0);
}

__attribute__((cold, noinline)) static void m253_init() {
    memset(M253, 0, sizeof(m253_state_t));
    M253 -> prescaler = 341;
    m253_prg();
    for (uint32_t i = 0; i < 8; i++) {
        m253_chr(i);
    }
}

/*
 * Front Fareast's Super Magic Card in its 4M mode (mapper 12 submapper 1: the Magic Card 4M disk images, F6xxx / F8xxx
 * and SM6001 / SM8001; the nesdev wiki's Super Magic Card registers). The copier's 512 kB of PRG DRAM: smc_cartridge
 * (cartridge_convert) makes the file's PRG ROM and CHR ROM one 512 kB PRG ROM, the CHR at $40000 as the copier loads
 * it (its trainer copies it to the CHR RAM), with the copier's 32 kB of WRAM. Registers:
 *   $42FC-$42FF: address bit 0 and bit 4 the mirroring (one screen, page bit 4 / vertical or horizontal by bit 4),
 *     address bit 1 the PRG write protect (and the latch at $8000-$FFFF), bits 5-7 the latch's mode (UNROM, UN1ROM +
 *     CHR, UOROM, reverse UOROM + CHR, GNROM, CNROM-256, CNROM-128, NROM-256)
 *   $43FC-$43FF: address bit 0 clear the 2M / 4M modes over the latch's, bits 0-1 the 8 kB CHR bank (the latch's too)
 *   $8000 / $A000 / $C000 / $E000: 2M mode's 8 kB banks (bits 2-7) and the 8 kB CHR bank (bits 0-1); $4504-$4507: 4M
 *     mode's (bits 0-5), 2M mode's while 4M mode is off
 *   $4500: bit 0 1 kB CHR banks ($4510-$4517), bit 3 the IRQ counter's clock (M2 / PPU A12 rises), bits 4-5 the WRAM bank
 *   $4501-$4503: the IRQ counter as Front Fareast's other boards (ffe_write_low): up, the IRQ at its wrap
 * The A12 rises a line at a time, as J.Y.'s (jy_line_ticks): an event of the board timer at dot 260 of each rendered
 * line. At power on the trainer runs first (as a JSR $7003, then JMP ($FFFC): smc_game_start). Not emulated: the nametables from CHR RAM ($4500 bit 1 clear), the MMC4 mode,
 * the GUI mode, the DRAM's writes, $5000-$5FFF's copies of the registers, $4500 / $4501's status reads. State in the register page
 * (+0x160-+0x17f; the counter in FFE's, +0xcc), saved with a state
 */
/*
 * Bung's Super Game Doctor 2M / 4M (mapper 561) and Venus' Turbo Game Doctor 4+ / 6+ / 6M (562), Magic Card 2M clones
 * (the nesdev wiki's pages), on the Super Magic Card's code: its $42FC-$42FF (the submapper the latch's mode at power
 * on), the latch's modes 0-5, its 2M bank registers at $8000-$FFFF. Theirs: latch modes 6 and 7 two 8 kB banks at
 * $8000 / $A000 (bits 0-3 / 4-7) and the last 16 kB at $C000, the CHR bank kept. 561: $43FE / $43FF address bit 0
 * clear the 4M mode (the registers' 6 bits), bits 0-1 the CHR bank; $4100 / $4101 the IRQ counter's low / high byte (the
 * low byte latched, loaded with the $4101 write), up on M2, a non-zero $4101 starts it, a zero one stops it and
 * acknowledges the IRQ. 562: $43FE / $43FF address bit 0
 * clear the 2M mode (the registers' 4 bits, bit 6 PRG A17), bits 0-1 the CHR bank; $4411 bit 7 the 4M mode, bit 6 1 kB
 * CHR banks ($4400-$4407); $440D / $440C the IRQ's target: a $440C write with bit 7 clear stops and clears the counter
 * (the games' NMI and IRQ handlers), one with bit 7 set starts it from $8000 if it's stopped; it counts M2 up to
 * $FFFF, raising the IRQ at the target (Super Mario Bros. 3's and Dragon Ninja's code; the wiki leaves the start out). Their trainers: NES 2.0's miscellaneous ROM (a load
 * address, an initialization routine's or 0, the bytes), loaded into RAM or WRAM, the routine run before the reset
 * (gd_keep_trainer, smc_game_start). 8 kB of WRAM. The FDS RAM adapter's disk IRQ, which converted Magic Card games
 * time frames with: $4025 bit 7 on, an IRQ every 1792 master clocks (149 1/3 CPU cycles), $4024 writes acknowledge
 * it. Not emulated: the BIOS at $4800-$4FFF past three bytes the lock outs compare and $4420's bank read (gd_read_low: the values they compare), DRAM writes
 */
typedef struct {
    uint16_t target;
    uint8_t tgd_mode, a17;
    // (the FDS adapter's disk IRQ: on, and the time to its next one in thirds of a CPU cycle, GD_FDS_PERIOD apart)
    uint8_t fds, pad;
    uint16_t fds_left;
} gd_state_t;
#define GD_FDS_PERIOD 448
#define GD ((gd_state_t*)((uint8_t*)cpu_registers + 0x180))

typedef struct {
    uint8_t mode, control, banking, prg_banks;
    uint8_t prg[4], prg_2m[4];
    uint8_t chr[8];
    uint16_t next_line;
    uint8_t next_line_set, latch;
    uint32_t timer_base;
} smc_state_t;
#define SMC ((smc_state_t*)((uint8_t*)cpu_registers + 0x160))
// (the file's 8 kB PRG banks, for the power on banks: smc_cartridge's, until smc_init)
static uint8_t smc_file_prg_banks FRNES_COLD_BSS;

static uint32_t smc_board(void) {
    return ((board_number == 12) && (board_submapper == 1)) || (board_number == BOARD_SGD) || (board_number == BOARD_TGD);
}

// cartridge_convert: a mapper 12 / 1 file as the copier's DRAM, 512 kB of PRG ROM (the CHR ROM at $40000), 32 kB of
// CHR RAM and WRAM
static uint8_t* smc_cartridge(uint8_t* data, uint32_t* size) {
    if ((*size < 16) || memcmp(data, "NES\x1a", 4) || ((data[7] & 0x0c) != 0x08) ||
        ((((data[8] & 0x0f) << 8) | (data[7] & 0xf0) | (data[6] >> 4)) != 12) || ((data[8] >> 4) != 1)) {
        return data;
    }
    uint32_t trainer = (data[6] & 4) ? 512 : 0;
    uint32_t prg = (((data[9] & 0x0f) << 8) | data[4]) * 0x4000, chr = (((data[9] & 0xf0) << 4) | data[5]) * 0x2000;
    uint32_t chr_at = (prg <= 0x40000) ? 0x40000 : prg;
    if ((chr_at + chr > 0x80000) || (16 + trainer + prg + chr > *size)) {
        return data;
    }
    uint8_t* out = malloc(16 + trainer + 0x80000);
    if (out == NULL) {
        return data;
    }
    memset(out, 0xff, 16 + trainer + 0x80000);
    memcpy(out, data, 16 + trainer);
    memcpy(out + 16 + trainer, data + 16 + trainer, prg);
    memcpy(out + 16 + trainer + chr_at, data + 16 + trainer + prg, chr);
    out[4] = 0x20;
    out[5] = 0;
    out[9] = 0;
    out[10] = 0x09;
    if ((out[11] & 0x0f) < 0x09) {
        out[11] = (out[11] & 0xf0) | 0x09;
    }
    smc_file_prg_banks = (uint8_t)(prg / 0x2000);
    printf(" ...Super Magic Card 4M: %lu kB of PRG and %lu kB of CHR in its 512 kB of DRAM\n", (unsigned long)(prg / 1024),
        (unsigned long)(chr / 1024));
    free(data);
    *size = 16 + trainer + 0x80000;
    return out;
}

static void smc_apply(void) {
    const smc_state_t* s = SMC;
    uint32_t chr = s -> banking & 3;
    if ((board_number == BOARD_TGD) && ((GD -> tgd_mode & 0x80) || !(s -> banking & 0x10))) {
        // (the Turbo Game Doctor: 4M, the registers' 6 bits; 2M, 4 bits and $43FE's A17)
        uint32_t four_m = GD -> tgd_mode & 0x80;
        for (uint32_t i = 0; i < 4; i++) {
            set_6502_bank_pointer(i, prg_page(four_m ? (s -> prg_2m[i] & 0x3f) : ((s -> prg_2m[i] & 0x0f) | GD -> a17)));
        }
    } else if (!(s -> banking & 0x10) && (board_number != BOARD_TGD)) {
        const uint8_t* banks = ((s -> banking & 0x20) || (board_number == BOARD_SGD)) ? s -> prg_2m : s -> prg;
        for (uint32_t i = 0; i < 4; i++) {
            set_6502_bank_pointer(i, prg_page(banks[i] & 0x3f));
        }
    } else if ((board_number != 12) && ((s -> control >> 5) >= 6)) {
        // (the Game Doctors' modes 6 and 7: 8 kB at $8000 and $A000, the last 16 kB fixed, the CHR bank kept)
        set_6502_bank_pointer(0, prg_page(s -> latch & 0x0f));
        set_6502_bank_pointer(1, prg_page(s -> latch >> 4));
        prg_16k(2, 7);
    } else {
        // (the latch's modes: 16 kB / 32 kB banks)
        uint32_t l = s -> latch, first = 0, last = 7;
        switch (s -> control >> 5) {
            case 0: first = l & 7; chr = 0; break;
            case 1: first = (l >> 2) & 0x0f; break;
            case 2: first = l & 0x0f; last = 15; chr = 0; break;
            case 3: first = 15; last = l & 0x0f; chr = (l >> 4) & 3; break;
            case 4: first = ((l >> 4) & 3) * 2; last = first + 1; break;
            case 5: first = 6; break;
            case 6: first = 6; chr &= 1; break;
            case 7: first = 6; chr = 0; break;
        }
        prg_16k(0, first);
        prg_16k(2, last);
    }
    if ((board_number == BOARD_TGD) ? (GD -> tgd_mode & 0x40) : ((board_number == 12) && (s -> mode & 0x01))) {
        for (uint32_t i = 0; i < 8; i++) {
            chr_1k(i, s -> chr[i]);
        }
    } else {
        chr_8k(chr);
    }
    if ((WRAM != NULL) && (board_number == 12)) {
        uint32_t banks = WRAM_size / 0x2000;
        set_wram_bank_pointer(WRAM + (((s -> mode >> 4) & 3) % banks) * 0x2000);
    }
    uint32_t c = s -> control;
    set_ppu_nametable_mirroring((c & 1) ? ((c & 0x10) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL) :
        ((c & 0x10) ? NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH : NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW));
}

static uint32_t smc_line_source(void) {
    return FFE -> enabled && (SMC -> mode & 0x08);
}

// the PPU's position as J.Y.'s (jy_line_start)
static void smc_line_start(void) {
    int32_t to_end = (int32_t)(ppu_dot_cpu_cycle(261, 340) - SMC -> timer_base);
    int32_t position = 262 * 341 - 1 - ((to_end > 0) ? to_end * 3 : 0);
    if (position < 0) {
        position = 0;
    }
    SMC -> next_line = (uint16_t)jy_next_line_after((uint32_t)position / 341, (uint32_t)position % 341);
    SMC -> next_line_set = 1;
}

// 'rises' counts: 1 when the counter wrapped (the IRQ; the counter stops, as the M2 clock's)
static uint32_t smc_count(uint32_t rises) {
    ffe_state_t* f = FFE;
    if (board_number == BOARD_TGD) {
        // (up while negative: the target reached raises the IRQ, then on to $FFFF, where it stops)
        uint32_t to_target = (uint16_t)(GD -> target - f -> counter);
        uint32_t to_end = 0x10000u - f -> counter;
        uint32_t fired = (GD -> target & 0x8000) && to_target && (rises >= to_target);
        if (rises >= to_end) {
            f -> counter = 0xffff;
            f -> enabled = 0;
        } else {
            f -> counter += rises;
        }
        return fired;
    }
    if (rises >= 0x10000u - f -> counter) {
        f -> counter = 0;
        f -> enabled = 0;
        return 1;
    }
    f -> counter += rises;
    return 0;
}

static void smc_cpu_run(uint32_t cycles) {
    smc_state_t* s = SMC;
    s -> timer_base += cycles;
    if ((board_number != 12) && GD -> fds) {
        gd_state_t* g = GD;
        uint32_t thirds = cycles * 3;
        if (thirds >= g -> fds_left) {
            g -> fds_left = (uint16_t)(GD_FDS_PERIOD - (thirds - g -> fds_left) % GD_FDS_PERIOD);
            mapper_irq_flag |= CPU_IRQ_MAPPER;
            cpu_irq_update_line();
        } else {
            g -> fds_left = (uint16_t)(g -> fds_left - thirds);
        }
    }
    if (!FFE -> enabled) {
        return;
    }
    if (!(s -> mode & 0x08) || (board_number != 12)) {
        if (smc_count(cycles)) {
            mapper_cpu_timer = FFE -> enabled || ((board_number != 12) && GD -> fds);
            mapper_irq_flag |= CPU_IRQ_MAPPER;
            cpu_irq_update_line();
        }
        return;
    }
    if (!s -> next_line_set) {
        smc_line_start();
    }
    for (uint32_t n = 0; n < 300; n++) {
        uint32_t line = s -> next_line;
        uint32_t at = ppu_dot_cpu_cycle(line, 260);
        if ((int32_t)(at - s -> timer_base) > 0) {
            break;
        }
        const _PPU_registers_t* ppu = (const _PPU_registers_t*)((const uint8_t*)cpu_registers + 32);
        uint32_t sprites_high = (ppu -> PPUCTRL & PPUCTRL_SPRITE_SIZE) || (ppu -> PPUCTRL & 0x08);
        uint32_t rises = ((ppu -> PPUCTRL & 0x10) ? 34 : 0) + (sprites_high ? 8 : 0);
        s -> next_line = (uint16_t)((line == 239) ? 261 : ((line == 261) ? 0 : line + 1));
        if ((ppu -> PPUMASK & 0x18) && rises && smc_count(rises)) {
            mapper_cpu_timer = 0;
            cpu_irq_raise_at(CPU_IRQ_MAPPER, at);
            return;
        }
    }
}

static uint32_t smc_counter_cycles_to_fire(void);

static uint32_t smc_cycles_to_fire(void) {
    uint32_t cycles = smc_counter_cycles_to_fire();
    if ((board_number != 12) && GD -> fds) {
        uint32_t fds = (GD -> fds_left + 2) / 3;
        cycles = (fds < cycles) ? fds : cycles;
    }
    return cycles;
}

static uint32_t smc_counter_cycles_to_fire(void) {
    if (!FFE -> enabled) {
        return 0xffffffff;
    }
    if (board_number == BOARD_TGD) {
        uint32_t to_target = (uint16_t)(GD -> target - FFE -> counter);
        uint32_t to_end = 0x10000u - FFE -> counter;
        return ((GD -> target & 0x8000) && to_target && (to_target < to_end)) ? to_target : to_end;
    }
    if (!(SMC -> mode & 0x08) || (board_number != 12)) {
        return 0x10000 - FFE -> counter;
    }
    if (!SMC -> next_line_set) {
        smc_line_start();
    }
    int32_t cycles = (int32_t)(ppu_dot_cpu_cycle(SMC -> next_line, 260) - SMC -> timer_base);
    return (cycles > 0) ? (uint32_t)cycles : 1;
}

static void smc_irq_changed(void) {
    if (!mapper_cpu_timer) {
        SMC -> timer_base = cpu_cycle_now;
    }
    counter_timer_start();
    SMC -> next_line_set = 0;
    mapper_cpu_timer = FFE -> enabled || ((board_number != 12) && GD -> fds);
    cpu_cut_batch_for_event(mapper_cycles_to_event());
}

static void smc_irq_changed(void);

/*
 * The Game Doctors' BIOS at $4800-$4FFF (offsets $0800-$0FFF), read by games that lock out the other copiers: only the
 * bytes the dumps compare, from their code (open bus otherwise; no BIOS image): $4FE0 = $FE (Arctic, Poker 集錦, 寺尾の
 * どすこい大相撲, Dragon Unit), $4FF1 = $4C (Conflict, TwinBee 3, Batman...: 13 dumps), $4FFE = $78 (Dragon Unit). And
 * $4420, the 1 kB CHR bank the PPU is fetching from, as $44: the value Goemon Gaiden, Final Fantasy III and Madara
 * compare it with. 562's registers read back: $4400-$4407, $4411, the counter at $440C / $440D
 */
static uint8_t gd_read_low(uint16_t address, uint8_t open_bus) {
    if (board_number == BOARD_TGD) {
        // (the Turbo Game Doctor's readable registers: the 1 kB CHR banks, its mode, the IRQ counter)
        if ((address >= 0x4400) && (address <= 0x4407)) {
            return SMC -> chr[address & 7];
        }
        if ((address == 0x440c) || (address == 0x440d) || (address == 0x4411)) {
            if (mapper_cpu_timer) {
                mapper_cpu_run(cpu_cycle_now);
            }
            return (address == 0x4411) ? GD -> tgd_mode : (uint8_t)((address == 0x440c) ? (FFE -> counter >> 8) : FFE -> counter);
        }
    }
    switch (address) {
        case 0x4fe0: return 0xfe;
        case 0x4ff1: return 0x4c;
        case 0x4ffe: return 0x78;
        case 0x4420: return (board_number == BOARD_TGD) ? 0x44 : open_bus;
    }
    return open_bus;
}

// the Game Doctors' own $4100-$4411 registers: 1 when the banks change
static uint32_t gd_write_low(uint16_t address, uint8_t data) {
    smc_state_t* s = SMC;
    ffe_state_t* f = FFE;
    gd_state_t* g = GD;
    uint32_t irq = 0;
    if ((board_number == BOARD_SGD) && ((address == 0x4100) || (address == 0x4101))) {
        if (mapper_cpu_timer) {
            mapper_cpu_run(cpu_cycle_now);
        }
        if (address == 0x4100) {
            // (a latch the $4101 write loads with it: Fantasy Zone II writes $00 here ~300 times a frame while counting)
            g -> target = (g -> target & 0xff00) | data;
            irq = 0;
        } else {
            f -> counter = (uint16_t)((data << 8) | (g -> target & 0xff));
            f -> enabled = (data != 0);
            if (!data) {
                cpu_irq_clear(CPU_IRQ_MAPPER);
            }
        }
        if (address == 0x4101) {
            irq = 1;
        }
    } else if ((address == 0x4024) || (address == 0x4025)) {
        // (the FDS adapter's disk IRQ)
        if (mapper_cpu_timer) {
            mapper_cpu_run(cpu_cycle_now);
        }
        if (address == 0x4025) {
            if ((data & 0x80) && !g -> fds) {
                g -> fds_left = GD_FDS_PERIOD;
            }
            g -> fds = data & 0x80;
        }
        cpu_irq_clear(CPU_IRQ_MAPPER);
        irq = 1;
    } else if (board_number != BOARD_TGD) {
        return 0;
    } else if ((address >= 0x4400) && (address <= 0x4407)) {
        s -> chr[address & 7] = data;
        return 1;
    } else if (address == 0x4411) {
        g -> tgd_mode = data;
        return 1;
    } else if ((address == 0x440c) || (address == 0x440d)) {
        if (mapper_cpu_timer) {
            mapper_cpu_run(cpu_cycle_now);
        }
        if (address == 0x440d) {
            g -> target = (g -> target & 0xff00) | data;
        } else if (data & 0x80) {
            // (a negative target: the counter, stopped, starts from $8000; running, it keeps on: a second IRQ)
            g -> target = (g -> target & 0x00ff) | (data << 8);
            if (!f -> enabled) {
                f -> counter = 0x8000;
                f -> enabled = 1;
            }
        } else {
            // (reset and stopped: the games' NMI and IRQ handlers)
            g -> target = (g -> target & 0x00ff) | (data << 8);
            f -> counter = 0;
            f -> enabled = 0;
        }
        cpu_irq_clear(CPU_IRQ_MAPPER);
        irq = 1;
    } else {
        return 0;
    }
    if (irq) {
        smc_irq_changed();
    }
    return 0;
}

static void smc_write_low(uint16_t address, uint8_t data) {
    smc_state_t* s = SMC;
    ffe_state_t* f = FFE;
    if ((address & 0xfffc) == 0x42fc) {
        s -> control = (data & 0xf0) | (address & 3);
    } else if (((address & 0xfffc) == 0x43fc) && ((board_number == 12) || (address & 2))) {
        // (the Game Doctors': $43FE / $43FF, address bit 0 the mode's enable; 562's bit 6 PRG A17)
        s -> banking = (uint8_t)(((board_number == 12) ? ((address & 3) << 4) : ((address & 1) << 4)) | (data & 3));
        if (board_number == BOARD_TGD) {
            GD -> a17 = (data & 0x40) >> 2;
        }
    } else if (board_number != 12) {
        if (!gd_write_low(address, data)) {
            return;
        }
    } else if (address == 0x4500) {
        if (mapper_cpu_timer) {
            mapper_cpu_run(cpu_cycle_now);
        }
        s -> mode = data;
        smc_irq_changed();
    } else if ((address >= 0x4501) && (address <= 0x4503)) {
        if (mapper_cpu_timer) {
            mapper_cpu_run(cpu_cycle_now);
        }
        if (address == 0x4501) {
            f -> enabled = 0;
        } else if (address == 0x4502) {
            f -> counter = (f -> counter & 0xff00) | data;
        } else {
            f -> counter = (f -> counter & 0x00ff) | (data << 8);
            f -> enabled = 1;
        }
        cpu_irq_clear(CPU_IRQ_MAPPER);
        smc_irq_changed();
        return;
    } else if ((address >= 0x4504) && (address <= 0x4507)) {
        if ((s -> banking & 0x30) == 0x00) {
            s -> prg[address - 0x4504] = data;
        } else {
            s -> prg_2m[address - 0x4504] = data;
        }
    } else if ((address >= 0x4510) && (address <= 0x4517)) {
        s -> chr[address - 0x4510] = data;
    } else {
        return;
    }
    ppu_sync();
    smc_apply();
}

// $8000-$FFFF: 2M mode's bank registers, and the latch while the DRAM is write protected (the DRAM's writes aren't
// emulated)
static void smc_write(uint16_t address, uint8_t data) {
    smc_state_t* s = SMC;
    s -> prg_2m[(address >> 13) & 3] = data >> 2;
    s -> banking = (s -> banking & ~3) | (data & 3);
    if (s -> control & 2) {
        s -> latch = data;
    }
    ppu_sync();
    smc_apply();
}

__attribute__((cold, noinline)) static void smc_init(void) {
    smc_state_t* s = SMC;
    memset(s, 0, sizeof(smc_state_t));
    memset(FFE, 0, sizeof(ffe_state_t));
    ffe_load_trainer();
    // (the power on state the wiki gives: play mode, the nametables from CIRAM; PRG write protected; 4M mode, its
    // banks the file's last four)
    s -> mode = 0x42;
    s -> control = 0x20 | ((NesHeader.byInfo1 & 1) ? 0 : 0x10) | 3;
    s -> banking = 0;
    uint32_t banks = smc_file_prg_banks ? smc_file_prg_banks : num_8k_ROM_pages;
    s -> prg_banks = (uint8_t)banks;
    for (uint32_t i = 0; i < 4; i++) {
        s -> prg[i] = (uint8_t)(banks - 4 + i);
    }
    smc_apply();
}

/*
 * The Game Doctors' trainer (NES 2.0's miscellaneous ROM: its load address, its initialization routine's or 0, its
 * bytes), kept by cartridge_convert in board_state as a gd_trainer_t
 */
typedef struct {
    uint32_t size;
    uint16_t load, init;
    uint8_t data[];
} gd_trainer_t;

static void gd_keep_trainer(const uint8_t* data, uint32_t size) {
    if ((size < 16) || memcmp(data, "NES\x1a", 4) || ((data[7] & 0x0c) != 0x08)) {
        return;
    }
    uint32_t number = ((data[8] & 0x0f) << 8) | (data[7] & 0xf0) | (data[6] >> 4);
    if ((number != 561) && (number != 562)) {
        return;
    }
    // (board_state is this board's from here: the miscellaneous ROM's trainer, else an iNES one at $7000, run from
    // $7003 as Front Fareast's (its banks' set up: Dragon Quest IV's 4M mode), else none)
    uint32_t prg = (((data[9] & 0x0f) << 8) | data[4]) * 0x4000, chr = (((data[9] & 0xf0) << 4) | data[5]) * 0x2000;
    uint32_t ines = (data[6] & 4) ? 512 : 0, at = 16 + ines + prg + chr;
    const uint8_t* bytes = NULL;
    uint32_t length = 0, load = 0x7000, init = 0;
    if ((data[14] & 3) && (at + 5 <= size)) {
        load = data[at] | (data[at + 1] << 8);
        init = data[at + 2] | (data[at + 3] << 8);
        bytes = data + at + 4;
        length = size - at - 4;
    } else if (ines && (size >= 16 + ines)) {
        bytes = data + 16;
        length = ines;
        init = 0x7003;
    }
    if (bytes == NULL) {
        free(board_state);
        board_state = NULL;
        return;
    }
    gd_trainer_t* t = board_state_alloc(sizeof(gd_trainer_t) + length);
    t -> size = length;
    t -> load = (uint16_t)load;
    t -> init = (uint16_t)init;
    memcpy(t -> data, bytes, length);
}

__attribute__((cold, noinline)) static void gd_init(void) {
    smc_state_t* s = SMC;
    memset(s, 0, sizeof(smc_state_t));
    memset(FFE, 0, sizeof(ffe_state_t));
    memset(GD, 0, sizeof(gd_state_t));
    // (the latch's mode the submapper's, PRG write protected, the header's mirroring; 2M / 4M off)
    s -> control = (uint8_t)((board_submapper << 5) | ((NesHeader.byInfo1 & 1) ? 0 : 0x10) | 3);
    s -> banking = 0x10;
    const gd_trainer_t* t = board_state;
    if (t != NULL) {
        uint32_t loaded = 0;
        for (uint32_t i = 0; i < t -> size; i++) {
            uint32_t address = (t -> load + i) & 0xffff;
            if ((address < 0x0800) || ((address >= 0x6000) && (address < 0x8000) && (WRAM != NULL))) {
                NES_WINDOW_BYTE(address) = t -> data[i];
                loaded++;
            }
        }
        printf(" ...Game Doctor: %lu bytes of trainer at $%04X (initialization $%04X)\n", (unsigned long)loaded, t -> load,
            t -> init);
    }
    smc_apply();
}

// main.c's game start (board_fresh_wram, before the CPU's first instruction): with a trainer's initialization routine,
// the CPU from it as a JSR there from the reset, returning to a JMP ($FFFC) at $0100 (the stack page's bottom, which
// the routines' few bytes of stack don't reach; the game's reset clears it): the reset vector read after the routine,
// which may switch $E000's bank (Dragon Quest IV's on the Turbo Game Doctor). The Super Magic Card's at $7003, the Game
// Doctors' at the address their trainers give
static void smc_game_start(void) {
    uint32_t init = 0x7003;
    if (board_number != 12) {
        const gd_trainer_t* t = board_state;
        init = t ? t -> init : 0;
    } else if (!(NesHeader.byInfo1 & 4) || (WRAM == NULL)) {
        return;
    }
    if (!init) {
        return;
    }
    static const uint8_t jump[3] FRNES_COLD_RODATA = { 0x6c, 0xfc, 0xff };
    for (uint32_t i = 0; i < sizeof(jump); i++) {
        NES_WINDOW_BYTE(0x0100 + i) = jump[i];
    }
    // (RTS returns to the address + 1)
    NES_WINDOW_BYTE(0x0100 + cpu_registers -> SP) = 0x00;
    NES_WINDOW_BYTE(0x0100 + ((cpu_registers -> SP - 1) & 0xff)) = 0xff;
    cpu_registers -> SP = (uint8_t)(cpu_registers -> SP - 2);
    cpu_registers -> PC = (uint16_t)init;
}


/*
 * Nanjing's boards (mapper 163: the Chinese RPGs and translations, Pokemon / Final Fantasy / Chrono Trigger ports; Mesen
 * 2's Nanjing, after FCEUX): registers at $5000-$5FFF (address & $7300; $5101 itself): $5000 bits 0-3 and $5200 bits
 * 0-3 the 32 kB PRG bank (low / high), $5000 bit 7 the CHR RAM halves switched by the PPU (8 kB of CHR RAM: both
 * halves the first 4 kB after line 239's dot 256, the second after line 127's, while the PPU renders), its clear with the PPU above line 128
 * the halves 0 / 1; $5100 = 6 PRG bank 3; $5101 going from non-zero to zero toggles the "trigger"; $5300 a byte the
 * protection reads give back: $5100 the registers' OR (with $5200 inverted), $5500 $5300 | $5000 while the trigger is
 * set (else 0), 4 elsewhere. The switch points are events of the board timer while bit 7 is set (no PPU hook; the
 * PPU's position from ppu_dot_cpu_cycle, as J.Y.'s). State in the register page (+0x190-+0x19f), saved with a state.
 * Waixing's FS304 (mapper 162: the Waixing and Nanjing RPGs on it; the nesdev wiki's registers) on the same code: $5000
 * bit 7 the same CHR RAM halves (clear: the 8 kB as it is), registers by address & $FF00 at $5000-$5300, the 32 kB PRG
 * bank A20-A19 $5200 bits 0-1, A18-A17 $5000 bits 2-3, A16 $5000 bit 1 with $5300 bit 2 (else 1), A15 $5000 bit 0
 * with $5300 bits 0 and 2, 1 with bit 0 alone, else $5100 bit 1 (bank 2 at power on); no protection reads
 */
typedef struct {
    uint8_t registers[5], trigger, next_set, pad;
    uint16_t next_line, pad2;
    uint32_t timer_base;
} nanjing_state_t;
#define NANJING ((nanjing_state_t*)((uint8_t*)cpu_registers + 0x190))

// the PPU's position (scanline * 341 + dot) at a CPU cycle, as jy_line_start works it out
static uint32_t nanjing_position(uint32_t cycle) {
    int32_t to_end = (int32_t)(ppu_dot_cpu_cycle(261, 340) - cycle);
    int32_t position = 262 * 341 - 1 - ((to_end > 0) ? to_end * 3 : 0);
    return (position < 0) ? 0 : (uint32_t)position;
}

static void nanjing_apply(void) {
    const nanjing_state_t* n = NANJING;
    if (board_number == 162) {
        const uint8_t* r = n -> registers;
        uint32_t a16 = (r[3] & 4) ? ((r[0] >> 1) & 1) : 1;
        uint32_t a15 = (r[3] & 1) ? ((r[3] & 4) ? (r[0] & 1) : 1) : ((r[1] >> 1) & 1);
        prg_32k(((r[2] & 3) << 4) | (((r[0] >> 2) & 3) << 2) | (a16 << 1) | a15);
        return;
    }
    prg_32k((n -> registers[0] & 0x0f) | ((n -> registers[2] & 0x0f) << 4));
}

// the next switch point after the PPU's position: line 127 or 239, dot 257; past line 239's, line 0 first (an event
// that switches nothing: ppu_dot_cpu_cycle takes a line less than half a frame behind as just gone, not next frame's)
static void nanjing_next(void) {
    nanjing_state_t* n = NANJING;
    uint32_t position = nanjing_position(n -> timer_base);
    n -> next_line = (position < 127 * 341 + 257) ? 127 : ((position < 239 * 341 + 257) ? 239 : 0);
    n -> next_set = 1;
}

static uint32_t nanjing_event_cycle(uint32_t line) {
    return ppu_dot_cpu_cycle(line, line ? 257 : 0);
}

static void nanjing_cpu_run(uint32_t cycles) {
    nanjing_state_t* n = NANJING;
    n -> timer_base += cycles;
    if (!(n -> registers[0] & 0x80)) {
        return;
    }
    if (!n -> next_set) {
        nanjing_next();
    }
    for (uint32_t i = 0; i < 6; i++) {
        uint32_t at = nanjing_event_cycle(n -> next_line);
        if ((int32_t)(at - n -> timer_base) > 0) {
            break;
        }
        if (n -> next_line == 0) {
            n -> next_line = 127;
            continue;
        }
        // (the board sees the PPU's address change: only while it renders, its fetches)
        if (((const _PPU_registers_t*)((const uint8_t*)cpu_registers + 32)) -> PPUMASK & 0x18) {
            ppu_sync();
            chr_4k(0, (n -> next_line == 127) ? 1 : 0);
            chr_4k(1, (n -> next_line == 127) ? 1 : 0);
        }
        n -> next_line = (n -> next_line == 127) ? 239 : 0;
    }
}

static uint32_t nanjing_cycles_to_fire(void) {
    nanjing_state_t* n = NANJING;
    if (!(n -> registers[0] & 0x80)) {
        return 0xffffffff;
    }
    if (!n -> next_set) {
        nanjing_next();
    }
    int32_t cycles = (int32_t)(nanjing_event_cycle(n -> next_line) - n -> timer_base);
    return (cycles > 0) ? (uint32_t)cycles : 1;
}

static void nanjing_write_low(uint16_t address, uint8_t data) {
    nanjing_state_t* n = NANJING;
    if (address < 0x5000) {
        return;
    }
    if (mapper_cpu_timer) {
        mapper_cpu_run(cpu_cycle_now);
    }
    if (board_number == 162) {
        if (address >= 0x5400) {
            return;
        }
        n -> registers[(address >> 8) & 3] = data;
        if ((address < 0x5100) && !(data & 0x80)) {
            ppu_sync();
            chr_8k(0);
        }
        nanjing_apply();
    } else {
        if (address == 0x5101) {
            if (n -> registers[4] && !data) {
                n -> trigger ^= 1;
            }
            n -> registers[4] = data;
            return;
        }
        if ((address == 0x5100) && (data == 6)) {
            prg_32k(3);
            return;
        }
        switch (address & 0x7300) {
            case 0x5000:
                n -> registers[0] = data;
                if (!(data & 0x80) && (nanjing_position(cpu_cycle_now) < 128 * 341)) {
                    ppu_sync();
                    chr_4k(0, 0);
                    chr_4k(1, 1);
                }
                nanjing_apply();
                break;
            case 0x5100:
                n -> registers[1] = data;
                if (data == 6) {
                    prg_32k(3);
                }
                break;
            case 0x5200:
                n -> registers[2] = data;
                nanjing_apply();
                break;
            case 0x5300:
                n -> registers[3] = data;
                break;
        }
    }
    if (!mapper_cpu_timer) {
        n -> timer_base = cpu_cycle_now;
    }
    counter_timer_start();
    n -> next_set = 0;
    mapper_cpu_timer = (n -> registers[0] & 0x80) != 0;
    cpu_cut_batch_for_event(mapper_cycles_to_event());
}

static uint8_t nanjing_read_low(uint16_t address, uint8_t open_bus) {
    const nanjing_state_t* n = NANJING;
    if (address < 0x5000) {
        return open_bus;
    }
    switch (address & 0x7700) {
        case 0x5100:
            return n -> registers[3] | n -> registers[1] | n -> registers[0] | (n -> registers[2] ^ 0xff);
        case 0x5500:
            return n -> trigger ? (n -> registers[3] | n -> registers[0]) : 0;
    }
    return 4;
}

__attribute__((cold, noinline)) static void nanjing_init(void) {
    nanjing_state_t* n = NANJING;
    memset(n, 0, sizeof(nanjing_state_t));
    // ("Initial value of this register is 1, initial value of 'trigger' is 0": Mesen 2's toggle starts set)
    n -> trigger = 1;
    prg_32k(0);
    chr_4k(0, 0);
    chr_4k(1, 0);
    if (board_number == 162) {
        chr_8k(0);
        nanjing_apply();
    }
}

static void cnrom185_write(uint8_t data) {
    uint32_t enabled = (board_submapper >= 4) ? ((data & 3) == (board_submapper & 3)) : ((data & 0x0f) && (data != 0x13));
    ppu_sync();
    if (enabled) {
        chr_8k(0);
    } else {
        for (uint32_t i = 0; i < 8; i++) {
            set_ppu_pattern_table_bank_pointer(i, (uint8_t*)board_state + i * 0x400);
        }
    }
}

__attribute__((cold, noinline)) static void cnrom185_init() {
    uint8_t* disabled = board_state_alloc(0x2000);
    for (uint32_t i = 0; i < 0x2000; i++) {
        disabled[i] = (uint8_t)(i + 1);
    }
    prg_16k(0, 0);
    prg_16k(2, num_8k_ROM_pages / 2 - 1);
    chr_8k(0);
}

/**
 * Jaleco JF-05 / 06 / 07 / 08 / 09 / 10 and Konami (mapper 87: Argus (J), City Connection (J), The Goonies (J)): a write
 * to $6000-$7FFF selects the 8 kB CHR bank with bits 0 and 1 swapped; PRG is fixed
 */
static void jf87_write(uint8_t data) {
    if (!chr_8k_mapped(((data & 1) << 1) | ((data >> 1) & 1))) {
        ppu_sync();
        chr_8k(((data & 1) << 1) | ((data >> 1) & 1));
    }
}

__attribute__((cold, noinline)) static void jf87_init() {
    mapper_6000_mode = MAPPER_6000_REGISTERS;
    board_6000 = 1;
    prg_16k(0, 0);
    prg_16k(2, num_8k_ROM_pages / 2 - 1);
    chr_8k(0);
}

/**
 * MMC3 multicarts with an outer bank register (written at $6000-$7FFF, Mesen 2's MMC3_47 / MMC3_37), which limit the
 * MMC3's PRG and CHR banks to a block of the ROM:
 * - 47, NES-QJ (Super Spike V'Ball + Nintendo World Cup): bit 0 picks the 128 kB PRG / 128 kB CHR half
 * - 37, PAL-ZZ (Super Mario Bros. + Tetris + Nintendo World Cup): bits 0-2; PRG 0-2 the first 64 kB, 3 the second,
 *   4-6 the second 128 kB, 7 the last 64 kB; CHR bit 2 the 128 kB half
 * They run as mapper 4 with the MMC3's code unchanged: the block is the page tables (ROM_pages / VROM_pages rebased on
 * it, their counts its size), so the MMC3's bank numbers wrap inside it, its fixed banks are the block's last, and the
 * hot paths don't know the board. The tables as loaded are kept (cold) to rebase from
 */
// the MMC3's last $A001 (PRG RAM enable and write protect: board_write's MMC3 boards keep it, mapper.c's MMC3 doesn't),
// in the register page (+0xb0, zeroed at each load, saved with a state). The outer bank registers at $6000-$7FFF take
// a write only while it enables the RAM and doesn't protect it (bit 7 set, bit 6 clear), as Mesen 2's MMC3_37 / 47
#define MMC3_BOARD_A001 (*((uint8_t*)cpu_registers + 0xb0))
void mapper_mmc3_synced_write(uint16_t address, uint8_t data);
/*
 * Nintendo's EVENT2 board (mapper 555: Nintendo Campus Challenge 1991; MAME's nes_event2_device, the reproduction
 * board it knows): an MMC3 with an outer bank register at $5800-$5FFF (bit 2 the 256 kB half of PRG, its 128 kB of
 * CHR with it; bits 0-1 the PRG block's size, 64 kB to 256 kB, as the mask ((bits 0-1) << 3) | 7 over 8 kB banks;
 * bits 1-2 = 01 TQROM mode: CHR bank values with bit 6 set pick the 8 kB of CHR RAM, Pin*Bot's; bit 3 the timer, off
 * clearing it), 2 kB of RAM at $5000-$57FF (WRAM past its first 8 kB), and the timer, read at $5800-$5FFF: bit 7 set
 * once it has counted the DIP switches' cycles, (0x10 | DIP) << 25 (the setting MAME has, 6:15). Its state (the
 * MMC3's bank select and registers as written, for the TQROM fix-up; $5800; the count) in the register page past
 * $A001's (+0xb4-+0xc3), saved with a state
 */
typedef struct {
    uint8_t bank_select, registers[8], outer;
    uint8_t pad[2];
    uint32_t count;
} event2_state_t;
#define EVENT2 ((event2_state_t*)((uint8_t*)cpu_registers + 0xb4))
#define EVENT2_TIMER_TARGET ((0x10u | 4u) << 25)

// TQROM mode: the CHR banks whose value has bit 6 set on the CHR RAM, after the MMC3 mapped them (as mapper.c's
// mmc3_set_chr does for mapper 119), in the same write
static void event2_chr_ram(void) {
    const event2_state_t* e = EVENT2;
    if ((e -> outer & 0x06) != 0x02) {
        return;
    }
    uint32_t flip = (e -> bank_select & 0x80) ? 4 : 0;
    const uint8_t* r = e -> registers;
    uint8_t values[8] = { r[0] & 0xfe, r[0] | 1, r[1] & 0xfe, r[1] | 1, r[2], r[3], r[4], r[5] };
    for (uint32_t i = 0; i < 8; i++) {
        if (values[i] & 0x40) {
            mapper_chr_ram_slots |= 1 << (i ^ flip);
            set_ppu_pattern_table_bank_pointer(i ^ flip, chr_ram_page(values[i] & 7));
        }
    }
}

// $8000-$FFFF writes on the MMC3 outer bank boards (board_write): the MMC3's own, its $A001 kept (and on 555 the
// bank registers, for the TQROM fix-up)
static void mmc3_board_write(uint16_t address, uint8_t data) {
    uint32_t reg = address & 0xe001;
    if (reg == 0xa001) {
        MMC3_BOARD_A001 = data;
    }
    mapper_mmc3_synced_write(address, data);
    if (cartridge_mapper_number == 555) {
        event2_state_t* e = EVENT2;
        if (reg == 0x8000) {
            e -> bank_select = data;
        } else if (reg == 0x8001) {
            e -> registers[e -> bank_select & 7] = data;
        }
        if ((reg == 0x8000) || (reg == 0x8001)) {
            event2_chr_ram();
        }
    }
}

static int mmc3_board_ram_writable(void) {
    return (MMC3_BOARD_A001 & 0xc0) == 0x80;
}

/*
 * MMC3 clones whose outer bits depend on the slot or the bank (Mesen 2's MMC3_12, MMC3_134, MMC3_224, MMC3_Coolboy):
 * the MMC3 (its IRQ counter and registers through mmc3_board_write), then mmc3x_apply maps the banks again from its
 * bank select and registers as written, after each $8000 / $8001 write and each outer register write:
 *   12 (submapper 0: Supertone / Gouder SL-5020B, Dragon Ball Z 5): $4100-$5FFF writes (board_low), bit 0 CHR bit 8
 *     for the banks at $0000-$0FFF, bit 4 for $1000-$1FFF
 *   134 (the ABAB / YH multicarts; the nesdev wiki's registers, Mesen 2 has $6001's bits 1 and 5 only): $6000-$7FFF
 *     (board_6000, address bits 0-1). $6000: bit 3 CNROM CHR ($6002's 8 kB bank), bit 4 PRG A19, bit 5 CHR A19, bit 7
 *     the lock (then only $6002's bits 0-1 change; bit 6, $8000-$FFFF reading the solder pads, isn't emulated). $6001:
 *     bits 0-1 PRG A17-A18 (A17 with bit 2's 128 kB blocks), bit 3 NROM-128 (with bit 7's NROM mode: the MMC3's $8000
 *     bank over $8000-$FFFF), bits 4-5 CHR A17-A18 (A17 with bit 6's 128 kB blocks)
 *   224 (Jncota KT-008: the 1 MB Ys / Dragon Quest / Pokemon conversions): $5000, bit 2 PRG bit 6
 *   313 (the reset based TxROM multicarts: 4-in-1s picked with the reset button; Mesen 2's ResetTxrom): the number of
 *     resets since power on (0-3: board_soft_reset) the outer bank, PRG bits 4-5 over the MMC3's 4, CHR bits 7-8 over
 *     its 7 (BOARD_RESET_TXROM)
 *   215 (UNL-8237 submapper 0: the Super Game boards, Pocahontas 2, Mortal Kombat 3, Earthworm Jim 2 and the Realtec
 *     multicarts; the nesdev wiki's registers, Mesen 2's MMC3_215 to compare): $5000-$5FFF by address bits 0-2 (board_low): 0 the mode (bits 0-3 the NROM
 *     bank, bit 5 32 kB, bit 6 the inner 128 kB / 128 kB block, bit 7 NROM), 1 the outer bits (0-1 PRG A17-A18, 2-3
 *     CHR A18-A19, 4 PRG A17 and 5 CHR A17 in the inner mode), 7 the scramble: its row of the tables below permutes
 *     the MMC3's register addresses ($8000 / $8001 / ... / $E001) and, at $8000, the bank select's index
 *   260 (the HP10xx / HP20xx multicarts; the nesdev wiki's registers, Mesen 2's BmcHpxx to compare): $5000-$5FFF by
 *     address bits 0-1 (board_low) until $5000's bit 7 locks them (a reset clears them): $5000 bits 0-2 the mode,
 *     $5001 the PRG base (16 kB units), $5002 the CHR base (8 kB). Modes 0-3 the MMC3 in a block (bit 1 128 kB of PRG,
 *     else 256 kB; bit 0 128 kB of CHR, else 256 kB) at the base; 4 NROM-128, 5 NROM-256 and 6 / 7 CNROM (16 / 32 kB
 *     of CHR) at it, $8000-$FFFF the latch (outer[3]: bits 0-1 the CNROM bank, bit 2 the mirroring) and not the MMC3.
 *     Its DIP switch reads 0. Board 76 (BOARD_HPXX)
 *   121 (Kasheng's A9711 / A9713: Street Fighter Zero 2 '97, Panda Prince, the Super 3-in-1; the nesdev wiki's
 *     registers, Mesen 2's MMC3_121 to compare): $5000-$5FFF writes (board_low) set the protection byte reads there
 *     give back ($83, $83, $42, $00 by the value's bits 0-1), at $5180 (& $F180) also the outer register (bit 7 PRG
 *     A18, and CHR A18 when PRG and CHR are the same size; $80 at power on). $8003 (& $E003) the override's mode, and
 *     the MMC3's $8000 too; $8001 its value, bits 0-5 reversed (applied while the mode lets it): modes $26 / $28 / $2A
 *     load $E000's / $C000's / $A000's bank and let later values through, $20 / $29 / $2B / $3C / $3F (and $2C with a
 *     value not 0) load $E000's and stop them, $2F keeps the banks, any other ends the overrides. A 512 kB CHR ROM with
 *     less PRG (Street Fighter Zero 2 '97): CHR A18 from PPU A12, the right pattern table's slots the upper 256 kB.
 *     In the register page (+0x1e4-+0x1eb: the three banks, the outer register, the protection byte, the mode, the
 *     value, the flag)
 *   189 (TXC's FC-001EM and the pirate boards: Thunder Warrior, Street Fighter II, Master Fighter III...; the nesdev
 *     wiki's generalized decode): a write to $4020-$7FFF (board_low, board_6000) the 32 kB PRG bank, the value's high
 *     nibble OR its low nibble, over the MMC3's PRG banks (its CHR banks and IRQ the MMC3's)
 *   196 (MRCM's Mario hacks; the nesdev wiki's): the MMC3 with its A0 from CPU A1 (submappers 1 and 3), A2 (2) or, in
 *     submapper 0, set when A0 is clear and any of A1-A3 is set; submapper 3, and 0 after a write to $6000-$7FFF, with
 *     189's PRG register at $6000-$7FFF (outer[1] set once it's on)
 *   249 (the T9552 chip: 封神榜, 楚漢爭霸, Pokemon Gold...; the nesdev wiki's register, Mesen 2's MMC3_249 to compare):
 *     $5000-$5FFF (board_low) the scramble; with its bit 1 set (the games write $02 at once) the MMC3's PRG bank bits
 *     0-4 are taken from its bits 0, 4, 3, 1, 2 and its CHR bank bits 0-7 from bits 0, 1, 3, 7, 6, 2, 4, 5
 *   534 (the ING-022 ASIC: 振华's multicarts and single games; the nesdev wiki's 126 / 534 pages, Mesen 2's MMC3_126
 *     to compare): $6000-$6003 (& $E003, board_6000; WRAM under them), at any time as Mesen 2's (the wiki's $A001
 *     bit 7 condition: the 700000-in-1 writes them before it), until the lock:
 *     $6000 the outer bank (bits 1-2 PRG A18-A19, bit 4 A20, bit 5 inverted A21; bit 6 PRG A17 from bit 0, a 128 kB
 *     inner bank; bit 7 CHR A17 from bit 3, bits 4 / 5 CHR A18 / A19), $6002 bits 0-3 the 8 kB CHR bank,
 *     $6003 bits 0-3 the PRG mode (0 MMC3, 1-2 NROM-128 and 3 NROM-256 from the MMC3's R6, D UNROM and F ANROM with
 *     the $8000-$FFFF writes R6's, 8-B as C-F; C / E not done), bit 4 8 kB CHR banks from $6002, bit 5 one-screen
 *     mirroring from R6's bit 4, bit 7 locks $6000, $6001 and $6003. In modes 8-F writes reach only the MMC3's odd
 *     registers. The IRQ latch takes $C000's value inverted. Not emulated: $6001 (PRG A0 from the menu's solder
 *     pads, the one-screen choice through $A000 bit 1), submappers 1-2's chip selects. Its registers in outer[0-3].
 *     Board 95 (BOARD_ING022), with 422 (the ING-003C boards, the same registers and the IRQ latch not inverted)
 *   115 (Kasheng's SFC-02B / -03 / -004: 幽☆遊☆白書 Final, Thunderbolt, the Kasheng ports; also iNES 248; the nesdev
 *     wiki's registers, Mesen 2's MMC3_115 to compare): $6000-$7FFF (& $E003, board_6000; the WRAM's writes too)
 *     $6000 the mode (bit 7 NROM: bits 0-3 the 16 kB bank, bit 5 32 kB; bit 6 PRG A18 in both modes), $6001 bit 0 CHR
 *     A18. Not emulated: $6002's solder pad reads, the "Sharp" IRQ (a latch of 0 every line). 114 (SuperGame's /
 *     Hosenkan's Aladdin, The Lion King, Boogerman...; the nesdev wiki's tables): the same $6000 / $6001 with the
 *     MMC3's registers reached through a scramble (mmc3x_114_*: the register each address reaches, the bank select's
 *     index, by submapper), and the MMC3A's IRQ as far as a latch of 0 turning it off
 *   197 (the Super Fighter / Mortal Kombat bootlegs; Mesen 2's MMC3_197 to compare): the MMC3's CHR registers doubled,
 *     a 4 kB and two 2 kB banks: CHR mode 0 R0 at $0000, R2 / R3 at $1000 / $1800; mode 1 R2 at $0000, R0 at both
 *     $1000 and $1800
 *   361 (J.Y.'s YY841101C: the 1994 Ball / 1995-1996 Super HiK 4-in-1s, OK-411; the nesdev wiki's, no Mesen 2 board): a
 *     write to $7000-$7FFF (board_6000; $6000-$6FFF the WRAM's) bits 4-7 (outer[0]) the 128 kB outer bank of PRG and
 *     of CHR, the MMC3's banks inside it (BOARD_YY841101)
 *   348 (BMC-830118C: the 1994 Red Pig / Super HiK 7-in-1s, M-022; the nesdev wiki's, Mesen 2's Bmc830118C to compare):
 *     a write to $6800-$68FF (board_6000) with the WRAM enabled and writable, bits 2-3 (outer[0]) the 128 kB outer bank
 *     of PRG and CHR; 3 the GNROM-like mode, the MMC3's banks for $8000 / $A000 AND $FD there and OR 2 at $C000 / $E000
 *   44 (the Super Big 7-in-1 / Super Cool Boy 4-in-1 MMC3 multicarts; Mesen 2's MMC3_44 to compare): $A001 writes (the
 *     MMC3's too) bits 0-2 the block (outer[0], 7 as 6): 0-5 128 kB of PRG (MMC3 bits 0-3) and of CHR (bits 0-6) at
 *     block x 128 kB, 6 256 kB of each at 768 kB. Reset: block 0
 *   250 (Nitra's MMC3: Time Diver Avenger; Mesen 2's MMC3_250 to compare): the MMC3's registers from the written
 *     address, A13-A15 and A10 as A0 the register, A0-A7 the value
 *   126 (the AT-207 / TEC9719 multicarts: Gamezone 118-in-1...; Mesen 2's MMC3_126 to compare): $6000-$7FFF writes
 *     (board_6000, address bits 0-1: outer[0-3]; 0 and 3 locked by 3's bit 7): PRG bit 7 from 0's bit 4, bits 5-6 its
 *     bits 1-2, with 0's bit 6 bit 4 its bit 0 (else the MMC3's); 3's bits 0-1 the mode (1 / 2 the R6 bank's 16 kB at
 *     both halves, 3 its 32 kB); CHR the MMC3's (bits 0-6 with 0's bit 7) over bit 7 (0's bit 3 with bit 7 set, else
 *     2's bit 7), bit 8 0's bit 5, bit 9 its bit 4; 3's bit 4 an 8 kB bank from 2's bits 0-3 instead
 *   245 (Waixing's MMC3 with CHR RAM: 勇者斗恶龙 VII...; Mesen 2's MMC3_245 to compare): PRG bit 6 from CHR register
 *     R0's bit 1, the MMC3's bits 0-5; with CHR RAM its 8 kB in 4 kB halves, swapped by the MMC3's CHR mode
 *   123 (卡聖's MMC3: Mortal Kombat 3...; Mesen 2's MMC3_123 to compare): $5800 (and $5000-$5FFF with address bit 11,
 *     board_low; bit 0 outer[0 / 1]): outer[0] bit 6 an NROM bank from its bits 0, 3, 2, 5 (bit 1: 32 kB, else 16 kB
 *     at both halves); the bank select's register number permuted (0 3 1 5 6 7 2 4)
 *   187 (the King of Fighters '96, Street Fighter Zero 2 '97 MMC3s; Mesen 2's MMC3_187 to compare): $5000 / $6000
 *     (board_low / board_6000) outer[0]: bit 7 an NROM bank from bits 0-4 (bit 5 clear: 16 kB at both halves; set: 32
 *     kB, with bit 6 aligned to 64 kB), else the MMC3's bits 0-5; CHR bit 8 on the MMC3's 2 kB banks' half; $8001
 *     only after a $8000 write (outer[1]); $5000-$5FFF reads $83
 *   287 (BMC-411120-C / K-3088: the 4-in-1 Contra Force+Master Fighter II...; Mesen 2's MMC3_Bmc411120C to compare):
 *     $6000-$7FFF writes (board_6000) the address's low byte (outer[0]): bits 0-1 a 128 kB outer bank of PRG and CHR,
 *     bit 3 32 kB (bits 4-5 one of the top 128 kB's, the MMC3's banks not used); its DIP switch (bit 2) at 0
 *   366 (BMC-GN-45: the K-3131SS / GN-45 4-in-1s; Mesen 2's BmcGn45 to compare): until bit 7 locks it, $6000-$6FFF
 *     writes the address's bits 4-5, $7000-$7FFF the value's, as a 128 kB outer bank of PRG and CHR (outer[0]; the
 *     lock outer[1], cleared at reset; then the WRAM's)
 *   263 (the King of Fighters '97 / Boogerman II MMC3; Mesen 2's MMC3_Kof97 to compare): every write's data bits 0 / 1 /
 *     2 / 5 to the MMC3 as its bits 2 / 0 / 5 / 1 (the others as they are), and $9000 / $D000 / $F000 as $8001 /
 *     $C001 / $E001
 *   254 (Pikachu Y2K's MMC3; Mesen 2's MMC3_254 to compare): until the first $8000 write (outer[0]), $6000-$7FFF reads
 *     give the RAM's byte XOR the last $A001 value (outer[1]; board_read_6000, through mapper_6000_reads only then)
 *   205 (the JC-016-2 / 3-in-1 / 4-in-1 MMC3 multicarts; Mesen 2's MMC3_205 to compare, the nesdev wiki's): a write
 *     to $6000-$7FFF (board_6000; the WRAM's too) bits 0-1 the block: PRG blocks 0 / 1 of 256 kB, 2 / 3 of 128 kB
 *     (the block << 4 over the 8 kB banks), CHR bit 7 the block's bit 0 and with bit 1 bit 8 and a 128 kB inner bank
 *   195 (Waixing FS303: Captain Tsubasa 2, Columbus, Mega Man X...; Mesen 2's MMC3_ChrRam as it has 195): CHR bank
 *     values 0-3 the cartridge's CHR RAM (its first 4 kB), the rest CHR ROM. Not emulated: the GAL's other RAM windows
 *     (bank values $80-$CA, the nesdev wiki's), two games' 4 kB of PRG RAM at $5000-$5FFF. The other Waixing MMC3s
 *     with CHR RAM banks the same way (the nesdev wiki's, as Mesen 2 has them): 74 (values 8-9, 2 kB), 191 ($80-$FF,
 *     2 kB), 192 (8-11, 4 kB), 194 (0-1, 2 kB)
 *   268 (Coolboy / Mindkids: submapper 0 at $6000-$7FFF, 1 at $5000-$5FFF; FCEUX's COOLBOY as Mesen 2 has it):
 *     four registers (address bits 0-1) until register 3 has bit 7 set and bit 4 clear: PRG base and mask (register 0
 *     bits 0-2 and 4-6, register 1 bits 2-7), CHR bit 7 / the 128 kB mask (register 0 bits 3 and 7), register 3 bit 4
 *     the 16 kB / 32 kB modes (bits 1-3, register 2 the 8 kB CHR bank), bit 6 the fixed banks' variant; $6000-$7FFF
 *     writes reach the WRAM too while $A001 enables it (BOARD_COOLBOY)
 * State in the register page (+0x150-+0x15f: the bank select, the registers, the outer registers; zeroed at each
 * load, saved with a state)
 */
typedef struct {
    uint8_t bank_select, registers[8], outer[4];
} mmc3x_state_t;
#define MMC3X ((mmc3x_state_t*)((uint8_t*)cpu_registers + 0x150))
// (121's: the three banks, the outer register, the protection byte, the mode, the value, the flag)
#define M121 ((uint8_t*)cpu_registers + 0x1e4)

static void mmc3x_prg(uint32_t slot, uint32_t page) {
    const uint8_t* e = MMC3X -> outer;
    switch (board_number) {
        case 134: {
            if (e[1] & 0x80) {
                // (NROM: CPU A13 / A14 low at the MMC3, its $8000 bank; A14 the MMC3's in NROM-128)
                uint32_t first = (MMC3X -> bank_select & 0x40) ? 0xfe : MMC3X -> registers[6];
                page = (first & ~3u) | (slot & 1) | ((e[1] & 0x08) ? (first & 2) : (slot & 2));
            }
            uint32_t mask = (e[1] & 0x04) ? 0x0f : 0x1f;
            page = (page & mask) | ((e[1] & 0x04) ? ((e[1] & 1) << 4) : 0) | ((e[1] & 2) << 4) | ((e[0] & 0x10) << 2);
        } break;
        case 224:
            page = (page & 0x3f) | ((e[0] & 0x04) << 4);
            break;
        case BOARD_HPXX:
            if (e[0] & 4) {
                uint32_t bank = e[1] & 0x3f;
                page = ((e[0] & 7) == 4) ? ((bank << 1) | (slot & 1)) : (((bank >> 1) << 2) | slot);
            } else {
                uint32_t mask = (e[0] & 2) ? 0x0f : 0x1f;
                page = (page & mask) | (((e[1] & 0x3f) << 1) & ~mask);
            }
            break;
        case 205:
            page = (page & ((e[0] & 2) ? 0x0f : 0x1f)) | ((e[0] & 3) << 4);
            break;
        case 445:
            // (445: $5002 bit 0 a 128 kB inner bank, $5000 bits 4-6 PRG A17-A19)
            page = (e[2] & 1) ? ((page & 0x0f) | (((e[0] >> 4) & 7) << 4)) : ((page & 0x1f) | (((e[0] >> 5) & 3) << 5));
            break;
        case 516:
            // (516: 128 kB inner banks, $8010-$FFFF's A0-A1 PRG A17-A18)
            page = (page & 0x0f) | ((e[0] & 3) << 4);
            break;
        case 198:
            // (198: banks $40-$4F the second chip's 128 kB, $00-$3F the first's 512 kB; its fixed banks $4E / $4F)
            page &= (page >= 0x40) ? 0x4f : 0x3f;
            break;
        case 267: case 377: {
            // (267 / 377: $6000 bits 1, 2 and 5 the outer bank: 256 kB on 267; on 377 128 kB, its menu's reset code in
            // the first 128 kB's last page)
            uint32_t outer = ((e[0] >> 1) & 3) | (((e[0] >> 5) & 1) << 2);
            page = (board_number == 377) ? ((page & 0x0f) | (outer << 4)) : ((page & 0x1f) | (outer << 5));
        } break;
        case 395:
            // (395: $6010 bit 3 a 128 kB inner bank, bit 0 its A17; $6000 bits 4-5 A18-A19, bit 3 A20)
            page = (e[1] & 0x08) ? ((page & 0x0f) | ((e[1] & 1) << 4)) : (page & 0x1f);
            page |= (((e[0] >> 4) & 3) << 5) | (((e[0] >> 3) & 1) << 7);
            break;
        case BOARD_YY841101:
            page = (page & 0x0f) | ((e[0] >> 4) << 4);
            break;
        case 44:
            page = (e[0] >= 6) ? ((page & 0x1f) | 0x60) : ((page & 0x0f) | (e[0] << 4));
            break;
        case 245:
            page = (page & 0x3f) | ((MMC3X -> registers[0] & 2) << 5);
            break;
        case BOARD_411120C:
            page = (e[0] & 8) ? (((((e[0] >> 4) & 3) | 0x0c) << 2) + slot) : ((page & 0x0f) | ((e[0] & 3) << 4));
            break;
        case BOARD_GN45:
            page = (page & 0x0f) | e[0];
            break;
        case 123:
            if (e[0] & 0x40) {
                uint32_t bank = (e[0] & 1) | ((e[0] >> 2) & 2) | (e[0] & 4) | ((e[0] >> 2) & 8);
                page = (e[0] & 2) ? (((bank & 0xfe) << 1) + slot) : ((bank << 1) + (slot & 1));
            }
            break;
        case 187:
            if (e[0] & 0x80) {
                uint32_t bank = e[0] & 0x1f;
                page = !(e[0] & 0x20) ? ((bank << 1) + (slot & 1)) : (e[0] & 0x40) ? ((bank & 0xfc) + slot) :
                    (((bank & 0xfe) << 1) + slot);
            } else {
                page &= 0x3f;
            }
            break;
        case 126: {
            if (e[3] & 3) {
                // (the R6 bank's 16 / 32 kB: R6 where the MMC3's PRG mode puts it)
                const mmc3x_state_t* m = MMC3X;
                page = m -> registers[6] + (((e[3] & 3) == 3) ? slot : (slot & 1));
            }
            page = (page & ((e[0] & 0x40) ? 0x0f : 0x1f)) | ((e[0] & 0x10) << 3) | ((e[0] & 6) << 4) |
                ((e[0] & e[0] >> 6 & 1) << 4);
        } break;
        case BOARD_830118C:
            if ((e[0] & 0x0c) == 0x0c) {
                // (the MMC3's CPU A14 low: its $8000 / $A000 banks at both halves, CPU A14 PRG A14)
                const mmc3x_state_t* m = MMC3X;
                uint32_t low = (slot & 1) ? m -> registers[7] : ((m -> bank_select & 0x40) ? 0xfe : m -> registers[6]);
                page = 0x30 | (((low & 0x0f) & ~2u) | ((slot & 2) ? 2 : 0));
            } else {
                page = ((e[0] & 0x0c) << 2) | (page & 0x0f);
            }
            break;
        case 115: case 248: case 114:
            if (e[0] & 0x80) {
                uint32_t bank = (e[0] & 0x0f) | ((e[0] & 0x40) >> 2);
                page = (e[0] & 0x20) ? (((bank >> 1) << 2) | slot) : ((bank << 1) | (slot & 1));
            } else {
                page = (page & 0x1f) | ((e[0] & 0x40) >> 1);
            }
            break;
        case BOARD_ING022: {
            uint32_t r6 = MMC3X -> registers[6];
            switch (e[3] & 0x0f) {
                case 1: case 2: case 5: case 6: page = (r6 & 0x1e) | (slot & 1); break;
                case 3: case 7: page = (r6 & 0x1c) | slot; break;
                case 9: case 0x0d: page = (slot < 2) ? (((r6 & 0x0f) << 1) | (slot & 1)) : (0x1e | (slot & 1)); break;
                case 0x0b: case 0x0f: page = ((r6 & 7) << 2) | slot; break;
            }
            uint32_t mask = (e[0] & 0x40) ? 0x0f : 0x1f;
            page = (page & mask) | ((e[0] & 0x40) ? ((e[0] & 1) << 4) : 0) | ((e[0] & 6) << 4) | ((e[0] & 0x10) << 3) |
                ((~e[0] & 0x20) << 3);
        } break;
        case 249:
            if (e[0] & 2) {
                page = (page & ~0x1fu) | (page & 1) | ((page >> 3) & 2) | ((page >> 1) & 4) | ((page << 2) & 8) |
                    ((page << 2) & 0x10);
            }
            break;
        case 189: case 196:
            if ((board_number == 189) || MMC3X -> outer[1]) {
                page = ((uint32_t)MMC3X -> outer[0] << 2) | slot;
            }
            break;
        case 121: {
            const uint8_t* r = M121;
            if ((r[5] & 0x3f) && slot) {
                page = r[3 - slot];
            }
            page = (page & 0x1f) | ((r[3] & 0x80) >> 2);
        } break;
        case 215:
            if (e[0] & 0x80) {
                uint32_t bank = ((e[1] & 3) << 4) | ((e[0] & 0x40) ? ((e[0] & 7) | ((e[1] & 0x10) >> 1)) : (e[0] & 0x0f));
                page = (e[0] & 0x20) ? (((bank >> 1) << 2) | slot) : ((bank << 1) | (slot & 1));
            } else if (e[0] & 0x40) {
                page = ((e[1] & 3) << 5) | (page & 0x0f) | (e[1] & 0x10);
            } else {
                page = ((e[1] & 3) << 5) | (page & 0x1f);
            }
            break;
        case BOARD_RESET_TXROM:
            page = (page & 0x0f) | ((e[0] & 3) << 4);
            break;
        case BOARD_COOLBOY: {
            uint32_t prg_mode = MMC3X -> bank_select & 0x40;
            uint32_t mask = ((0x3f | (e[1] & 0x40) | ((e[1] & 0x20) << 2)) ^ ((e[0] & 0x40) >> 2)) ^ ((e[1] & 0x80) >> 2);
            uint32_t base = (e[0] & 0x07) | ((e[1] & 0x10) >> 1) | ((e[1] & 0x0c) << 2) | ((e[0] & 0x30) << 2);
            if ((e[3] & 0x40) && (page >= 0xfe) && prg_mode && ((slot == 1) || (slot == 3))) {
                page = 0;
            }
            if (!(e[3] & 0x10)) {
                page = ((base << 4) & ~mask) | (page & mask);
            } else {
                mask &= 0xf0;
                uint32_t emask = (e[1] & 0x02) ? ((e[3] & 0x0c) | (slot & 2)) : (e[3] & 0x0e);
                page = ((base << 4) & ~mask) | (page & mask) | emask | (slot & 1);
            }
        } break;
    }
    set_6502_bank_pointer(slot, prg_page(page));
}

static void mmc3x_chr(uint32_t slot, uint32_t page) {
    const mmc3x_state_t* m = MMC3X;
    const uint8_t* e = m -> outer;
    switch (board_number) {
        case 12:
            page |= (e[0] & ((slot < 4) ? 0x01 : 0x10)) ? 0x100 : 0;
            break;
        case 215:
            page = ((e[1] & 0x0c) << 6) | ((e[0] & 0x40) ? ((page & 0x7f) | ((e[1] & 0x20) << 2)) : page);
            break;
        case BOARD_YY841101:
            page = (page & 0x7f) | ((e[0] >> 4) << 7);
            break;
        case BOARD_830118C:
            page = ((e[0] & 0x0c) << 5) | (page & 0x7f);
            break;
        case 44:
            page = (e[0] >= 6) ? ((page & 0xff) | 0x300) : ((page & 0x7f) | (e[0] << 7));
            break;
        case 245:
            if (!num_1k_VROM_pages) {
                // (8 kB of CHR RAM, its 4 kB halves swapped by the MMC3's CHR mode)
                page = slot ^ ((m -> bank_select & 0x80) ? 4 : 0);
            }
            break;
        case 187:
            if (((m -> bank_select & 0x80) != 0) == (slot >= 4)) {
                page |= 0x100;
            }
            break;
        case BOARD_411120C:
            page |= (e[0] & 3) << 7;
            break;
        case BOARD_GN45:
            page = (page & 0x7f) | (e[0] << 3);
            break;
        case 126: {
            uint32_t outer = ((((~e[0] >> 7) & (e[2] >> 7)) | ((e[0] >> 3) & (e[0] >> 7))) & 1) << 7 |
                ((e[0] >> 5) & 1) << 8 | ((e[0] >> 4) & 1) << 9;
            page = (e[3] & 0x10) ? (outer | ((e[2] & 0x0f) << 3) | slot) : (outer | (page & ((e[0] & 0x80) ? 0x7f : 0x3ff)));
        } break;
        case 115: case 248: case 114:
            page |= (e[1] & 1) << 8;
            break;
        case 445:
            // ($5002 bit 3 a 128 kB inner bank, $5001 bits 4-6 CHR A17-A19)
            page = (e[2] & 8) ? ((page & 0x7f) | (((e[1] >> 4) & 7) << 7)) : ((page & 0xff) | (((e[1] >> 5) & 3) << 8));
            break;
        case 516:
            // (A2-A3 CHR A17-A18)
            page = (page & 0x7f) | (((e[0] >> 2) & 3) << 7);
            break;
        case 198: {
            // (198: its 8 kB of CHR RAM in order while the six CHR registers are all 0, as Mesen 2's)
            const uint8_t* r = m -> registers;
            if (!(r[0] | r[1] | r[2] | r[3] | r[4] | r[5])) {
                page = slot;
            }
        } break;
        case 267: case 377:
            // (the same bits the 128 kB outer CHR bank)
            page = (page & 0x7f) | (((e[0] >> 1) & 3) << 7) | (((e[0] >> 5) & 1) << 9);
            break;
        case 395:
            // ($6010 bit 6 a 128 kB inner bank, bit 4 its A17, bit 5 A20; $6000 bits 4-5 A18-A19)
            page = (e[1] & 0x40) ? ((page & 0x7f) | (((e[1] >> 4) & 1) << 7)) : (page & 0xff);
            page |= (((e[0] >> 4) & 3) << 8) | (((e[1] >> 5) & 1) << 10);
            break;
        case 205:
            if (e[0] & 2) {
                page = (page & 0x7f) | 0x100;
            }
            page |= (e[0] & 1) << 7;
            break;
        case BOARD_ING022: {
            uint32_t outer = ((e[0] & 0x80) ? (((e[0] >> 3) & 1) << 7) : 0) | (((e[0] >> 4) & 1) << 8) | (((e[0] >> 5) & 1) << 9);
            if (e[3] & 0x10) {
                page = outer | ((e[2] & 0x0f) << 3) | slot;
            } else {
                page = outer | (page & ((e[0] & 0x80) ? 0x7f : 0xff));
            }
        } break;
        case 249:
            if (e[0] & 2) {
                page = (page & ~0xffu) | (page & 3) | ((page >> 1) & 4) | ((page >> 4) & 8) | ((page >> 2) & 0x10) |
                    ((page << 3) & 0x20) | ((page << 2) & 0xc0);
            }
            break;
        case 121:
            if (num_1k_VROM_pages == num_8k_ROM_pages * 8) {
                page = (page & 0xff) | ((M121[3] & 0x80) << 1);
            } else if (num_1k_VROM_pages > 256) {
                page = (page & 0xff) | ((slot >= 4) ? 0x100 : 0);
            }
            break;
        case BOARD_HPXX:
            if (e[0] & 4) {
                uint32_t inner = ((e[0] & 7) == 6) ? 1 : ((e[0] & 7) == 7) ? 3 : 0;
                page = ((((e[2] & 0x7f) & ~inner) | (e[3] & inner)) << 3) | slot;
            } else {
                uint32_t mask = (e[0] & 1) ? 0x7f : 0xff;
                page = (page & mask) | (((e[2] & 0x7f) << 3) & ~mask);
            }
            break;
        case 195: case 74: case 191: case 192: case 194: {
            // (the CHR RAM's bank values: first, last; the RAM's 1 kB pages)
            uint32_t first = (board_number == 74) || (board_number == 192) ? 8 : (board_number == 191) ? 0x80 : 0;
            uint32_t last = (board_number == 74) ? 9 : (board_number == 191) ? 0xff : (board_number == 192) ? 11 :
                (board_number == 194) ? 1 : 3;
            uint32_t ram = ((board_number == 192) || (board_number == 195)) ? 4 : 2;
            if ((page >= first) && (page <= last) && num_1k_VRAM_pages) {
                mapper_chr_ram_slots |= 1 << slot;
                set_ppu_pattern_table_bank_pointer(slot, chr_ram_page((page - first) % ram));
                return;
            }
            mapper_chr_ram_slots &= ~(1 << slot);
        } break;
        case BOARD_RESET_TXROM:
            page = (page & 0x7f) | ((e[0] & 3) << 7);
            break;
        case 134: {
            uint32_t mask = (e[1] & 0x40) ? 0x7f : 0xff;
            if (e[0] & 0x08) {
                page = ((e[2] & (mask >> 3)) << 3) | slot;
            }
            page = (page & mask) | ((e[1] & 0x40) ? ((e[1] & 0x10) << 3) : 0) | ((e[1] & 0x20) << 3) | ((e[0] & 0x20) << 4);
        } break;
        case BOARD_COOLBOY: {
            uint32_t mask = 0xff ^ (e[0] & 0x80), high = ((e[0] & 0x08) << 4) & ~mask;
            // (the slot's place in the MMC3's layout: 0-7 with its 2 kB banks first)
            uint32_t place = (m -> bank_select & 0x80) ? (slot ^ 4) : slot;
            if (e[3] & 0x10) {
                if ((e[3] & 0x40) && ((place == 1) || (place == 3))) {
                    page &= 0x7f;
                }
                page = (page & 0x80 & mask) | high | ((e[2] & 0x0f) << 3) | slot;
            } else {
                if (e[3] & 0x40) {
                    page = (place == 0) ? m -> registers[0] : (place == 2) ? m -> registers[1] :
                        ((place == 1) || (place == 3)) ? 0 : page;
                }
                page = (page & mask) | high;
            }
        } break;
    }
    set_ppu_pattern_table_bank_pointer(slot, chr_page(page));
}

// (260's latch bit 2 in modes 4-7: horizontal when set, the nesdev wiki's; vertical in Mesen 2. None of Matt's dumps
// tells them apart)
#define MIRROR260_SET NAMETABLE_MIRRORING_HORIZONTAL
#define MIRROR260_CLEAR NAMETABLE_MIRRORING_VERTICAL
static void mmc3x_apply(void) {
    const mmc3x_state_t* m = MMC3X;
    const uint8_t* r = m -> registers;
    uint32_t swap = (m -> bank_select & 0x40) ? 2 : 0;
    mmc3x_prg(0 ^ swap, r[6]);
    mmc3x_prg(1, r[7]);
    mmc3x_prg(2 ^ swap, 0xfe);
    mmc3x_prg(3, 0xff);
    uint32_t flip = (m -> bank_select & 0x80) ? 4 : 0;
    if (board_number == 197) {
        // (a 4 kB and two 2 kB banks from the registers doubled)
        uint32_t four = flip ? r[2] : r[0], two_a = flip ? r[0] : r[2], two_b = flip ? r[0] : r[3];
        for (uint32_t i = 0; i < 4; i++) {
            mmc3x_chr(i, (four << 1) + i);
        }
        for (uint32_t i = 0; i < 2; i++) {
            mmc3x_chr(4 + i, (two_a << 1) + i);
            mmc3x_chr(6 + i, (two_b << 1) + i);
        }
        return;
    }
    const uint8_t values[8] = { r[0] & 0xfe, r[0] | 1, r[1] & 0xfe, r[1] | 1, r[2], r[3], r[4], r[5] };
    for (uint32_t i = 0; i < 8; i++) {
        mmc3x_chr(i ^ flip, values[i]);
    }
    if ((board_number == BOARD_ING022) && (m -> outer[3] & 0x20)) {
        set_ppu_nametable_mirroring((r[6] & 0x10) ? NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH : NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW);
    }
    if ((board_number == BOARD_HPXX) && (m -> outer[0] & 4)) {
        set_ppu_nametable_mirroring((m -> outer[3] & 4) ? MIRROR260_SET : MIRROR260_CLEAR);
    }
}

/*
 * 215's scramble, by $5007's bits 0-2 (the nesdev wiki's tables): the MMC3 register a write to $8000 / $8001 / $A000 /
 * ... / $E001 reaches, and the bank select index a value written to $8000 gives (its bits 6-7 kept). (Mode 4's $A001
 * and $C001 are $C001 and $C000 in Mesen 2 and FCEUX; the games that use mode 4 here, Super 308, king005, Mortal
 * Kombat 3 Extra 60 and The Super Shinobi, give the same frames either way)
 */
static const uint16_t mmc3x_215_registers[8][8] FRNES_COLD_RODATA = {
    { 0x8000, 0x8001, 0xa000, 0xa001, 0xc000, 0xc001, 0xe000, 0xe001 },
    { 0xa001, 0xa000, 0x8000, 0xc000, 0x8001, 0xc001, 0xe000, 0xe001 },
    { 0x8000, 0x8001, 0xa000, 0xa001, 0xc000, 0xc001, 0xe000, 0xe001 },
    { 0xc001, 0x8000, 0x8001, 0xa000, 0xa001, 0xe001, 0xe000, 0xc000 },
    { 0xa001, 0x8001, 0x8000, 0xc000, 0xa000, 0xc001, 0xe000, 0xe001 },
    { 0x8000, 0x8001, 0xa000, 0xa001, 0xc000, 0xc001, 0xe000, 0xe001 },
    { 0x8000, 0x8001, 0xa000, 0xa001, 0xc000, 0xc001, 0xe000, 0xe001 },
    { 0x8000, 0x8001, 0xa000, 0xa001, 0xc000, 0xc001, 0xe000, 0xe001 }
};
static const uint8_t mmc3x_215_selects[8][8] FRNES_COLD_RODATA = {
    { 0, 1, 2, 3, 4, 5, 6, 7 }, { 0, 2, 6, 1, 7, 3, 4, 5 }, { 0, 5, 4, 1, 7, 2, 6, 3 }, { 0, 6, 3, 7, 5, 2, 4, 1 },
    { 0, 2, 5, 3, 6, 1, 7, 4 }, { 0, 1, 2, 3, 4, 5, 6, 7 }, { 0, 1, 2, 3, 4, 5, 6, 7 }, { 0, 1, 2, 3, 4, 5, 6, 7 }
};

// 114's scramble (the nesdev wiki's), by submapper: the MMC3 register each of $8000 / $8001 / $A000 / ... / $E001
// reaches, and the bank select index each written index gives
static const uint16_t mmc3x_114_registers[2][8] FRNES_COLD_RODATA = {
    { 0xa001, 0xa000, 0x8000, 0xc000, 0x8001, 0xc001, 0xe000, 0xe001 },
    { 0xa001, 0x8001, 0x8000, 0xc001, 0xa000, 0xc000, 0xe000, 0xe001 }
};
static const uint8_t mmc3x_114_selects[2][8] FRNES_COLD_RODATA = {
    { 0, 3, 1, 5, 6, 7, 2, 4 }, { 0, 2, 5, 3, 6, 1, 7, 4 }
};

static void mmc3x_write(uint16_t address, uint8_t data) {
    mmc3x_state_t* m = MMC3X;
    if ((board_number == 516) && (address & 0x10)) {
        // (516: with A4 set the PAL's outer bank register, A0-A3)
        m -> outer[0] = address & 0x0f;
        ppu_sync();
        mmc3x_apply();
        return;
    }
    if (board_number == 114) {
        uint32_t sub = (board_submapper == 1) ? 1 : 0;
        address = mmc3x_114_registers[sub][((address >> 12) & 6) | (address & 1)];
        if (address == 0x8000) {
            data = (data & 0xc0) | mmc3x_114_selects[sub][data & 7];
        }
    }
    if ((board_number == 114) || ((board_number == 12) && (board_submapper == 0))) {
        // (the MMC3A's IRQ: none while the latch is 0. The game's latch and enable in outer[2-3], the MMC3's IRQ
        // kept off while the latch is 0)
        uint32_t reg = address & 0xe001;
        if (reg == 0xc000) {
            m -> outer[2] = data;
            if (m -> outer[3]) {
                mmc3_board_write(data ? 0xe001 : 0xe000, 0);
            }
        } else if ((reg == 0xe000) || (reg == 0xe001)) {
            m -> outer[3] = reg & 1;
            if ((reg == 0xe001) && !m -> outer[2]) {
                return;
            }
        }
    }
    if (board_number == BOARD_ING022) {
        // (modes 8-F: the MMC3's odd registers only, D / F all writes R6's; the IRQ latch inverted)
        uint32_t mode = m -> outer[3] & 0x0f;
        if ((mode & 9) == 9) {
            address = 0x8001;
        } else if (mode & 8) {
            address |= 1;
        }
        if (((address & 0xe001) == 0xc000) && (cartridge_mapper_number == 534)) {
            data ^= 0xff;
        }
    }
    if (board_number == 196) {
        // (the MMC3's A0 from another address line)
        uint32_t a0 = (board_submapper == 2) ? ((address >> 2) & 1) : (board_submapper != 0) ? ((address >> 1) & 1) :
            (((address & 0x0e) != 0) && !(address & 1));
        address = (address & 0xfffe) | a0;
    }
    if (board_number == 250) {
        data = address & 0xff;
        address = (address & 0xe000) | ((address >> 10) & 1);
    }
    if (board_number == 254) {
        if (address == 0x8000) {
            m -> outer[0] = 0xff;
        } else if (address == 0xa001) {
            m -> outer[1] = data;
        }
        mmc3_board_write(address, data);
        // (the MMC3's $A001 sets mapper_6000_reads for its RAM enable: the XOR's reads again while it's on)
        mapper_6000_reads = !m -> outer[0];
        if ((address & 0xe001) <= 0x8001) {
            if ((address & 0xe001) == 0x8000) {
                m -> bank_select = data;
            } else {
                m -> registers[m -> bank_select & 7] = data;
            }
            mmc3x_apply();
        }
        return;
    }
    if (board_number == BOARD_KOF97) {
        data = (data & 0xd8) | ((data >> 4) & 2) | ((data << 3) & 0x20) | ((data >> 1) & 1) | ((data << 2) & 4);
        if ((address == 0x9000) || (address == 0xd000) || (address == 0xf000)) {
            address |= 1;
            address &= 0xe001;
        }
    }
    if ((board_number == 123) && ((address & 0xe001) == 0x8000)) {
        static const uint8_t selects[8] FRNES_COLD_RODATA = { 0, 3, 1, 5, 6, 7, 2, 4 };
        data = (data & 0xc0) | selects[data & 7];
    }
    if (board_number == 187) {
        if ((address & 0xe001) == 0x8000) {
            m -> outer[1] = 1;
        } else if (((address & 0xe001) == 0x8001) && (m -> outer[1] != 1)) {
            return;
        }
    }
    if ((board_number == 44) && ((address & 0xe001) == 0xa001)) {
        m -> outer[0] = ((data & 7) == 7) ? 6 : (data & 7);
        mmc3_board_write(address, data);
        mmc3x_apply();
        return;
    }
    if (board_number == 215) {
        uint32_t row = m -> outer[2] & 7;
        address = mmc3x_215_registers[row][((address >> 12) & 6) | (address & 1)];
        if (address == 0x8000) {
            data = (data & 0xc0) | mmc3x_215_selects[row][data & 7];
        }
    }
    mmc3_board_write(address, data);
    switch (address & 0xe001) {
        case 0x8000:
            m -> bank_select = data;
            break;
        case 0x8001:
            m -> registers[m -> bank_select & 7] = data;
            break;
        default:
            return;
    }
    mmc3x_apply();
}

// an outer register write: 12's and 224's at $4020-$5FFF (board_low), 134's and the Coolboy's at $6000-$7FFF
// (board_6000), the Mindkids' at $5000-$5FFF
static void mmc3x_outer_write(uint16_t address, uint8_t data) {
    mmc3x_state_t* m = MMC3X;
    switch (board_number) {
        case 12:
            if ((address & 0xe100) != 0x4100) {
                return;
            }
            m -> outer[0] = data;
            break;
        case BOARD_411120C:
            if (address < 0x6000) {
                return;
            }
            m -> outer[0] = address & 0xff;
            break;
        case BOARD_GN45:
            if (address < 0x6000) {
                return;
            }
            if (m -> outer[1]) {
                if (WRAM != NULL) {
                    NES_WINDOW_BYTE(address) = data;
                }
                return;
            }
            if (address < 0x7000) {
                m -> outer[0] = address & 0x30;
                m -> outer[1] = (address & 0x80) != 0;
            } else {
                m -> outer[0] = data & 0x30;
            }
            break;
        case 123:
            if ((address < 0x5001) || (address >= 0x6000) || !(address & 0x800)) {
                return;
            }
            m -> outer[address & 1] = data;
            break;
        case 187:
            if ((address != 0x5000) && (address != 0x6000)) {
                if ((address >= 0x6000) && (WRAM != NULL)) {
                    NES_WINDOW_BYTE(address) = data;
                }
                return;
            }
            m -> outer[0] = data;
            break;
        case 224:
            if (address != 0x5000) {
                return;
            }
            m -> outer[0] = data;
            break;
        case 249:
            if ((address & 0xf000) != 0x5000) {
                return;
            }
            m -> outer[0] = data;
            break;
        case 205:
            if (address < 0x6000) {
                return;
            }
            if (WRAM != NULL) {
                NES_WINDOW_BYTE(address) = data;
            }
            m -> outer[0] = data & 3;
            break;
        case 267: case 377:
            if (address < 0x6000) {
                return;
            }
            m -> outer[0] = data;
            break;
        case 445:
            // ($5000-$5003 decoded with $F003; $5003 bit 5 locks them)
            if (((address & 0xf000) != 0x5000) || (m -> outer[3] & 0x20)) {
                return;
            }
            m -> outer[address & 3] = data;
            break;
        case 395:
            // (the registers over the WRAM, decoded with $E010; $6010 bit 7 locks both)
            if ((address < 0x6000) || (m -> outer[1] & 0x80)) {
                return;
            }
            m -> outer[(address >> 4) & 1] = data;
            break;
        case 126:
            if ((address < 0x6000) || ((((address & 3) == 0) || ((address & 3) == 3)) && (m -> outer[3] & 0x80))) {
                return;
            }
            m -> outer[address & 3] = data;
            break;
        case BOARD_830118C:
            if ((address & 0xff00) != 0x6800) {
                if (WRAM != NULL) {
                    NES_WINDOW_BYTE(address) = data;
                }
                return;
            }
            if (!mmc3_board_ram_writable()) {
                return;
            }
            m -> outer[0] = data;
            break;
        case BOARD_YY841101:
            if (address < 0x7000) {
                if ((address >= 0x6000) && (WRAM != NULL)) {
                    NES_WINDOW_BYTE(address) = data;
                }
                return;
            }
            m -> outer[0] = data;
            break;
        case 115: case 248: case 114:
            if (address < 0x6000) {
                return;
            }
            if (WRAM != NULL) {
                NES_WINDOW_BYTE(address) = data;
            }
            if ((address & 3) > 1) {
                return;
            }
            m -> outer[address & 1] = data;
            break;
        case BOARD_ING022:
            if (address < 0x6000) {
                return;
            }
            if (WRAM != NULL) {
                NES_WINDOW_BYTE(address) = data;
            }
            if ((m -> outer[3] & 0x80) && ((address & 3) != 2)) {
                return;
            }
            if ((address & 3) != 1) {
                m -> outer[address & 3] = data;
            }
            break;
        case 189: case 196:
            if ((board_number == 196) && ((address < 0x6000) || ((board_submapper != 0) && (board_submapper != 3)))) {
                return;
            }
            m -> outer[0] = (data >> 4) | (data & 0x0f);
            m -> outer[1] = 1;
            break;
        case BOARD_HPXX:
            if (((address & 0xf000) != 0x5000) || (m -> outer[0] & 0x80) || ((address & 3) == 3)) {
                return;
            }
            m -> outer[address & 3] = data;
            break;
        case 215:
            if ((address & 0xf000) != 0x5000) {
                return;
            }
            if ((address & 7) == 7) {
                m -> outer[2] = data & 7;
                return;
            }
            if ((address & 7) > 1) {
                return;
            }
            m -> outer[address & 7] = data;
            break;
        case 134:
            if (m -> outer[0] & 0x80) {
                if ((address & 3) != 2) {
                    return;
                }
                data = (m -> outer[2] & ~3) | (data & 3);
            }
            m -> outer[address & 3] = data;
            break;
        case BOARD_COOLBOY:
            if ((address >= 0x6000) && (MMC3_BOARD_A001 & 0x80) && (WRAM != NULL)) {
                NES_WINDOW_BYTE(address) = data;
            }
            if ((board_submapper == 1) != (address < 0x6000)) {
                return;
            }
            if ((m -> outer[3] & 0x90) == 0x80) {
                return;
            }
            m -> outer[address & 3] = data;
            break;
        default:
            return;
    }
    ppu_sync();
    mmc3x_apply();
}

// 121's override: the mode ($8003's bits 0-5) takes the reversed value
static void m121_update(void) {
    uint8_t* r = M121;
    switch (r[5] & 0x3f) {
        case 0x20: case 0x29: case 0x2b: case 0x3c: case 0x3f:
            r[7] = 1;
            r[0] = r[6];
            break;
        case 0x26:
            r[7] = 0;
            r[0] = r[6];
            break;
        case 0x2c:
            r[7] = 1;
            if (r[6]) {
                r[0] = r[6];
            }
            break;
        case 0x28:
            r[7] = 0;
            r[1] = r[6];
            break;
        case 0x2a:
            r[7] = 0;
            r[2] = r[6];
            break;
        case 0x2f:
            break;
        default:
            r[5] = 0;
            break;
    }
}

static void m121_write(uint16_t address, uint8_t data) {
    uint8_t* r = M121;
    if (address >= 0xa000) {
        mmc3x_write(address, data);
        return;
    }
    switch (address & 3) {
        case 3:
            r[5] = data;
            m121_update();
            mmc3x_write(0x8000, data);
            break;
        case 1: {
            uint32_t v = data & 0x3f;
            r[6] = (uint8_t)(((v & 1) << 5) | ((v & 2) << 3) | ((v & 4) << 1) | ((v & 8) >> 1) | ((v & 0x10) >> 3) |
                ((v & 0x20) >> 5));
            if (!r[7]) {
                m121_update();
            }
            mmc3x_write(0x8001, data);
        } break;
        default:
            mmc3x_write(0x8000, data);
            break;
    }
}

static void m121_write_low(uint16_t address, uint8_t data) {
    static const uint8_t protection[4] FRNES_COLD_RODATA = { 0x83, 0x83, 0x42, 0x00 };
    if ((address & 0xf000) != 0x5000) {
        return;
    }
    M121[4] = protection[data & 3];
    if ((address & 0x5180) == 0x5180) {
        M121[3] = data;
        ppu_sync();
        mmc3x_apply();
    }
}

/*
 * J.Y. Company's mapper 91 (JY830623C / YY840238C: Street Fighter III, Mortal Kombat II / III, Dragon Ball Z Super
 * Butouden 2, the Super HiK 4-in-1s; Mesen 2's Mapper91, the nesdev wiki's outer register): the MMC3 for its IRQ
 * counter only (mapper.c's, through mmc3_board_write; its banks are jy91_apply's), registers at $6000-$7FFF by
 * address bits 0-1 (board_6000): $6000-$6FFF the 2 kB CHR banks, $7000 / $7001 the 8 kB PRG banks at $8000 / $A000
 * (bits 0-3; $C000-$FFFF the block's last 16 kB), $7002 the IRQ off and acknowledged (the MMC3's $E000), $7003 on (its
 * latch 7, a reload and $E001: an IRQ after 8 scanlines' A12 rises). Submapper 0's outer register at $8000-$9FFF: bit 0
 * CHR A19 (512 kB), bits 1-2 PRG A17-A18 (128 kB). The mirroring the header's. Not emulated: submapper 1's (EJ-006-1,
 * Super Fighter III) M2 counter and mirroring registers, the MMC3's counter as Mesen 2's. In the register page
 * (+0x1b8-+0x1be: the CHR banks, the PRG banks, the outer register; zeroed at each load, saved with a state)
 */
typedef struct {
    uint8_t chr[4], prg[2], outer;
} jy91_state_t;
#define JY91 ((jy91_state_t*)((uint8_t*)cpu_registers + 0x1b8))

static void jy91_apply(void) {
    const jy91_state_t* j = JY91;
    uint32_t prg = (j -> outer & 6) << 3, chr = (j -> outer & 1) << 8;
    set_6502_bank_pointer(0, prg_page(prg | (j -> prg[0] & 0x0f)));
    set_6502_bank_pointer(1, prg_page(prg | (j -> prg[1] & 0x0f)));
    set_6502_bank_pointer(2, prg_page(prg | 0x0e));
    set_6502_bank_pointer(3, prg_page(prg | 0x0f));
    for (uint32_t i = 0; i < 4; i++) {
        chr_2k(i * 2, (chr | j -> chr[i]) << 1);
    }
}

static void jy91_write(uint16_t address, uint8_t data) {
    jy91_state_t* j = JY91;
    if (address < 0x7000) {
        if (j -> chr[address & 3] != data) {
            ppu_sync();
            j -> chr[address & 3] = data;
            jy91_apply();
        }
        return;
    }
    switch (address & 3) {
        case 0: case 1:
            j -> prg[address & 1] = data;
            jy91_apply();
            break;
        case 2:
            mmc3_board_write(0xe000, 0);
            break;
        case 3:
            mmc3_board_write(0xc000, 7);
            mmc3_board_write(0xc001, 0);
            mmc3_board_write(0xe001, 0);
            break;
    }
}

static void jy91_outer_write(uint16_t address, uint8_t data) {
    if ((board_submapper == 0) && (address < 0xa000)) {
        ppu_sync();
        JY91 -> outer = data;
        jy91_apply();
    }
}

__attribute__((cold, noinline)) static void jy91_init(void) {
    memset(JY91, 0, sizeof(jy91_state_t));
    mapper_6000_mode = MAPPER_6000_REGISTERS;
    board_6000 = 1;
    jy91_apply();
}

// the reset button for the boards (frnes_soft_reset): 313's outer bank, the resets since power on, steps; 260's
// registers cleared
static void mmc3x_init(void);
static void board_soft_reset(void) {
    if (board_number == 519) {
        memset(M519, 0, 3);
        m519_apply();
    }
    if (board_number == 438) {
        m438_write(0x8000, 0);
    }
    if (board_number == 449) {
        memset(M449, 0, 3);
        m449_apply();
    }
    if (board_number == 46) {
        M46[0] = M46[1] = 0;
        m46_apply();
    }
    if (board_number == BOARD_GN45) {
        MMC3X -> outer[0] = MMC3X -> outer[1] = 0;
        mmc3x_apply();
    }
    if (board_number == 246) {
        set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
    }
    if (board_number == 44) {
        MMC3X -> outer[0] = 0;
        mmc3x_apply();
    }
    if (board_number == 230) {
        M230_CONTRA = !M230_CONTRA;
        m230_mode();
    }
    if (board_number == BOARD_MMC1_RESET) {
        M374 -> game = (M374 -> game + 1) & 3;
        m374_apply();
    }
    if (board_number == BOARD_PB011003) {
        m314_reset();
    }
    if (board_number == 60) {
        M60_RESETS = (M60_RESETS + 1) & 3;
        m60_apply();
    }
    if (board_number == BOARD_RESET_TXROM) {
        MMC3X -> outer[0] = (MMC3X -> outer[0] + 1) & 3;
        mmc3x_apply();
    }
    if (board_number == 226) {
        LATCH226[0] = LATCH226[1] = 0;
        if (cartridge_mapper_number == 233) {
            M233_RESET ^= 1;
        }
        latch226_apply();
    }
    if (board_number == 236) {
        // (the mode and the outer bits cleared: the menu)
        LATCH236[1] &= 0x0f;
        if (!num_1k_VROM_pages) {
            LATCH236[0] &= ~7;
        }
        latch236_apply();
    }
    if (board_number == BOARD_YOKO) {
        CONY -> mode = CONY -> base = 0;
        cony_apply();
    }
    if (board_number == BOARD_HPXX) {
        // (its registers and lock cleared, and the MMC3's banks and IRQ as at power on, as Mesen 2's: the menu, which
        // doesn't set them again)
        mmc3_board_write(0xe000, 0);
        mmc3x_init();
    }
}

// (a reset not yet in the recording being made: frnes_reset_event's)
static uint8_t frnes_reset_pending FRNES_COLD_BSS;

/*
 * The PPU through its save state chunk (ppu.c's ppu_state_save / _load, the cold code that sets its position from
 * outside it: out of the link time optimized code here): the chunk's first fields, by value, ppu_frame_count and
 * ppu_bus_frame_count (0, 4), ppu_scanline (8), ppu_dot (12), ppu_pending_dots (16). The power on's dot and odd frame
 * (initialize_ppu's, or a movie's power on alignment: input_recording_power_on) kept at the game's start
 * (frnes_power_on_capture, from board_fresh_wram), for a reset to start the PPU as it did. Only in the builds that
 * reset (the players, the replay builds, the host): the plain test builds' link time optimization leaves the PPU's
 * state code out. The power on's dot and odd frame and a reset asked for (for the next input point) in the register
 * page (+0x1b0-+0x1b5, zeroed at each load, saved with a state)
 */
#if defined(FRNES_PLAYER_BUILD) || defined(AC_PLAYBACK) || defined(FRNES_HOST)
#define FRNES_RESET_PPU 1
#endif
#define FRNES_POWER_ON_DOT ((uint8_t*)cpu_registers + 0x1b0)
#define FRNES_POWER_ON_ODD (*((uint8_t*)cpu_registers + 0x1b4))
#define FRNES_RESET_REQUESTED (*((uint8_t*)cpu_registers + 0x1b5))
#define PPU_CHUNK_SCANLINE 8
#define PPU_CHUNK_DOT 12
#define PPU_CHUNK_PENDING 16
#define PPU_CHUNK_FRAME_READY 20
#define PPU_CHUNK_MAX 1024

#ifdef FRNES_RESET_PPU
static int ppu_chunk_save(uint8_t* buffer, uint32_t* size) {
    state_writer_t writer = { buffer, 0, PPU_CHUNK_MAX, 0 };
    ppu_state_save(&writer);
    *size = writer.size;
    return writer.failed ? -1 : 0;
}

static void frnes_power_on_capture(void) {
    uint8_t chunk[PPU_CHUNK_MAX];
    uint32_t size;
    if (ppu_chunk_save(chunk, &size) == 0) {
        memcpy(FRNES_POWER_ON_DOT, chunk + PPU_CHUNK_DOT, 4);
        FRNES_POWER_ON_ODD = ((const _PPU_registers_t*)((const uint8_t*)cpu_registers + 32)) -> ODDFRAME;
    }
}
#endif

/*
 * The reset as NesHawk has it (BizHawk's NES.Core.cs / PPU.cs / APU.cs / MOS6502X.cs, the TASVideos movies' emulator):
 * the CPU through its reset vector (stack down 3, interrupts off); the APU's $4015 written 0 and its frame counter
 * restarted from step 0 in the last $4017's mode, its IRQ flag cleared; the PPU's registers as at power on (PPUCTRL,
 * PPUMASK, the address, toggle and read buffer, $2002's flags, OAMADDR) and its timing started again as at power on
 * (its frame from scanline 0 at the power on's dot: the "dead" frame NesHawk runs before its next vblank). The front
 * loading NES resets its PPU with the CPU (nesdev's PPU power up state). The frame being emulated ends here
 * (ppu_frame_ready): NesHawk's frames end at vblank, where its reset comes, and this one's input point is there too
 * (FrNES's end after the pre-render line); the next one's input is at the vblank the reset's frame ends with
 */
FRNES_COLD_TEXT void frnes_soft_reset(void) {
    FRNES_RESET_REQUESTED = 1;
}

FRNES_COLD_TEXT void frnes_soft_reset_now(void) {
    FRNES_RESET_REQUESTED = 0;
    board_soft_reset();
    cpu_registers -> SP -= 3;
    cpu_registers -> S |= 0x04;
    cpu_registers -> NMI_Request = 0;
    // (the vector through the CPU window, the board's banks just mapped)
    cpu_registers -> PC = NES_WINDOW_BYTE(0xfffc) | (NES_WINDOW_BYTE(0xfffd) << 8);
    write_byte(0x4015, 0x00);
    write_byte(0x2000, 0x00);
    write_byte(0x2001, 0x00);
    // (the frame counter: its IRQ flag cleared by a write with the inhibit bit, then the last 4 / 5 step mode written
    // again with the inhibit clear, as Mesen 2's reset)
    extern uint8_t apu_frame_counter_mode;
    uint8_t mode = apu_frame_counter_mode & 0x80;
    write_byte(0x4017, 0x40);
    write_byte(0x4017, mode);
    _PPU_registers_t* ppu = (_PPU_registers_t*)((uint8_t*)cpu_registers + 32);
    ppu -> PPU_V = ppu -> PPU_T = 0;
    ppu -> PPU_X = ppu -> PPU_W = 0;
    ppu -> PPUDATA = 0;
    ppu -> OAMADDR = 0;
    ppu -> PPUSTATUS &= 0x1f;
#ifdef FRNES_RESET_PPU
    ppu -> ODDFRAME = FRNES_POWER_ON_ODD;
    uint8_t chunk[PPU_CHUNK_MAX];
    uint32_t size, zero = 0, one = 1, scanline;
    if (ppu_chunk_save(chunk, &size) == 0) {
        // (the frame finished when its picture is: a reset at vblank's input point; at the frame's end, after the
        // pre-render line, the frame was taken already and the next one starts again)
        memcpy(&scanline, chunk + PPU_CHUNK_SCANLINE, sizeof(scanline));
        if (scanline >= 241) {
            memcpy(chunk + PPU_CHUNK_FRAME_READY, &one, sizeof(one));
        }
        memcpy(chunk + PPU_CHUNK_SCANLINE, &zero, sizeof(zero));
        memcpy(chunk + PPU_CHUNK_DOT, FRNES_POWER_ON_DOT, 4);
        memcpy(chunk + PPU_CHUNK_PENDING, &zero, sizeof(zero));
        state_reader_t reader = { chunk, size, 0 };
        ppu_state_load(&reader, PPU_STATE_VERSION);
    }
#endif
    frnes_reset_pending = 1;
}

// (the reset asked for, at the live input point: input_poll_ports)
FRNES_COLD_TEXT void frnes_reset_input_point(void) {
    if (FRNES_RESET_REQUESTED) {
        frnes_soft_reset_now();
    }
}

FRNES_COLD_TEXT uint16_t frnes_reset_event(void) {
    uint16_t event = frnes_reset_pending ? INPUT_RECORDING_EVENT_RESET : 0;
    frnes_reset_pending = 0;
    return event;
}

__attribute__((cold, noinline)) static void mmc3x_init(void) {
    mmc3x_state_t* m = MMC3X;
    memset(m, 0, sizeof(mmc3x_state_t));
    static const uint8_t initial[8] FRNES_COLD_RODATA = { 0, 2, 4, 5, 6, 7, 0, 1 };
    memcpy(m -> registers, initial, sizeof(initial));
    if (board_number == 215) {
        m -> outer[1] = 3;
    }
    if (board_number == 198) {
        // (8 kB of battery backed PRG RAM at $6000, and 4 kB more at $5000-$5FFF after it in WRAM)
        resize_wram(0x3000);
    }
    if (board_number == 254) {
        mapper_6000_reads = 1;
    }
    if ((board_number == 134) || (board_number == BOARD_COOLBOY) || (board_number == 189) || (board_number == 196) ||
        (board_number == BOARD_ING022) || (board_number == 115) || (board_number == 248) || (board_number == 114) ||
        (board_number == 205) || (board_number == 395) || (board_number == 267) || (board_number == 377) || (board_number == BOARD_YY841101) || (board_number == BOARD_830118C) || (board_number == 126) ||
        (board_number == 187) || (board_number == BOARD_411120C) || (board_number == BOARD_GN45)) {
        mapper_6000_mode = MAPPER_6000_REGISTERS;
        board_6000 = 1;
    }
    mmc3x_apply();
}

/*
 * MMC3 multicarts with four outer bank registers (mapper 45: the Power Joy plug-and-plays, Super 8-in-1 / 1000000-in-1
 * cartridges; Mesen 2's MMC3_45): $6000-$7FFF writes go to registers 0-3 in turn until register 3's bit 6 locks them
 * (then to the WRAM). 0: CHR bits 3-10 (1 kB banks) OR'd in; 1: PRG bits 0-7 (8 kB banks) OR'd in; 2: bits 0-3 the
 * MMC3's CHR bits kept ($F all 8, 7 or less none), bits 4-7 CHR bits 11-14, bits 6-7 PRG bits 8-9 too; 3: bits 0-5 the
 * MMC3's PRG bits taken away, bit 6 the lock. Through the page tables (mmc3_outer_tables), CHR RAM untouched. In the
 * register page (+0xc4-+0xc8: the registers and the next one's index; zeroed at each load, saved with a state)
 */
#define MMC3_45_REGISTERS ((uint8_t*)cpu_registers + 0xc4)
/*
 * Mapper 52 (MMC3 multicarts: Mario 7-in-1, the ABAB / Super Cool Boy 4-in-1s...; Mesen 2's MMC3_52): one outer bank
 * register at $6000-$7FFF, written while the MMC3's $A001 enables its RAM unprotected, until its bit 7 locks it (then
 * the writes go to the WRAM). Bit 3 set: 128 kB PRG blocks (bits 0-2), clear: 256 kB (bits 1-2); bit 6 set: 128 kB
 * CHR blocks (bits 2, 4-5), clear: 256 kB (bits 2 and 5). In the register page (+0xc9, saved with a state)
 */
#define MMC3_52_REGISTER (*((uint8_t*)cpu_registers + 0xc9))
#define MMC3_45_INDEX (*((uint8_t*)cpu_registers + 0xc8))

struct mmc3_outer {
    uint32_t rom_pages, vrom_pages;
    uint8_t* rom[MAXIMUM_ROM_PAGES];
    uint8_t* vrom[MAXIMUM_VROM_PAGES];
};

// the page tables on the block 'value' selects (a save state's load: without mmc3_reapply, the window maps the banks)
static void mmc3_outer_tables(uint32_t value) {
    struct mmc3_outer* o = board_state;
    uint32_t prg_base, prg_size, chr_base;
    if (cartridge_mapper_number == 52) {
        uint32_t r = MMC3_52_REGISTER;
        uint32_t base = (r & 0x08) ? ((r & 7) << 4) : ((r & 6) << 4), mask = (r & 0x08) ? 0x0f : 0x1f;
        for (uint32_t i = 0; i < 64; i++) {
            ROM_pages[i] = o->rom[(base | (i & mask)) % o->rom_pages];
        }
        num_8k_ROM_pages = 64;
        if (o->vrom_pages) {
            uint32_t chr_base = ((r & 0x40) ? ((r & 4) | ((r >> 4) & 3)) : ((r & 4) | ((r >> 4) & 2))) << 7;
            uint32_t chr_mask = (r & 0x40) ? 0x7f : 0xff;
            for (uint32_t i = 0; i < 256; i++) {
                VROM_pages[i] = o->vrom[(chr_base | (i & chr_mask)) % o->vrom_pages];
            }
            num_1k_VROM_pages = 256;
        }
        return;
    }
    if (cartridge_mapper_number == 45) {
        const uint8_t* r = MMC3_45_REGISTERS;
        uint32_t base = r[1] | ((r[2] & 0xc0) << 2), mask = 0x3f ^ (r[3] & 0x3f);
        for (uint32_t i = 0; i < 64; i++) {
            ROM_pages[i] = o->rom[(base | (i & mask)) % o->rom_pages];
        }
        num_8k_ROM_pages = 64;
        if (o->vrom_pages) {
            uint32_t chr_base = r[0] | ((r[2] & 0xf0) << 4), chr_mask = 0xff >> (15 - (r[2] & 0x0f));
            for (uint32_t i = 0; i < 256; i++) {
                VROM_pages[i] = o->vrom[(chr_base | (i & chr_mask)) % o->vrom_pages];
            }
            num_1k_VROM_pages = 256;
        }
        return;
    }
    if (cartridge_mapper_number == 555) {
        // (a mask, not always a size: ROM_pages takes it, 32 entries, the MMC3's last banks its last)
        uint32_t base = (value & 4) << 3, mask = ((value & 3) << 3) | 7;
        for (uint32_t i = 0; i < 32; i++) {
            ROM_pages[i] = o->rom[(base | (i & mask)) % o->rom_pages];
        }
        num_8k_ROM_pages = 32;
        if (o->vrom_pages) {
            for (uint32_t i = 0; i < 128; i++) {
                VROM_pages[i] = o->vrom[((base << 2) + i) % o->vrom_pages];
            }
            num_1k_VROM_pages = 128;
        }
        return;
    }
    if (cartridge_mapper_number == 47) {
        prg_base = (value & 1) * 16;
        prg_size = 16;
        chr_base = (value & 1) * 128;
    } else {
        value &= 7;
        prg_size = ((value >= 4) && (value <= 6)) ? 16 : 8;
        prg_base = (value <= 2) ? 0 : (value == 3) ? 8 : (value == 7) ? 24 : 16;
        chr_base = (value & 4) ? 128 : 0;
    }
    for (uint32_t i = 0; i < prg_size; i++) {
        ROM_pages[i] = o->rom[(prg_base + i) % o->rom_pages];
    }
    num_8k_ROM_pages = prg_size;
    if (o->vrom_pages) {
        uint32_t chr_size = (o->vrom_pages < 128) ? o->vrom_pages : 128;
        for (uint32_t i = 0; i < chr_size; i++) {
            VROM_pages[i] = o->vrom[(chr_base + i) % o->vrom_pages];
        }
        num_1k_VROM_pages = chr_size;
    }
}

static void mmc3_outer_select(uint32_t value) {
    mmc3_outer_tables(value);
    mmc3_reapply();
}

__attribute__((cold, noinline)) void board_mmc3_outer_init() {
    struct mmc3_outer* o = board_state_alloc(sizeof(struct mmc3_outer));
    o->rom_pages = num_8k_ROM_pages;
    o->vrom_pages = num_1k_VROM_pages;
    memcpy(o->rom, ROM_pages, num_8k_ROM_pages * sizeof(ROM_pages[0]));
    memcpy(o->vrom, VROM_pages, num_1k_VROM_pages * sizeof(VROM_pages[0]));
    if (cartridge_mapper_number == 555) {
        // (its outer register at $5800: the $6000 window is its WRAM, as the MMC3's)
        memset(EVENT2, 0, sizeof(event2_state_t));
        static const uint8_t initial[8] FRNES_COLD_RODATA = { 0, 2, 4, 5, 6, 7, 0, 1 };
        memcpy(EVENT2 -> registers, initial, sizeof(initial));
        counter_timer_start();
        mmc3_outer_select(0);
        return;
    }
    if (cartridge_mapper_number == 45) {
        memset(MMC3_45_REGISTERS, 0, 5);
        MMC3_45_REGISTERS[2] = 0x0f;
    }
    MMC3_52_REGISTER = 0;
    mapper_6000_mode = MAPPER_6000_REGISTERS;
    board_6000 = 1;
    mmc3_outer_select(0);
}

// mapper 555's $5000-$5FFF (board_write_low / board_read_low)
static void event2_write_low(uint16_t address, uint8_t data) {
    event2_state_t* e = EVENT2;
    if ((address & 0xf800) == 0x5000) {
        if (WRAM_size > 0x2000) {
            WRAM[0x2000 + (address & 0x7ff) % (WRAM_size - 0x2000)] = data;
        }
    } else if ((address & 0xfc00) == 0x5800) {
        // (its register at $5800-$5BFF; $5C00-$5FFF writes are the RJ11's, to the event's score display)
        if (mapper_cpu_timer) {
            mapper_cpu_run(cpu_cycle_now);
        }
        e -> outer = data;
        if (!(data & 0x08)) {
            e -> count = 0;
        }
        mapper_cpu_timer = (data & 0x08) && (e -> count < EVENT2_TIMER_TARGET);
        mmc3_outer_select(data);
        event2_chr_ram();
    }
}

static uint8_t event2_read_low(uint16_t address, uint8_t open_bus) {
    if ((address & 0xf800) == 0x5000) {
        return (WRAM_size > 0x2000) ? WRAM[0x2000 + (address & 0x7ff) % (WRAM_size - 0x2000)] : open_bus;
    }
    if ((address & 0xf800) == 0x5800) {
        if (mapper_cpu_timer) {
            mapper_cpu_run(cpu_cycle_now);
        }
        return (EVENT2 -> count >= EVENT2_TIMER_TARGET) ? 0x80 : 0;
    }
    return open_bus;
}

static void event2_cpu_run(uint32_t cycles) {
    event2_state_t* e = EVENT2;
    if (cycles >= EVENT2_TIMER_TARGET - e -> count) {
        e -> count = EVENT2_TIMER_TARGET;
        mapper_cpu_timer = 0;
    } else {
        e -> count += cycles;
    }
}

/**
 * Irem LROG017 (mapper 77: Napoleon Senki): writes to $8000-$FFFF (with bus conflicts) select bits 0-3 the 32 kB PRG
 * bank, bits 4-7 the 2 kB CHR ROM bank at $0000; $0800-$1FFF is 6 kB of CHR RAM and the nametables are four-screen.
 */
static uint8_t irem77_prg;

static void irem77_banks(uint8_t data) {
    irem77_prg = data & 0x0f;
    prg_32k(irem77_prg);
    if (!chr_2k_mapped(0, (data >> 4) * 2)) {
        ppu_sync();
        chr_2k(0, (data >> 4) * 2);
    }
}

static void irem77_write(uint16_t address, uint8_t data) {
    irem77_banks(data & prg_page(irem77_prg * 4 + ((address >> 13) & 3))[address & 0x1fff]);
}

__attribute__((cold, noinline)) static void irem77_init() {
    irem77_banks(0);
    for (uint32_t slot = 2; slot < 8; slot++) {
        mapper_chr_ram_slots |= 1 << slot;
        set_ppu_pattern_table_bank_pointer(slot, chr_ram_page(slot - 2));
    }
    set_ppu_nametable_pages(0, 1, 2, 3);
}

/**
 * Namco 175 / 340 (mapper 210, submapper 1 / 2: Family Circuit '91, Wagan Land 2 and 3, Splatterhouse: Wanpaku
 * Graffiti): the Namco 163's banks without its sound, IRQ or nametable banking. $8000-$BFFF 1 kB CHR banks (one per
 * $800), $E000 / $E800 / $F000 8 kB PRG at $8000 / $A000 / $C000 (the last bank at $E000). 175: hardwired mirroring,
 * $C000 bit 0 enables writing its PRG RAM; 340: no PRG RAM, $E000 bits 6-7 mirroring (one screen low, vertical,
 * horizontal, one screen high).
 */
static void namco210_write(uint16_t address, uint8_t data) {
    switch (address & 0xf800) {
        case 0x8000: case 0x8800: case 0x9000: case 0x9800: case 0xa000: case 0xa800: case 0xb000: case 0xb800:
            if (!chr_1k_mapped((address - 0x8000) >> 11, data)) {
                ppu_sync();
                chr_1k((address - 0x8000) >> 11, data);
            }
            break;
        case 0xc000:
            namco175_write_enable = data & 1;
            break;
        case 0xe000:
            set_6502_bank_pointer(0, prg_page(data & 0x3f));
            if (board_submapper == 2) {
                static const uint8_t modes[4] = { NAMETABLE_MIRRORING_SINGLE_SCREEN_LOW, NAMETABLE_MIRRORING_VERTICAL,
                                                  NAMETABLE_MIRRORING_HORIZONTAL, NAMETABLE_MIRRORING_SINGLE_SCREEN_HIGH };
                ppu_sync();
                set_ppu_nametable_mirroring(modes[data >> 6]);
            }
            break;
        case 0xe800:
            set_6502_bank_pointer(1, prg_page(data & 0x3f));
            break;
        case 0xf000:
            set_6502_bank_pointer(2, prg_page(data & 0x3f));
            break;
    }
}

__attribute__((cold, noinline)) static void namco210_init() {
    namco175_write_enable = 0;
    resize_wram(0x2000);
    set_wram_bank_pointer(WRAM);
    mapper_6000_mode = MAPPER_6000_REGISTERS;
    board_6000 = 1;
    // (340 has no PRG RAM: $6000-$7FFF reads are open bus)
    mapper_6000_reads = (board_submapper == 2);
    for (uint32_t i = 0; i < 3; i++) {
        set_6502_bank_pointer(i, prg_page(i));
    }
    set_6502_bank_pointer(3, prg_page(num_8k_ROM_pages - 1));
    chr_8k(0);
}

// the CPU cycle counters above, for mapper_cpu_run / mapper_cycles_to_fire / mapper_cycles_to_event (board_timer set)
/**
 * The Vs. System (NES 2.0 console type 1, an iNES header's flags 7 bit 0), on any board: $4016 / $4017 reads take
 * the DIP switches (1-2 in $4016 bits 3-4, 3-8 in $4017 bits 2-7) and the coins ($4016 bits 5-6) in place of the open
 * bus bits (the register page's PAD_OpenMask / PADn_Extra, which 6502_readbyte.c ORs in), $4016 writes come here
 * (vs_write_4016: mapper 99's bank select) and its RGB PPU's palette replaces the NES's (vs_palette). Mapper 99 (the
 * Vs. Unisystem's own board): $4016 bit 2 selects the 8 kB CHR bank and, on 40 kB PRG boards (Vs. Gumshoe), the 8 kB
 * PRG bank at $8000 (0 or 4); its 2 kB of PRG RAM is the cartridge's WRAM. The Dual System's games run as its main
 * side ($4016 bit 7 clear) and the protection boards (Vs. hardware types other than 0 and 5) as plain ones. All of it
 * is in the register page (no .bss): the state's RAM chunk saves it, and vs_state_loaded sets it up again
 */
#include "vs_palettes.h"

// NES 2.0 byte 13's Vs. PPU types: the palette each one has (vs_palettes: the RP2C03's for the RP2C03s / RC2C03s and
// the RC2C05s, which have the same colors)
static const uint8_t vs_ppu_palette[16] FRNES_COLD_RODATA = { 0, 0, 1, 2, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

// The frontend's settings: the Vs. System's DIP switches, the Zapper's speed and port mode. Set any time, before a game
// too (frnes_run_movie's caller), and kept across games: only their setters write them, and each game's set up
// copies them into the register page or acts on them (vs_setup; the Zapper's)
#define VS_SETTING_DIPS 0
#define VS_SETTING_ZAPPER_SPEED 1
// (input.h's: NES port 1's device, port 0's, the four player mode, NES ports 0-3's sources)
#define VS_SETTING_ZAPPER_PORT 2
#define VS_SETTING_PORT0_DEVICE 3
#define VS_SETTING_FOUR_SCORE 4
#define VS_SETTING_SOURCES 5
// (the Dreamcast ports set to plug into nothing: bit n for port n)
#define VS_SETTING_DISABLED 9
static uint8_t vs_settings[10] FRNES_COLD_BSS;

#include "vs_table.h"
#define VS_INFO_KNOWN 0x10000

// NES 2.0 bytes 13 (bits 0-7: the PPU type, the hardware type above it) and 15 (bits 8-13: the default expansion
// device) for the game: its header's, or for an older header the game database's (vs_table.h, with VS_INFO_KNOWN; an
// NES game with the Zapper has VS_TABLE_NES too), 0 if it has none
// the game database table's entry for the game, whatever its header (0: none)
static FRNES_BOARD_TEXT uint32_t vs_table_lookup(uint32_t crc) {
    int32_t low = 0, high = VS_TABLE_SIZE - 1;
    while (low <= high) {
        int32_t middle = (low + high) / 2;
        if (vs_table_crcs[middle] == crc) {
            return vs_table_info[middle] | VS_INFO_KNOWN;
        }
        if (vs_table_crcs[middle] < crc) {
            low = middle + 1;
        } else {
            high = middle - 1;
        }
    }
    return 0;
}

// An old mapper 99 dump padded to 32 kB of PRG (Tetris (VS).nes: 24 kB of PRG after an 8 kB bank of one repeated
// byte, the database's C492B4D1 being the 24 kB one): the CRC32 the database has for it, without that bank (0 when
// the game isn't one). Worked out once per load, in the register page (+0x1a0, mappers 319's and 57's there: zeroed at
// each load, saved with a state; 1 when the game isn't padded), as vs_info's callers run every frame
#define VS_PADDED_CRC (*(uint32_t*)((uint8_t*)cpu_registers + 0x1a0))
// (the CRC32 input_recording_crc32 computes, bit by bit: calling that from here put its update out of line in the
// LTO units, moving the ordinary .text after it)
static FRNES_BOARD_TEXT uint32_t vs_crc32_update(uint32_t crc, const uint8_t* data, uint32_t length) {
    crc = ~crc;
    while (length--) {
        crc ^= *data++;
        for (uint32_t bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xedb88320u & -(crc & 1));
        }
    }
    return ~crc;
}
static FRNES_BOARD_TEXT uint32_t vs_padded_crc(void) {
    if ((cartridge_mapper_number != 99) || (num_8k_ROM_pages != 4)) {
        return 0;
    }
    uint32_t crc = VS_PADDED_CRC;
    if (!crc) {
        const uint8_t* fill = ROM_pages[0];
        uint32_t i = 1;
        while ((i < 0x2000) && (fill[i] == fill[0])) {
            i++;
        }
        crc = 1;
        if (i == 0x2000) {
            crc = 0;
            for (i = 1; i < 4; i++) {
                crc = vs_crc32_update(crc, ROM_pages[i], 0x2000);
            }
            for (i = 0; i < num_1k_VROM_pages; i++) {
                crc = vs_crc32_update(crc, VROM_pages[i], 0x400);
            }
            crc = crc ? crc : 1;
        }
        VS_PADDED_CRC = crc;
    }
    return (crc == 1) ? 0 : crc;
}

static FRNES_BOARD_TEXT uint32_t vs_table_entry(void) {
    uint32_t entry = vs_table_lookup(currentCRC32);
    if (!entry) {
        uint32_t padded = vs_padded_crc();
        entry = padded ? vs_table_lookup(padded) : 0;
    }
    return entry;
}

static FRNES_BOARD_TEXT uint32_t vs_info(void) {
    if ((NesHeader.byInfo2 & 0x0c) == 0x08) {
        return NesHeader.byReserve[5] | ((NesHeader.byReserve[7] & 0x3f) << 8);
    }
    return vs_table_entry();
}

// a NES 2.0 header's console type 1, or an older header that the game database knows or whose flags 7 bit 0 says so
// (with bytes 12-15 clear: not a header with a dumper's name in them)
static FRNES_BOARD_TEXT int vs_game(void) {
    if ((NesHeader.byInfo2 & 0x0c) == 0x08) {
        return (NesHeader.byInfo2 & 3) == 1;
    }
    uint32_t info = vs_info();
    if (info & VS_INFO_KNOWN) {
        return !(info & VS_TABLE_NES);
    }
    return (NesHeader.byInfo2 & 1) && !(NesHeader.byReserve[4] | NesHeader.byReserve[5] | NesHeader.byReserve[6] |
        NesHeader.byReserve[7]);
}

// player 1 on $4017 (the default expansion device: 5 is "Vs. System (1P via $4017)")
static FRNES_BOARD_TEXT int vs_swapped(void) {
    return ((vs_info() >> 8) & 0x3f) == 0x05;
}

/*
 * The RGB PPUs don't skip the odd frames' last pre-render dot (Mesen 2's Vs. frames are all 89342 dots), with the
 * PPU's hot code unchanged: the board timer, started at the game's first frame (vs_odd_timer_start), clears ODDFRAME
 * at each catch-up (board_cpu_run). ODDFRAME goes to 1 at a frame's end, after the pre-render line, and the timer has
 * the CPU's batches end at least every VS_ODD_CLEAR_CYCLES (board_cycles_to_fire, for a game that runs with the NMI
 * off), so a catch-up clears it long before the next pre-render line (a frame, ~29781 cycles, later) looks at it.
 * Only on the boards without a CPU cycle counter of their own (mapper_cpu_run's board_timer branch comes before
 * theirs): the Vs. games' 1, 2, 75, 99 and 206, and 0 / 3 / 7; Platoon (67, Sunsoft-3) keeps the skip
 */
#define VS_ODD_CLEAR_CYCLES 8192
// (the Zapper's, zapper_*)
#define ZAPPER_DOTS_PER_LINE 341
#define ZAPPER_BRIGHT 0x3fff3fff1fff19ffull
// the game's bright colours, in the register page (+0x88: zapper_setup): ZAPPER_BRIGHT, or for a Vs. game whose RGB
// PPU orders its colours its own way (the RP2C04s) the colours its palette makes bright (vs_palettes: r + g + b >= 142
// of 3 x 255, the threshold that gives the RP2C03's, the 2C02's order, as ZAPPER_BRIGHT within 2 colours)
#define ZAPPER_BRIGHT_MASK (*(uint64_t*)((uint8_t*)cpu_registers + 0x88))
#define ZAPPER_BRIGHT_SUM 142
#define ZAPPER_LIGHT_LINES 21
// (the per dot pass writes pixel x at dot x + 1, which ppu_sync() has run once the PPU is at dot x + 2: the light's
// start, a dot after Mesen 2's (a CPU cycle at most), reads this frame's pixel)
#define ZAPPER_PIXEL_DOTS 2
#define ZAPPER_NO_LIGHT 0x08
#define ZAPPER_TRIGGER 0x10
// light is sensed only around a shot: while the trigger's held and 8 frames after its release (the games look for it in
// the frames after a shot: Duck Hunt fires on the release and flashes its targets a frame or two later). Off the rest of
// the time, no light and no catch-ups, so aiming costs nothing (a game sensing light without the trigger, none known,
// would see none)
#define ZAPPER_SENSE_FRAMES 8
typedef struct {
    // the aim (y 0xff: off the screen), the phase this frame (0: before the aim point, 1: in the light window, 2:
    // after it), which Zapper (ZAPPER_PORT2 / ZAPPER_VS), the timer's clock (mapper.c's mapper_timer_cycle as
    // board_cpu_run sees it) and the next change's cycle
    uint8_t x, y, phase, kind;
    uint32_t clock, next;
    // the player's aim in 1/256 pixels and its step at full stick or d-pad (zapper_set_sensitivity: speed x 0.8
    // pixels a frame)
    uint16_t aim_x, aim_y, step;
    // the frames left to sense light: ZAPPER_SENSE_FRAMES while the trigger's held, counting down after (0: no light
    // and no timer events)
    uint8_t sense;
} zapper_state_t;
// (two: a Zapper on each port, Chiller's; slot 0 the only one's, or NES port 0's with two)
#define ZAPPER_SLOTS 2
#define ZAPPER_SLOT(i) ((zapper_state_t*)((uint8_t*)cpu_registers + 0x40 + 0x18 * (i)))
#define ZAPPER_STATE ZAPPER_SLOT(0)
_Static_assert(sizeof(zapper_state_t) <= 0x18, "zapper_state_t");
static FRNES_BOARD_TEXT void zapper_slot_aim(zapper_state_t* z, int x, int y, int trigger);
// the NES Zapper on port 2 ($4017 bits 3 / 4 through PAD2_Extra) and the Vs. System's (byte 15: 7) on $4016, 8 serial
// bits as a pad's (Mesen 2's VsZapper): bit 4 always set, bit 6 light, bit 7 trigger, in PAD1_Latch. The frame's
// input gives the latch (ZAPPER_VS_BASE | trigger << 7, what a movie records); the light's bit is set here
#define ZAPPER_PORT2 1
#define ZAPPER_VS 2
// (the NES Zapper on port 1: $4016 bits 3 / 4 through PAD1_Extra, as port 2's through PAD2_Extra)
#define ZAPPER_PORT1 3
// the NES port a Zapper's on
#define ZAPPER_NES_PORT(z) (((z) -> kind == ZAPPER_PORT2) ? 1 : 0)
#define ZAPPER_VS_BASE 0x10
#define ZAPPER_VS_LIGHT 0x40
#define ZAPPER_VS_TRIGGER 0x80
// (NES pad bits: Select and Start, which stay the pad's while its controller drives the Zapper; Start)
#define NES_PAD_START_SELECT 0x0c
#define NES_PAD_START 0x08
// 0.8 pixels a frame in 1/256 pixels for each step of speed; the stick's dead zone (its range is -128 to 127)
#define ZAPPER_SPEED_STEP(speed) ((uint16_t)((speed) * 205))
#define ZAPPER_STICK_DEAD_ZONE 24
// the PPU's registers through cpu_registers (ppu_registers is the register page's + 32 as well: memory.c): a reference to
// ppu_registers from outside the link time optimized code makes it visible there and changes the PPU's code (3_sync)
#define VS_PPU_REGISTERS ((_PPU_registers_t*)((uint8_t*)cpu_registers + 32))
static FRNES_BOARD_TEXT int vs_rgb_timer_board(void) {
    switch (cartridge_mapper_number) {
        case 0: case 1: case 2: case 3: case 7: case 75: case 99: case 151: case 206:
        // (and the Zapper games' 4, 5, 11, 33, 66, whose counters aren't CPU cycle ones)
        case 4: case 5: case 11: case 33: case 66:
            return 1;
    }
    return 0;
}

// the board timer for the odd frames, from the first frame's input (vs_frame_input, vs_recording_event: before the
// first odd frame's pre-render line) or $4016 write, whichever comes first (mapper_init stops the timers after
// board_override)
static FRNES_BOARD_TEXT void vs_odd_timer_start(void) {
    if (!board_timer && vs_rgb_timer_board()) {
        board_timer = 1;
        counter_timer_start();
        mapper_cpu_timer = 1;
        ZAPPER_SLOT(0) -> clock = ZAPPER_SLOT(1) -> clock = cpu_cycle_now;
    }
}

/*
 * The Zapper on port 2 (NES 2.0 byte 15, the default expansion device: 8). $4017 reads give bit 3 clear while light
 * is sensed and bit 4 set while the trigger is pulled, nothing serial (6502_readbyte.c: the pad's bit, 0 for an empty
 * latch, then open bus & 0xe0 | PAD2_Extra): no code of its own in the CPU's paths. Light (Mesen 2's rule): the
 * beam between the aim point and 20 lines below it, past x on the aim line itself, with the aim pixel (this frame's)
 * bright: r + g + b >= 85 in Mesen's 2C02 palette (ZAPPER_BRIGHT, colour index bit set; emphasis left out). So the
 * light bit changes at two moments a frame, the beam reaching (x, y) and line y + 21: the board timer
 * (vs_odd_timer_start's) ends a batch there (zapper_cycles_to_event), and its catch-up (zapper_run) brings the PPU
 * up to the moment, reads the pixel and sets PAD2_Extra. Its state (no .bss) is in the register page past the PPU's
 * registers (the state's RAM chunk saves it). The aim and trigger are the frame's (zapper_aim)
 */

// which Zapper the game has: ZAPPER_PORT2, ZAPPER_VS or 0. By its default expansion device (vs_info: the NES 2.0
// header's byte 15, or for an older header the game database's), or for an NES game the frontend's port mode
// (zapper_set_port_mode: ZAPPER_PORT_AUTO the device's, ZAPPER_PORT_CONTROLLER none, ZAPPER_PORT_ZAPPER the NES Zapper
// on port 2); a Vs. game's is its device's
static FRNES_BOARD_TEXT int zapper_kind(void) {
    uint8_t device = (vs_info() >> 8) & 0x3f;
    if (cpu_registers -> VS_Flags & VS_SYSTEM) {
        return (device == 0x07) ? ZAPPER_VS : 0;
    }
    // (an Arkanoid paddle game has the paddle, whatever port 2's set to: Matt)
    if ((device >= 0x0f) && (device <= 0x11)) {
        return 0;
    }
    switch (vs_settings[VS_SETTING_ZAPPER_PORT]) {
        case ZAPPER_PORT_CONTROLLER:
            return 0;
        case ZAPPER_PORT_ZAPPER:
            return ZAPPER_PORT2;
    }
    // (8: the Zapper on port 2; 9: one on each port)
    return ((device == 0x08) || (device == 0x09)) ? ZAPPER_PORT2 : 0;
}

// NES port 0's Zapper (ZAPPER_PORT1) for an NES game: the second of two (default expansion device 9: Chiller), or
// the frontend's port 0 device (input_set_port_device)
static FRNES_BOARD_TEXT int zapper_port0(void) {
    if (cpu_registers -> VS_Flags & VS_SYSTEM) {
        return 0;
    }
    switch (vs_settings[VS_SETTING_PORT0_DEVICE]) {
        case INPUT_DEVICE_PAD:
            return 0;
        case INPUT_DEVICE_ZAPPER:
            return 1;
    }
    return ((vs_info() >> 8) & 0x3f) == 0x09;
}

/*
 * The Vs. System's protection (NES 2.0 byte 13's hardware type, or the game database's: vs_info), as Mesen 2 has it.
 * R.B.I. Baseball (1) and T.K.O. Boxing (2): a security chip at $5E00 / $5E01, a $5E00 read restarting its 32 byte
 * sequence and each $5E01 read giving its next byte; Super Xevious (3): every other $4020-$5FFF read giving the next of
 * its own; the rest of $4020-$5FFF reads 0 on those boards. Ice Climber's (4): the pads' Start bit always set
 * (input_strobe_fall). The sequences' position in the register page (+0x80: 0 at each load)
 */
#define VS_PROTECTION_POSITION ((uint8_t*)cpu_registers + 0x80)
static const uint8_t vs_protection_sequences[3][32] FRNES_COLD_RODATA = {
    // T.K.O. Boxing
    { 0xff, 0xbf, 0xb7, 0x97, 0x97, 0x17, 0x57, 0x4f, 0x6f, 0x6b, 0xeb, 0xa9, 0xb1, 0x90, 0x94, 0x14,
      0x56, 0x4e, 0x6f, 0x6b, 0xeb, 0xa9, 0xb1, 0x90, 0xd4, 0x5c, 0x3e, 0x26, 0x87, 0x83, 0x13, 0x00 },
    // R.B.I. Baseball
    { 0x00, 0x00, 0x00, 0x00, 0xb4, 0x00, 0x00, 0x00, 0x00, 0x6f, 0x00, 0x00, 0x00, 0x00, 0x94, 0x00 },
    // Super Xevious
    { 0x05, 0x01, 0x89, 0x37, 0x05, 0x00, 0xd1, 0x3e },
};

// the game's protection: its Vs. hardware type 1-4, 0 for none (or not a Vs. game)
static FRNES_BOARD_TEXT uint32_t vs_protection(void) {
    if (!(cpu_registers -> VS_Flags & VS_SYSTEM)) {
        return 0;
    }
    uint32_t hardware = (vs_info() >> 4) & 0x0f;
    return (hardware <= 4) ? hardware : 0;
}

// the boards with a protection read at $4020-$5FFF (namco108_init's: mapper 206's, board_low)
FRNES_BOARD_TEXT int vs_protection_reads(void) {
    uint32_t protection = vs_protection();
    return (protection >= 1) && (protection <= 3);
}

static FRNES_BOARD_TEXT uint8_t vs_read_low(uint16_t address) {
    uint32_t protection = vs_protection();
    uint8_t* position = VS_PROTECTION_POSITION;
    if (address == 0x5e00) {
        *position = 0;
        return 0;
    }
    if (address == 0x5e01) {
        return (protection == 3) ? 0 : vs_protection_sequences[(protection == 2) ? 0 : 1][(*position)++ & 0x1f];
    }
    return (protection == 3) ? vs_protection_sequences[2][(*position)++ & 0x1f] : 0;
}

// the game's four players (input.h): the NES Four Score (NES 2.0 byte 15's default expansion device 2) or the Famicom's
// four player adapter (3), by the frontend's mode; none on the Vs. System
static FRNES_BOARD_TEXT void input_setup(void) {
    uint8_t device = (vs_info() >> 8) & 0x3f, active = INPUT_FOUR_SCORE_NONE;
    switch (vs_settings[VS_SETTING_FOUR_SCORE]) {
        case INPUT_FOUR_SCORE_AUTO:
            active = (device == 0x02) ? INPUT_FOUR_SCORE_NES : ((device == 0x03) ? INPUT_FOUR_SCORE_FAMICOM : INPUT_FOUR_SCORE_NONE);
            break;
        case INPUT_FOUR_SCORE_ON:
            active = (device == 0x03) ? INPUT_FOUR_SCORE_FAMICOM : INPUT_FOUR_SCORE_NES;
            break;
    }
    PAD_INPUT -> four_score = (cpu_registers -> VS_Flags & VS_SYSTEM) ? INPUT_FOUR_SCORE_NONE : active;
    PAD_INPUT -> strobe_fall = (PAD_INPUT -> four_score ? PAD_STROBE_FOUR_SCORE : 0) | ((vs_protection() == 4) ? PAD_STROBE_VS_START : 0);
}

// (after the Vs. System's set up, so its VS_SYSTEM is known)
static FRNES_BOARD_TEXT void zapper_setup(void) {
    // (the slots' kinds: with a Zapper on each port, port 0's first)
    int kinds[ZAPPER_SLOTS] = { 0, 0 }, slots = 0;
    if (zapper_port0()) {
        kinds[slots++] = ZAPPER_PORT1;
    }
    if (zapper_kind()) {
        kinds[slots++] = zapper_kind();
    }
#ifdef FRNES_PLAYER_BUILD
    zapper_gun_report();
    // (the previous game's reticle off)
    if (frame_hud_overlay == zapper_reticle_draw) {
        frame_hud_overlay = NULL;
    }
#endif
    if (!slots || !vs_rgb_timer_board()) {
        return;
    }
    cpu_registers -> VS_Flags |= VS_ZAPPER;
    uint64_t bright = ZAPPER_BRIGHT;
    uint32_t palette = vs_game() ? vs_ppu_palette[vs_info() & 0x0f] : 0;
    if (palette) {
        bright = 0;
        for (uint32_t i = 0; i < 64; i++) {
            uint32_t c = vs_palettes[palette][i];
            uint32_t sum = ((c >> 11) & 31) * 255 / 31 + ((c >> 5) & 63) * 255 / 63 + (c & 31) * 255 / 31;
            bright |= (uint64_t)(sum >= ZAPPER_BRIGHT_SUM) << i;
        }
    }
    ZAPPER_BRIGHT_MASK = bright;
    uint8_t speed = vs_settings[VS_SETTING_ZAPPER_SPEED];
    for (int i = 0; i < ZAPPER_SLOTS; i++) {
        zapper_state_t* z = ZAPPER_SLOT(i);
        memset(z, 0, sizeof(*z));
        if (i >= slots) {
            continue;
        }
        int kind = kinds[i];
        printf(" ...Zapper %s\n", (kind == ZAPPER_VS) ? "(Vs. System) on $4016" : ((kind == ZAPPER_PORT1) ? "on port 1" :
            "on port 2"));
        if (kind == ZAPPER_PORT2) {
            cpu_registers -> PAD2_Extra = ZAPPER_NO_LIGHT;
        } else if (kind == ZAPPER_PORT1) {
            cpu_registers -> PAD1_Extra = ZAPPER_NO_LIGHT;
        }
        z -> y = 0xff;
        z -> kind = kind;
        // (the screen's middle; the frontend's speed, zapper_set_sensitivity, 1-10, 0 taken as 5)
        z -> aim_x = 128 << 8;
        z -> aim_y = 120 << 8;
        z -> step = ZAPPER_SPEED_STEP((speed == 0) ? 5 : ((speed > 10) ? 10 : speed));
    }
    // (a game whose Zapper is an option, its Zapper on port 2 by auto: off at the start, the game menu turns it on:
    // gen_vs_table.py's ZAPPER_OPTIONAL)
    if ((vs_settings[VS_SETTING_ZAPPER_PORT] == ZAPPER_PORT_AUTO) && (vs_table_entry() & VS_TABLE_ZAPPER_OFF) &&
        (ZAPPER_SLOT(slots - 1) -> kind == ZAPPER_PORT2)) {
        PAD_INPUT -> zapper_off |= 2;
    }
#ifdef FRNES_PLAYER_BUILD
    frame_hud_overlay = zapper_reticle_draw;
#endif
}


/*
 * The Arkanoid (Vaus) paddle: a knob's potentiometer (VAUS_MIN-VAUS_MAX, left to right) read out serially, MSB first and
 * inverted, after the $4016 strobe's fall latches it, and a fire button. The NES one on $4017: bit 4 the serial bit,
 * bit 3 the button, PAD2_Extra's (the Zapper's bits); the Famicom one on the expansion port: $4017 bit 1 the serial
 * bit, $4016 bit 1 the button, PAD1_Extra / PAD2_Extra too. The serial bit changes with each $4017 read, which
 * nothing on the read path's hot code does: from the strobe's fall (vaus_strobe, the input_strobe_fall hook) until 8
 * bits are read, the board timer ends a batch at every instruction (board_cycles_to_fire 1) and its catch-up (vaus_run)
 * puts the next bit in PAD2_Extra for the read PAD2_Bit counts. Only on the boards
 * without a CPU cycle counter of their own (vs_rgb_timer_board's, and Arkanoid II's 152). By the game's default
 * expansion device (vs_info: 15 the NES one, 16 / 17 the Famicom one: whatever port 2 is set to, a Zapper there left
 * out); the frontend's Paddle setting picks its device (input_ports.c), in those games only. The knob and the button are the frame's
 * (vaus_player_input: a mouse or a controller; movies: input_ext_apply)
 */
#define VAUS_NES 1
#define VAUS_FAMICOM 2
// (the knob's travel: wide enough for Arkanoid II's left wall, which 98 didn't reach (Matt); Arkanoid stops at its own
// walls inside it, 98-242)
#define VAUS_MIN 0x48
#define VAUS_MAX 0xf4
static struct {
    uint8_t kind, fire, latched, window;
    // the knob, VAUS_MIN-VAUS_MAX in 1/256ths
    int32_t knob;
    uint32_t strobe_cycle;
} vaus FRNES_COLD_BSS;
// (a poll's 8 reads come well inside this)
#define VAUS_WINDOW_CYCLES 2000

static FRNES_BOARD_TEXT int vaus_kind(void) {
    if (cpu_registers -> VS_Flags & (VS_SYSTEM | VS_ZAPPER)) {
        return 0;
    }
    uint8_t device = (vs_info() >> 8) & 0x3f;
    // (only the Arkanoid games, whatever port 2's set to: a port set to Paddle is the paddle's device in them, not a
    // paddle in every game, Matt)
    return (device == 0x0f) ? VAUS_NES : (((device == 0x10) || (device == 0x11)) ? VAUS_FAMICOM : 0);
}

static FRNES_BOARD_TEXT int vaus_timer_board(void) {
    return vs_rgb_timer_board() || (cartridge_mapper_number == 152);
}

// (the board timer, from the first strobe: mapper_init stops the timers after board_override)
static FRNES_BOARD_TEXT void vaus_timer_start(void) {
    if (!board_timer && vaus_timer_board()) {
        board_timer = 1;
        counter_timer_start();
        mapper_cpu_timer = 1;
    }
}

// the serial bit for the $4017 read PAD2_Bit counts, 1s after the 8th (Mesen 2's: the shift register's inverted 0s),
// in PAD2_Extra's bit 4 (NES) or bit 1 (Famicom); the button in $4017's bit 3 (NES) or $4016's bit 1 (Famicom: PAD1_Extra,
// whatever the read: Arkanoid II reads it past the pad's 8 bits)
static FRNES_BOARD_TEXT void vaus_bit(void) {
    uint32_t bit = cpu_registers -> PAD2_Bit;
    uint8_t serial = (bit < 8) ? ((~vaus.latched >> (7 - bit)) & 1) : 1;
    if (vaus.kind == VAUS_NES) {
        cpu_registers -> PAD2_Extra = (vaus.fire ? 0x08 : 0) | (serial << 4);
    } else {
        cpu_registers -> PAD2_Extra = serial << 1;
        cpu_registers -> PAD1_Extra = vaus.fire ? 0x02 : 0;
    }
    // (until the 8 bits are read, or VAUS_WINDOW_CYCLES after the strobe: a game that reads fewer leaves no batch
    // at every instruction for the rest of the frame)
    vaus.window = (bit < 8) && ((uint32_t)(cpu_cycle_now - vaus.strobe_cycle) < VAUS_WINDOW_CYCLES);
}

// the strobe's fall (input_strobe_fall): the knob latched, its bits read out one by one from here
FRNES_BOARD_TEXT void vaus_strobe(void) {
    // (the board timer, at the first strobe: mapper_init stops the timers after board_override)
    vaus_timer_start();
    vaus.latched = (uint8_t)(vaus.knob >> 8);
    vaus.strobe_cycle = cpu_cycle_now;
    vaus_bit();
    // (a batch at every instruction from here until the 8 bits are read)
    cpu_cut_batch_for_event(1);
}

// the board timer's catch-up (board_cpu_run): the paddle's next bit
static FRNES_BOARD_TEXT void vaus_run(void) {
    if (vaus.window) {
        vaus_bit();
    }
}

// the game's set up (vs_setup, NES games: after input_setup and zapper_setup)
static FRNES_BOARD_TEXT void vaus_setup(void) {
    int kind = vaus_kind();
    int32_t knob = vaus.knob;
    memset(&vaus, 0, sizeof(vaus));
    vaus.knob = ((knob >> 8) >= VAUS_MIN) && ((knob >> 8) <= VAUS_MAX) ? knob : (((VAUS_MIN + VAUS_MAX) / 2) << 8);
    if (!kind) {
        return;
    }
    if (!vaus_timer_board()) {
        printf(" ...Arkanoid paddle: not on mapper %lu's board\n", (unsigned long)cartridge_mapper_number);
        return;
    }
    vaus.kind = kind;
    cpu_registers -> VS_Flags |= VS_VAUS;
    PAD_INPUT -> strobe_fall |= PAD_STROBE_VAUS;
    printf(" ...Arkanoid paddle (%s)\n", (kind == VAUS_NES) ? "NES, port 2" : "Famicom, expansion port");
}

// the knob and the button for the frame (a movie's: input_ext_apply; the player's: vaus_player_input)
FRNES_BOARD_TEXT void vaus_set(uint8_t knob, int fire) {
    vaus.knob = (int32_t)((knob < VAUS_MIN) ? VAUS_MIN : ((knob > VAUS_MAX) ? VAUS_MAX : knob)) << 8;
    vaus.fire = fire != 0;
}

FRNES_BOARD_TEXT uint8_t vaus_knob(void) {
    return (uint8_t)(vaus.knob >> 8);
}

FRNES_BOARD_TEXT int vaus_fire(void) {
    return vaus.fire;
}

#ifdef FRNES_PLAYER_BUILD
/*
 * The player's paddle: a mouse's left and right movement turns the knob (VAUS_MOUSE_STEP 1/256ths a count), any of
 * its buttons fires; or a controller's analog stick (speed with its deflection) and D-pad (full speed) turn it, A fires
 */
#ifndef VAUS_MOUSE_STEP
#define VAUS_MOUSE_STEP 96
#endif
#define VAUS_PAD_STEP (3 << 8)
static FRNES_BOARD_TEXT void vaus_player_input(void) {
    maple_device_t* device = (maple_device_t*)input_vaus_device();
    void* status = device ? maple_dev_status(device) : NULL;
    if (status == NULL) {
        vaus.fire = 0;
        return;
    }
    int32_t knob = vaus.knob;
    if (device -> info.functions & MAPLE_FUNC_MOUSE) {
        const mouse_state_t* m = (const mouse_state_t*)status;
        knob += m -> dx * VAUS_MOUSE_STEP;
        // (any of its buttons: Matt)
        vaus.fire = (m -> buttons & MOUSE_BUTTONS) != 0;
    } else {
        const cont_state_t* state = (const cont_state_t*)status;
        knob += zapper_stick(state -> joyx, VAUS_PAD_STEP);
        knob += ((state -> buttons & CONT_DPAD_RIGHT) ? VAUS_PAD_STEP : 0) - ((state -> buttons & CONT_DPAD_LEFT) ? VAUS_PAD_STEP : 0);
        vaus.fire = (state -> buttons & CONT_A) != 0;
    }
    knob = (knob < (VAUS_MIN << 8)) ? (VAUS_MIN << 8) : ((knob > (VAUS_MAX << 8)) ? (VAUS_MAX << 8) : knob);
    vaus.knob = knob;
    // (the button, read before the next strobe too)
    if (vaus.kind == VAUS_FAMICOM) {
        cpu_registers -> PAD1_Extra = vaus.fire ? 0x02 : 0;
    } else {
        cpu_registers -> PAD2_Extra = (cpu_registers -> PAD2_Extra & 0x10) | (vaus.fire ? 0x08 : 0);
    }
}
#endif

#ifdef FRNES_PLAYER_BUILD
/*
 * The player's Zapper (Matt, 2026-10-28): the analog stick (speed with its deflection, past a dead zone) and the
 * d-pad (full speed) move the reticle, A fires and B fires off the screen (to reload, in the games that ask for it).
 * The first controller's state from maple, as the frame loop's pad_state reads it (the d-pad and A also reach the
 * NES pad 1, which the NES Zapper games read for their menus). The reticle is drawn over the picture through
 * frame_hud_overlay (frame_hud.c, the frame display's translucent list), mapped through video_picture
 */
static FRNES_BOARD_TEXT int32_t zapper_stick(int32_t value, int32_t step) {
    if (value > ZAPPER_STICK_DEAD_ZONE) {
        return (value - ZAPPER_STICK_DEAD_ZONE) * step / (127 - ZAPPER_STICK_DEAD_ZONE);
    }
    if (value < -ZAPPER_STICK_DEAD_ZONE) {
        return (value + ZAPPER_STICK_DEAD_ZONE) * step / (128 - ZAPPER_STICK_DEAD_ZONE);
    }
    return 0;
}

/*
 * The Dreamcast light gun as the Zapper (CRT televisions only: it times the beam). Its trigger is the controller's A,
 * B fires off the screen. Each frame asks maple to latch the beam's position for the next one (KOS's gun mode: the
 * PVR's H and V counters when the gun saw the beam, read after the frame's maple DMA) and takes the one it
 * latched last: aiming at something dark latches nothing, so the aim stays where it was, and the emulated Zapper
 * senses that pixel of the NES picture as the NES Zapper would. The counters to the screen: the H counter in pixels
 * from the mode's bitmapx (one a pixel in the 640 wide modes) less ZAPPER_GUN_X for the gun's lag, the V counter in
 * lines from bitmapy (a field's in an interlaced mode: doubled) plus ZAPPER_GUN_Y; then to the NES picture through
 * video_picture's corners
 */
// (BLUE's gun on a composite TV, 640x480 NTSC interlaced, 2026-11-05: aimed at the 2x picture's middle it read H 591
// (bitmapx 164: 427 for screen x 320), at its right edge 9 (867: 703 for 576), V 139 for the middle, 233 near the
// bottom)
#ifndef ZAPPER_GUN_X
#define ZAPPER_GUN_X 125
#endif
#ifndef ZAPPER_GUN_Y
#define ZAPPER_GUN_Y 0
#endif
// (the screen round the NES picture while a gun aims, zapper_reticle_draw: 60% grey, opaque)
#ifndef ZAPPER_MOUSE_STEP
#define ZAPPER_MOUSE_STEP 128
#endif
#ifndef ZAPPER_GUN_GREY
#define ZAPPER_GUN_GREY 0xff999999
#endif
#ifndef ZAPPER_GUN_X_SCALE
#define ZAPPER_GUN_X_SCALE 1.0f
#endif
// (the gun's aim on the screen, for the reticle: off the NES picture too; its counters' range and the last pulls,
// printed at the next game's start, as the console is quiet while a game runs: calibration)
static struct {
    float sx, sy;
    uint8_t latched, pulled;
    uint32_t frames, pulls;
    int16_t h_min, h_max, v_min, v_max;
    int16_t pull_hv[16][2];
} zapper_gun FRNES_COLD_BSS;

// (the frontend's calibration, zapper_gun_set_calibration: x_scale 0 for ZAPPER_GUN_X's)
static struct {
    int16_t x_scale, x_offset, y_scale, y_offset;
    uint8_t hide_reticle;
} zapper_gun_calibration FRNES_COLD_BSS;

FRNES_BOARD_TEXT void zapper_gun_set_calibration(int16_t x_scale, int16_t x_offset, int16_t y_scale, int16_t y_offset,
    uint8_t hide_reticle) {
    zapper_gun_calibration.hide_reticle = hide_reticle;
    zapper_gun_calibration.x_scale = x_scale;
    zapper_gun_calibration.x_offset = x_offset;
    zapper_gun_calibration.y_scale = y_scale;
    zapper_gun_calibration.y_offset = y_offset;
}

static FRNES_BOARD_TEXT void zapper_gun_report(void) {
    if (zapper_gun.latched) {
        printf(" ...light gun last game: H %d-%d, V %d-%d (bitmap x %d y %d), %lu pulls:", zapper_gun.h_min,
            zapper_gun.h_max, zapper_gun.v_min, zapper_gun.v_max, (int)vid_mode -> bitmapx, (int)vid_mode -> bitmapy,
            (unsigned long)zapper_gun.pulls);
        printf(" picture x %d-%d y %d-%d;", (int)video_picture.x1, (int)video_picture.x2, (int)video_picture.y1,
            (int)video_picture.y2);
        for (uint32_t i = 0; (i < zapper_gun.pulls) && (i < 16); i++) {
            printf(" %d,%d", zapper_gun.pull_hv[i][0], zapper_gun.pull_hv[i][1]);
        }
        // (puts, which the link has: a printf of a lone newline is putchar, which would come into ordinary .text)
        puts("");
    }
    memset(&zapper_gun, 0, sizeof(zapper_gun));
}

static FRNES_BOARD_TEXT void zapper_gun_input(zapper_state_t* z, maple_device_t* device, const cont_state_t* state) {
    // (maple_gun_read_pos's and maple_gun_enable's work, on KOS's maple state itself: calling them would link
    // maple_utils.c's two functions into ordinary .text, moving the KOS code after them)
    int gx = maple_state.gun_x, gy = maple_state.gun_y;
    // (every other frame: the gun answers no condition request in a frame it's latching for, so its buttons are
    // taken from the frames between)
    if (++zapper_gun.frames & 1) {
        maple_state.gun_port = device -> port;
    }
    int pulled = (state -> buttons & CONT_A) != 0;
    int off = 0;
    // (nothing latched yet: KOS's -1, or 0)
    if ((gx > 0) && (gy > 0)) {
        if (!zapper_gun.latched) {
            zapper_gun.h_min = zapper_gun.h_max = gx;
            zapper_gun.v_min = zapper_gun.v_max = gy;
        }
        zapper_gun.latched = 1;
        zapper_gun.h_min = (gx < zapper_gun.h_min) ? gx : zapper_gun.h_min;
        zapper_gun.h_max = (gx > zapper_gun.h_max) ? gx : zapper_gun.h_max;
        zapper_gun.v_min = (gy < zapper_gun.v_min) ? gy : zapper_gun.v_min;
        zapper_gun.v_max = (gy > zapper_gun.v_max) ? gy : zapper_gun.v_max;
        const video_picture_t* p = &video_picture;
        // (the gun latches about ZAPPER_GUN_X counts late, so the right of the picture comes past the line's end and
        // reads from 0: a count before the picture's start is the next line's)
        int h = (gx < (int)vid_mode -> bitmapx) ? (gx + vid_mode -> clocks + 1) : gx;
        int set = zapper_gun_calibration.x_scale != 0;
        float x_scale = set ? zapper_gun_calibration.x_scale / 1000.0f : ZAPPER_GUN_X_SCALE;
        float x_offset = set ? (float)zapper_gun_calibration.x_offset : -(float)ZAPPER_GUN_X;
        float y_scale = set ? zapper_gun_calibration.y_scale / 1000.0f : 1.0f;
        float y_offset = set ? (float)zapper_gun_calibration.y_offset : (float)ZAPPER_GUN_Y;
        float sx = (float)(h - (int)vid_mode -> bitmapx) * x_scale + x_offset;
        float sy = (float)(gy - (int)vid_mode -> bitmapy) * ((vid_mode -> flags & VID_INTERLACE) ? 2.0f : 1.0f) * y_scale + y_offset;
        zapper_gun.sx = sx;
        zapper_gun.sy = sy;
        // (off the NES picture: a shot there misses everything, as B's does)
        off = (sx < p -> x1) || (sx >= p -> x2) || (sy < p -> y1) || (sy >= p -> y2);
        // (the screen point's texel, 256 x 256 texels across u and v 0-1)
        float u = p -> u1 + (sx - p -> x1) * (p -> u2 - p -> u1) / (p -> x2 - p -> x1);
        float v = p -> v1 + (sy - p -> y1) * (p -> v2 - p -> v1) / (p -> y2 - p -> y1);
        int32_t x = (int32_t)(u * 256.0f * 256.0f), y = (int32_t)(v * 256.0f * 256.0f);
        x = (x < 0) ? 0 : ((x > (255 << 8)) ? (255 << 8) : x);
        y = (y < 0) ? 0 : ((y > (239 << 8)) ? (239 << 8) : y);
        z -> aim_x = (uint16_t)x;
        z -> aim_y = (uint16_t)y;
    }
    if (pulled && !zapper_gun.pulled) {
        if (zapper_gun.pulls < 16) {
            zapper_gun.pull_hv[zapper_gun.pulls][0] = gx;
            zapper_gun.pull_hv[zapper_gun.pulls][1] = gy;
        }
        zapper_gun.pulls++;
    }
    zapper_gun.pulled = pulled;
    int fire = (state -> buttons & (CONT_A | CONT_B)) != 0;
    zapper_slot_aim(z, z -> aim_x >> 8, (off || (state -> buttons & CONT_B)) ? -1 : (z -> aim_y >> 8), fire);
    if (z -> kind == ZAPPER_VS) {
        cpu_registers -> PAD1_Latch = ZAPPER_VS_BASE | (fire ? ZAPPER_VS_TRIGGER : 0);
    }
}

// one Zapper's frame from its controller (NULL: none, idle)
static FRNES_BOARD_TEXT void zapper_slot_input(zapper_state_t* z, maple_device_t* device) {
    uint8_t port = ZAPPER_NES_PORT(z);
    // (turned off, zapper_set_enabled: idle, and the controller's buttons the pad's)
    cont_state_t* state = device ? (cont_state_t*)maple_dev_status(device) : NULL;
    if ((PAD_INPUT -> zapper_off & (1 << port)) || (state == NULL)) {
        zapper_slot_aim(z, z -> aim_x >> 8, -1, 0);
        if (z -> kind == ZAPPER_VS) {
            cpu_registers -> PAD1_Latch = ZAPPER_VS_BASE;
        }
        return;
    }
    if (device -> info.functions & MAPLE_FUNC_LIGHTGUN) {
        zapper_gun_input(z, device, state);
        return;
    }
    // (a mouse: its movement moves the reticle, ZAPPER_MOUSE_STEP 1/256ths of a pixel a count, its buttons fire)
    if (device -> info.functions & MAPLE_FUNC_MOUSE) {
        const mouse_state_t* m = (const mouse_state_t*)state;
        int32_t x = (int32_t)z -> aim_x + m -> dx * ZAPPER_MOUSE_STEP, y = (int32_t)z -> aim_y + m -> dy * ZAPPER_MOUSE_STEP;
        x = (x < 0) ? 0 : ((x > (255 << 8)) ? (255 << 8) : x);
        y = (y < 0) ? 0 : ((y > (239 << 8)) ? (239 << 8) : y);
        z -> aim_x = (uint16_t)x;
        z -> aim_y = (uint16_t)y;
        // (any of its buttons is the trigger: Matt)
        int fire = (m -> buttons & MOUSE_BUTTONS) != 0;
        zapper_slot_aim(z, x >> 8, y >> 8, fire);
        if (z -> kind == ZAPPER_VS) {
            cpu_registers -> PAD1_Latch = ZAPPER_VS_BASE | (fire ? ZAPPER_VS_TRIGGER : 0);
        }
        return;
    }
    int32_t step = z -> step;
    int32_t dx = zapper_stick(state -> joyx, step), dy = zapper_stick(state -> joyy, step);
    dx += ((state -> buttons & CONT_DPAD_RIGHT) ? step : 0) - ((state -> buttons & CONT_DPAD_LEFT) ? step : 0);
    dy += ((state -> buttons & CONT_DPAD_DOWN) ? step : 0) - ((state -> buttons & CONT_DPAD_UP) ? step : 0);
    int32_t x = (int32_t)z -> aim_x + dx, y = (int32_t)z -> aim_y + dy;
    x = (x < 0) ? 0 : ((x > (255 << 8)) ? (255 << 8) : x);
    y = (y < 0) ? 0 : ((y > (239 << 8)) ? (239 << 8) : y);
    z -> aim_x = (uint16_t)x;
    z -> aim_y = (uint16_t)y;
    int fire = (state -> buttons & (CONT_A | CONT_B)) != 0;
    zapper_slot_aim(z, x >> 8, (state -> buttons & CONT_B) ? -1 : (y >> 8), fire);
    if (z -> kind == ZAPPER_VS) {
        // (the Vs. Zapper is the pad on $4016: its serial bits in place of the pad's)
        cpu_registers -> PAD1_Latch = ZAPPER_VS_BASE | (fire ? ZAPPER_VS_TRIGGER : 0);
    }
}

/*
 * The Zappers' frame (vs_frame_input, after the pads'), each from its controller (input_ports.c's routing,
 * input_zapper_device). A NES Zapper's port has no pad (its bit 0 reads 0); a Zapper on, driven by player 1's pad's
 * controller, leaves that pad only Start and Select from it, so firing doesn't press the pad's buttons too (off, the
 * pad has all of it: input_pad_masked)
 */
static FRNES_BOARD_TEXT void zapper_player_input(void) {
    for (int i = 0; i < ZAPPER_SLOTS; i++) {
        zapper_state_t* z = ZAPPER_SLOT(i);
        if (!z -> kind) {
            continue;
        }
        uint8_t port = ZAPPER_NES_PORT(z);
        if (z -> kind == ZAPPER_PORT2) {
            cpu_registers -> PAD2_Latch = 0;
            if (input_pad_masked()) {
                cpu_registers -> PAD1_Latch &= NES_PAD_START_SELECT;
            }
        } else if (z -> kind == ZAPPER_PORT1) {
            cpu_registers -> PAD1_Latch = 0;
        }
        zapper_slot_input(z, (maple_device_t*)input_zapper_device(port));
    }
}

// a translucent quad in screen coordinates
static FRNES_BOARD_TEXT void zapper_quad(float x, float y, float w, float h, uint32_t argb) {
    pvr_vertex_t vertex;
    vertex.flags = PVR_CMD_VERTEX;
    vertex.z = 3.0f;
    vertex.u = vertex.v = 0.0f;
    vertex.oargb = 0;
    vertex.argb = argb;
    vertex.x = x;     vertex.y = y + h; pvr_prim(&vertex, sizeof(vertex));
    vertex.x = x;     vertex.y = y;     pvr_prim(&vertex, sizeof(vertex));
    vertex.x = x + w; vertex.y = y + h; pvr_prim(&vertex, sizeof(vertex));
    vertex.flags = PVR_CMD_VERTEX_EOL;
    vertex.x = x + w; vertex.y = y;     pvr_prim(&vertex, sizeof(vertex));
}

// the reticle: a crosshair at the aim, white over a dark outline, its middle left open (frame_hud_overlay)
static FRNES_BOARD_TEXT void zapper_reticle_draw(void) {
    // (no game: the overlay stays until the next game's zapper_setup, and a frame display between games draws it)
    if ((cpu_registers == NULL) || !(cpu_registers -> VS_Flags & VS_ZAPPER)) {
        return;
    }
    int two = ZAPPER_SLOT(1) -> kind != 0;
    pvr_poly_cxt_t context;
    pvr_poly_hdr_t header;
    pvr_poly_cxt_col(&context, PVR_LIST_TR_POLY);
    pvr_poly_compile(&header, &context);
    pvr_prim(&header, sizeof(header));
    for (int i = 0; i < ZAPPER_SLOTS; i++) {
        const zapper_state_t* z = ZAPPER_SLOT(i);
        if (!z -> kind || (PAD_INPUT -> zapper_off & (1 << ZAPPER_NES_PORT(z)))) {
            continue;
        }
        const video_picture_t* p = &video_picture;
        // the aim pixel's middle on the screen (the picture shows texture (u1, v1)-(u2, v2), 256 x 256 texels)
        float sx = p -> x1 + ((z -> aim_x / 256.0f + 0.5f) / 256.0f - p -> u1) * (p -> x2 - p -> x1) / (p -> u2 - p -> u1);
        float sy = p -> y1 + ((z -> aim_y / 256.0f + 0.5f) / 256.0f - p -> v1) * (p -> y2 - p -> y1) / (p -> v2 - p -> v1);
        // (a light gun's where it points, off the NES picture too; the screen round the picture grey, not black, so
        // it tracks there too and can shoot off the picture: Matt, 2026-11-05)
        const maple_device_t* device = (const maple_device_t*)input_zapper_device(ZAPPER_NES_PORT(z));
        if (device && (device -> info.functions & MAPLE_FUNC_LIGHTGUN)) {
            zapper_quad(0.0f, 0.0f, 640.0f, p -> y1, ZAPPER_GUN_GREY);
            zapper_quad(0.0f, p -> y2, 640.0f, 480.0f - p -> y2, ZAPPER_GUN_GREY);
            zapper_quad(0.0f, p -> y1, p -> x1, p -> y2 - p -> y1, ZAPPER_GUN_GREY);
            zapper_quad(p -> x2, p -> y1, 640.0f - p -> x2, p -> y2 - p -> y1, ZAPPER_GUN_GREY);
            // (Matt: hidden if the Controls page says so)
            if (zapper_gun_calibration.hide_reticle) {
                continue;
            }
            if (zapper_gun.latched) {
                sx = zapper_gun.sx;
                sy = zapper_gun.sy;
            }
        }
        const float arm = 10.0f, gap = 3.0f, thick = 2.0f;
        // (player 1's white, player 2's cyan when there are two)
        uint32_t light = (two && (ZAPPER_NES_PORT(z) == 1)) ? 0xe040e0ff : 0xe0ffffff;
        for (int pass = 0; pass < 2; pass++) {
            // (the outline a pixel wider all round, then the colour)
            float o = pass ? 0.0f : 1.0f;
            uint32_t colour = pass ? light : 0xa0000000;
            zapper_quad(sx - arm - o, sy - thick / 2 - o, arm - gap + 2 * o, thick + 2 * o, colour);
            zapper_quad(sx + gap - o, sy - thick / 2 - o, arm - gap + 2 * o, thick + 2 * o, colour);
            zapper_quad(sx - thick / 2 - o, sy - arm - o, thick + 2 * o, arm - gap + 2 * o, colour);
            zapper_quad(sx - thick / 2 - o, sy + gap - o, thick + 2 * o, arm - gap + 2 * o, colour);
        }
    }
}
#endif

// the light seen or not
static FRNES_BOARD_TEXT void zapper_light(const zapper_state_t* z, int light) {
    if (z -> kind == ZAPPER_VS) {
        cpu_registers -> PAD1_Latch = light ? (cpu_registers -> PAD1_Latch | ZAPPER_VS_LIGHT) :
            (cpu_registers -> PAD1_Latch & ~ZAPPER_VS_LIGHT);
    } else {
        uint8_t* extra = (z -> kind == ZAPPER_PORT1) ? &cpu_registers -> PAD1_Extra : &cpu_registers -> PAD2_Extra;
        *extra = light ? (*extra & ~ZAPPER_NO_LIGHT) : (*extra | ZAPPER_NO_LIGHT);
    }
}

static FRNES_BOARD_TEXT void zapper_slot_aim(zapper_state_t* z, int x, int y, int trigger) {
    // (turned off, zapper_set_enabled: idle whatever the aim and trigger, a movie's or the host's too)
    if (PAD_INPUT -> zapper_off & (1 << ZAPPER_NES_PORT(z))) {
        y = -1;
        trigger = 0;
    }
    z -> x = (x < 0) ? 0 : ((x > 255) ? 255 : x);
    z -> y = ((y < 0) || (y >= PPU_FRAMEBUFFER_HEIGHT)) ? 0xff : y;
    // (the Vs. Zapper's trigger is the frame's input, a movie's as well)
    if (trigger || ((z -> kind == ZAPPER_VS) && (cpu_registers -> PAD1_Latch & ZAPPER_VS_TRIGGER))) {
        // (from sensing nothing: the window's looked for from now, zapper_run)
        if (z -> sense == 0) {
            z -> phase = 0;
        }
        z -> sense = ZAPPER_SENSE_FRAMES;
    } else if (z -> sense && (--z -> sense == 0)) {
        zapper_light(z, 0);
    }
    // (the Vs. Zapper's trigger is in the frame's input: ZAPPER_VS_BASE | ZAPPER_VS_TRIGGER)
    if (z -> kind != ZAPPER_VS) {
        uint8_t* extra = (z -> kind == ZAPPER_PORT1) ? &cpu_registers -> PAD1_Extra : &cpu_registers -> PAD2_Extra;
        *extra = (*extra & ZAPPER_NO_LIGHT) | (trigger ? ZAPPER_TRIGGER : 0);
    }
}

// the Zapper on NES port 0 / 1's aim and trigger for the frame (none there: nothing)
FRNES_BOARD_TEXT void zapper_port_aim(uint8_t nes_port, int x, int y, int trigger) {
    if (!(cpu_registers -> VS_Flags & VS_ZAPPER)) {
        return;
    }
    vs_odd_timer_start();
    for (int i = 0; i < ZAPPER_SLOTS; i++) {
        zapper_state_t* z = ZAPPER_SLOT(i);
        if (z -> kind && (ZAPPER_NES_PORT(z) == nes_port)) {
            zapper_slot_aim(z, x, y, trigger);
        }
    }
}

// the only Zapper's (with two, the first's: NES port 0's)
FRNES_BOARD_TEXT void zapper_aim(int x, int y, int trigger) {
    if (!(cpu_registers -> VS_Flags & VS_ZAPPER)) {
        return;
    }
    vs_odd_timer_start();
    zapper_slot_aim(ZAPPER_SLOT(0), x, y, trigger);
}

// a frame in CPU cycles (262 lines of 341 dots, 3 a cycle; an odd frame's dot less doesn't matter here)
#define ZAPPER_FRAME_CYCLES ((262 * 341) / 3)

// the CPU cycle of the PPU's next position that changes the light: the aim point, line y + 21, next frame's line 0
static FRNES_BOARD_TEXT uint32_t zapper_next_cycle(const zapper_state_t* z) {
    if ((z -> y == 0xff) || (z -> phase == 2)) {
        // (next frame's start: ppu_dot_cpu_cycle takes a target less than half a frame behind as one just gone, so
        // after a window early in the frame, or each frame start with the aim off screen, line 0 would be due on
        // every batch, a cycle each, up to the middle of the frame: Matt's Duck Hunt at half speed while the aim was
        // in the top half)
        uint32_t cycle = ppu_dot_cpu_cycle(0, 0);
        return ((int32_t)(cycle - z -> clock) > 0) ? cycle : cycle + ZAPPER_FRAME_CYCLES;
    }
    return (z -> phase == 0) ? ppu_dot_cpu_cycle(z -> y, z -> x + ZAPPER_PIXEL_DOTS) : ppu_dot_cpu_cycle(z -> y + ZAPPER_LIGHT_LINES, 0);
}

// pixel x of the scanline being drawn, after ppu_sync() past it: the per dot pass's line buffer, or with rendering
// off the blank colour, which the PPU fills into the output (ppu.c's ppu_blank_color, through cpu_registers' page: a
// reference to ppu_registers from here would change the PPU's code)
static FRNES_BOARD_TEXT uint8_t zapper_line_pixel(uint32_t x) {
    const _PPU_registers_t* registers = VS_PPU_REGISTERS;
    if (registers -> PPUMASK & (PPUMASK_ENABLE_BACKGROUND_RENDERING | PPUMASK_ENABLE_SPRITE_RENDERING)) {
        return ppu_line_buffer_address()[x & 0xff];
    }
    uint32_t index = 0;
    if ((registers -> PPU_V & 0x3f00) == 0x3f00) {
        index = registers -> PPU_V & 0x1f;
        if ((index & 0x13) == 0x10) {
            index &= ~0x10;
        }
    }
    uint8_t colour = ((uint8_t*)PALETTE_RAM_ADDR)[index];
    return (registers -> PPUMASK & PPUMASK_GREYSCALE) ? (colour & 0x30) : colour;
}

static FRNES_BOARD_TEXT uint32_t zapper_slot_cycles_to_event(zapper_state_t* z) {
    if (z -> sense == 0) {
        return 0xffffffff;
    }
    z -> next = zapper_next_cycle(z);
    int32_t cycles = (int32_t)(z -> next - z -> clock);
    return (cycles > 0) ? (uint32_t)cycles : 1;
}

static FRNES_BOARD_TEXT void zapper_slot_run(zapper_state_t* z, uint32_t cycles) {
    z -> clock += cycles;
    if ((z -> sense == 0) || ((int32_t)(z -> clock - z -> next) < 0)) {
        return;
    }
    // (the batch can end with the PPU behind: bring it up to now, and the line being drawn up to its dot)
    cpu_sync_devices();
    ppu_sync();
    if (z -> y == 0xff) {
        z -> phase = 0;
        zapper_light(z, 0);
        return;
    }
    // where the PPU is against the window, in CPU cycles from now (ppu_dot_cpu_cycle: ppu_current_scanline /
    // ppu_current_dot, which LTO inlines everywhere else, would have to be kept as functions for a call from here)
    int32_t to_start = (int32_t)(ppu_dot_cpu_cycle(z -> y, z -> x + ZAPPER_PIXEL_DOTS) - z -> clock);
    int32_t to_end = (int32_t)(ppu_dot_cpu_cycle(z -> y + ZAPPER_LIGHT_LINES, 0) - z -> clock);
    if ((to_start <= 0) && (to_end > 0)) {
        if (z -> phase == 0) {
            // (on the aim line: the pixel is in the line being drawn; past it, a late catch-up, the output's)
            int on_line = (int32_t)(ppu_dot_cpu_cycle(z -> y + 1, 0) - z -> clock) > 0;
            uint8_t colour = on_line ? (zapper_line_pixel(z -> x) & 0x3f) :
                (ppu_framebuffer[z -> y * PPU_FRAMEBUFFER_WIDTH + z -> x] & 0x3f);
            if ((ZAPPER_BRIGHT_MASK >> colour) & 1) {
                zapper_light(z, 1);
            }
#ifdef FRNES_HOST
            if (getenv("ZAPPER_TRACE")) {
                printf("ZAPPER frame=%lu line=%lu dot=%lu aim=%u,%u colour=%02X %s\n", (unsigned long)ppu_frame_count,
                    (unsigned long)ppu_current_scanline(), (unsigned long)ppu_current_dot(), z -> x, z -> y, colour,
                    ((ZAPPER_BRIGHT_MASK >> colour) & 1) ? "LIGHT" : "dark");
            }
#endif
            z -> phase = 1;
        }
    } else {
        zapper_light(z, 0);
        // (before the aim point: a new frame's)
        z -> phase = (to_start > 0) ? 0 : 2;
    }
}

// the Zappers' next light change, in cycles (0xffffffff: none), and their catch-ups (a second Zapper's slot is idle,
// kind 0, sense 0, with one)
static FRNES_BOARD_TEXT uint32_t zapper_cycles_to_event(void) {
    uint32_t first = zapper_slot_cycles_to_event(ZAPPER_SLOT(0)), second = 0xffffffff;
    if (ZAPPER_SLOT(1) -> kind) {
        second = zapper_slot_cycles_to_event(ZAPPER_SLOT(1));
    }
    return (second < first) ? second : first;
}

static FRNES_BOARD_TEXT void zapper_run(uint32_t cycles) {
    zapper_slot_run(ZAPPER_SLOT(0), cycles);
    if (ZAPPER_SLOT(1) -> kind) {
        zapper_slot_run(ZAPPER_SLOT(1), cycles);
    }
}

// the bits the reads add: the DIP switches and coin 1 (kept from PAD1_Extra)
static FRNES_BOARD_TEXT void vs_apply(void) {
    uint8_t dips = cpu_registers -> VS_Dips;
    cpu_registers -> PAD1_Extra = (cpu_registers -> PAD1_Extra & VS_COIN1_BIT) | ((dips & 3) << 3);
    cpu_registers -> PAD2_Extra = dips & 0xfc;
}

// the game's set up (board_override, every game but Bandai's, whose register page keeps initialize_registers')
static FRNES_BOARD_TEXT void vs_setup(void) {
    if (!vs_game()) {
        cpu_registers -> VS_Flags = 0;
        cpu_registers -> PAD_OpenMask = 0xe0;
        cpu_registers -> PAD1_Extra = cpu_registers -> PAD2_Extra = 0;
        input_setup();
        zapper_setup();
        vaus_setup();
        return;
    }
    uint32_t info = vs_info(), hardware = (info >> 4) & 0x0f;
    printf(" ...Vs. System: PPU type %lu, hardware type %lu%s%s%s\n", (unsigned long)(info & 0x0f), (unsigned long)hardware,
        (((NesHeader.byInfo2 & 0x0c) != 0x08) && (info & VS_INFO_KNOWN)) ? " (game database)" : "",
        vs_swapped() ? ", player 1 on $4017" : "", (hardware == 6) ? " (its protection isn't emulated)" :
        ((hardware == 5) ? " (the Dual System's main side)" : ""));
    cpu_registers -> VS_Flags = VS_SYSTEM;
    // (the RC2C05s, PPU types 8-12: their $2002 ID and $2000 / $2001 swapped, as Mesen 2 has them)
    uint32_t ppu_type = vs_info() & 0x0f;
    if ((ppu_type >= 8) && (ppu_type <= 12)) {
        static const uint8_t rc2c05_ids[5] FRNES_COLD_RODATA = { 0x1b, 0x3d, 0x1c, 0x1b, 0x00 };
        VS_PPU_STATUS_ID = rc2c05_ids[ppu_type - 8];
        VS_PPU_STATUS_DRIVE = 0x1f;
        VS_PPU_SWAP = 1;
    }
    // (the Vs. System's boards have 4 kB of nametable RAM: four screens, as the game database has them, which an iNES
    // 1.0 dump's header leaves out. On the boards with no mirroring control, 99 and 206 (Hogan's Alley's town scrolls
    // across all four), and the VRC1's, 75 (Vs. Gradius, Vs. The Goonies: four screens in the database, the VRC1's
    // mirroring bit then ignored), so mapper_init maps them; the others' mirroring writes stay theirs)
    if ((cartridge_mapper_number == 99) || (cartridge_mapper_number == 206) || (cartridge_mapper_number == 75)) {
        NesHeader.byInfo1 |= 8;
    }
    cpu_registers -> PAD_OpenMask = 0;
    cpu_registers -> PAD1_Extra &= ~VS_COIN1_BIT;
    cpu_registers -> VS_Dips = vs_settings[VS_SETTING_DIPS];
    vs_apply();
    input_setup();
    zapper_setup();
}

// the DIP switches (bit n is switch n + 1, set = on): 0 (all off) until set
FRNES_BOARD_TEXT void vs_set_dip_switches(uint8_t dips) {
    vs_settings[VS_SETTING_DIPS] = dips;
    // (a Vs. game running: now)
    if ((cpu_registers != NULL) && (cpu_registers -> VS_Flags & VS_SYSTEM)) {
        cpu_registers -> VS_Dips = dips;
        vs_apply();
    }
}

// the Zapper's reticle speed, 1-10 (0: 5), speed x 0.8 pixels a frame at full stick or d-pad: taken at the game's start
FRNES_BOARD_TEXT void zapper_set_sensitivity(uint8_t speed) {
    vs_settings[VS_SETTING_ZAPPER_SPEED] = speed;
}

// port 2 for NES games (not Vs. ones): ZAPPER_PORT_AUTO (the game's header), ZAPPER_PORT_CONTROLLER (a pad even if the
// header says Zapper), ZAPPER_PORT_ZAPPER (the NES Zapper whatever the header says): taken at the game's start
FRNES_BOARD_TEXT void zapper_set_port_mode(uint8_t mode) {
    input_set_port_device(1, mode);
}

// after a state's load: the register page is the state's, the game's set up and the DIP switches (a setting) are
// this run's again (a state saved before the Vs. System had none, and PAD_OpenMask 0)
FRNES_BOARD_TEXT void vs_state_loaded(uint8_t dips) {
    (void)dips;
    // (the Zappers turned on or off, aimed and timed as the state had them: zapper_setup clears their slots)
    uint8_t zapper_off = PAD_INPUT -> zapper_off;
    zapper_state_t zappers[ZAPPER_SLOTS];
    for (uint32_t i = 0; i < ZAPPER_SLOTS; i++) {
        zappers[i] = *ZAPPER_SLOT(i);
    }
    vs_setup();
    PAD_INPUT -> zapper_off = zapper_off;
    for (uint32_t i = 0; i < ZAPPER_SLOTS; i++) {
        *ZAPPER_SLOT(i) = zappers[i];
    }
}

// PPU register writes on an RC2C05 (6502_writebyte.c, VS_PPU_SWAP set): $2000 and $2001 swapped, the write itself
// write_byte's with the swap off for it
FRNES_BOARD_TEXT void vs_ppu_write(uint16_t address, uint8_t data) {
    VS_PPU_SWAP = 0;
    write_byte(address ^ ((address & 6) ? 0 : 1), data);
    VS_PPU_SWAP = 1;
}

// $4016 writes on a Vs. System game (6502_writebyte.c): mapper 99's bank select
FRNES_BOARD_TEXT void vs_write_4016(uint8_t data) {
    vs_odd_timer_start();
    uint8_t changed = (cpu_registers -> VS_4016 ^ data) & 4;
    cpu_registers -> VS_4016 = data;
    if (changed && (cartridge_mapper_number == 99)) {
        ppu_sync();
        uint32_t bank = (data >> 2) & 1;
        chr_8k(bank);
        if (num_8k_ROM_pages > 4) {
            set_6502_bank_pointer(0, prg_page(bank * 4));
        }
    }
}

// the player's frame (main.c): coin 1 while its buttons are held (the pads' Select and Start left out then), and
// player 1's pad on $4017 for the games that read it there; the recording's event for the coin
// (frames a coin is in for: Vs. Duck Hunt takes 1-3 as a coin, 10 as a jammed one, no credit)
#define VS_COIN_PULSE_FRAMES 2
// The pads' Start and Select on a Vs. System game (input_poll_ports, as the controllers are read, before vs_frame_input
// and the recording: movies hold the NES's bits): its 1P start button is the pad's Select bit and 2P start its Start bit,
// so Start (the Dreamcast's) is 1P start and the Select buttons are 2P start (all of Matt's 52 Vs. dumps that start
// from a pad, 2026-11-02). Not on the Zapper games, whose trigger starts them
FRNES_BOARD_TEXT void vs_start_swap(void) {
    if ((cpu_registers -> VS_Flags & (VS_SYSTEM | VS_ZAPPER)) != VS_SYSTEM) {
        return;
    }
    uint8_t pad1 = cpu_registers -> PAD1_Latch, pad2 = cpu_registers -> PAD2_Latch;
    cpu_registers -> PAD1_Latch = (pad1 & ~(VS_PAD_SELECT | VS_PAD_START)) | ((pad1 & VS_PAD_SELECT) << 1) | ((pad1 & VS_PAD_START) >> 1);
    cpu_registers -> PAD2_Latch = (pad2 & ~(VS_PAD_SELECT | VS_PAD_START)) | ((pad2 & VS_PAD_SELECT) << 1) | ((pad2 & VS_PAD_START) >> 1);
}

FRNES_BOARD_TEXT uint16_t vs_frame_input(int coin) {
    vs_odd_timer_start();
#ifdef FRNES_PLAYER_BUILD
    if (cpu_registers -> VS_Flags & VS_ZAPPER) {
        zapper_player_input();
    }
    if (cpu_registers -> VS_Flags & VS_VAUS) {
        vaus_player_input();
    }
#endif
    if (!(cpu_registers -> VS_Flags & VS_SYSTEM)) {
        return 0;
    }
    if (coin) {
        cpu_registers -> PAD1_Latch &= ~(VS_PAD_SELECT | VS_PAD_START);
    }
    if (vs_swapped()) {
        // (the joysticks and their buttons: the start buttons are the cabinet's, on $4016 for player 1 whichever port
        // its joystick is on. Super Sky Kid and Vs. Tetris start only from $4016's Select / Start bits)
        uint8_t pad1 = cpu_registers -> PAD1_Latch, pad2 = cpu_registers -> PAD2_Latch, starts = VS_PAD_SELECT | VS_PAD_START;
        cpu_registers -> PAD1_Latch = (pad2 & ~starts) | (pad1 & starts);
        cpu_registers -> PAD2_Latch = (pad1 & ~starts) | (pad2 & starts);
    }
    // a press is one coin, VS_COIN_PULSE_FRAMES frames long however long the buttons are held; the next once they're
    // let go
    uint8_t flags = cpu_registers -> VS_Flags;
    uint32_t pulse = (flags & VS_COIN_PULSE_MASK) >> VS_COIN_PULSE_SHIFT;
    if (coin && !(flags & VS_COIN_HELD)) {
        pulse = VS_COIN_PULSE_FRAMES;
    }
    flags = coin ? (flags | VS_COIN_HELD) : (flags & ~VS_COIN_HELD);
    uint16_t event = pulse ? INPUT_RECORDING_EVENT_VS_COIN : 0;
    pulse = pulse ? (pulse - 1) : 0;
    cpu_registers -> VS_Flags = (flags & ~VS_COIN_PULSE_MASK) | (pulse << VS_COIN_PULSE_SHIFT);
    vs_recording_event(event);
    return event;
}

// a recording's frame (and the player's): coin 1 in while the frame has the event
FRNES_BOARD_TEXT void vs_recording_event(uint16_t event) {
    vs_odd_timer_start();
    if (!(cpu_registers -> VS_Flags & VS_SYSTEM)) {
        return;
    }
    cpu_registers -> PAD1_Extra = (cpu_registers -> PAD1_Extra & ~VS_COIN1_BIT) | ((event & INPUT_RECORDING_EVENT_VS_COIN) ? VS_COIN1_BIT : 0);
}

// the game's palette when it is a Vs. System one (video_init), else NULL
FRNES_BOARD_TEXT const uint16_t* vs_palette(void) {
    return vs_game() ? vs_palettes[vs_ppu_palette[vs_info() & 0x0f]] : NULL;
}

__attribute__((cold, noinline)) static void vs99_init() {
    chr_8k(0);
}

void board_cpu_run(uint32_t cycles) {
    if (cpu_registers -> VS_Flags) {
        // (the Vs. System's RGB PPU: no odd frame's skipped dot, see vs_rgb_timer_board; the Zapper's light)
        if (cpu_registers -> VS_Flags & VS_SYSTEM) {
            VS_PPU_REGISTERS -> ODDFRAME = 0;
        }
        if (cpu_registers -> VS_Flags & VS_ZAPPER) {
            zapper_run(cycles);
        }
        if (cpu_registers -> VS_Flags & VS_VAUS) {
            vaus_run();
        }
    } else if (board_number == 20) {
        fds_cpu_run(cycles);
    } else if (board_number == 65) {
        h3001_cpu_run(cycles);
    } else if (board_number == 18) {
        ss88006_cpu_run(cycles);
    } else if (board_number == 105) {
        nwc_cpu_run(cycles);
    } else if (board_number == BOARD_EVENT2) {
        event2_cpu_run(cycles);
    } else if ((board_number == 6) || (board_number == 8) || (board_number == 17)) {
        ffe_cpu_run(cycles);
    } else if ((board_number == 42) || (board_number == 40) || (board_number == 43) || (board_number == 304) || (board_number == 50) || (board_number == 368)) {
        m42_cpu_run(cycles);
    } else if (board_number == 535) {
        m535_cpu_run(cycles);
    } else if (board_number == BOARD_SB5013) {
        sb5013_cpu_run(cycles);
    } else if (board_number == 117) {
        m117_cpu_run(cycles);
    } else if (board_number == 106) {
        m106_cpu_run(cycles);
    } else if (board_number == 35) {
        m35_cpu_run(cycles);
    } else if ((board_number == 253) || (board_number == 529) || (board_number == 362)) {
        m253_cpu_run(cycles);
    } else if (board_number == 183) {
        m183_cpu_run(cycles);
    } else if (board_number == 222) {
        m222_cpu_run(cycles);
    } else if ((board_number == 142) || (board_number == 56)) {
        ks202_cpu_run(cycles);
    } else if ((board_number == 90) || (board_number == 209) || (board_number == 211)) {
        jy_cpu_run(cycles);
    } else if (smc_board()) {
        smc_cpu_run(cycles);
    } else if ((board_number == 163) || (board_number == 162)) {
        nanjing_cpu_run(cycles);
    } else if ((board_number == 83) || (board_number == BOARD_YOKO)) {
        cony_cpu_run(cycles);
    }
}

uint32_t board_cycles_to_fire() {
    if (!mapper_cpu_timer) {
        return 0xffffffff;
    }
    if (cpu_registers -> VS_Flags) {
        // (a catch-up at least every VS_ODD_CLEAR_CYCLES, so one comes between a frame's end and the next pre-render
        // line, a frame later, even with the NMI off: vs_rgb_timer_board; the Zapper's next light change)
        uint32_t cycles = (cpu_registers -> VS_Flags & VS_SYSTEM) ? VS_ODD_CLEAR_CYCLES : 0xffffffff;
        if (cpu_registers -> VS_Flags & VS_ZAPPER) {
            uint32_t zapper = zapper_cycles_to_event();
            cycles = (zapper < cycles) ? zapper : cycles;
        }
        // (the NES Vaus paddle's bits being read: every instruction)
        if ((cpu_registers -> VS_Flags & VS_VAUS) && vaus.window) {
            cycles = 1;
        }
        return cycles;
    }
    if (board_number == 20) {
        // (the next timer step or disk byte: the batch ends there; the FDS raises its IRQs itself)
        return fds_cycles_to_event();
    }
    if (board_number == 65) {
        return h3001_counter ? h3001_counter : 0x10000;
    }
    if (board_number == 105) {
        return NWC_TIMER_TARGET - NWC -> counter;
    }
    if (board_number == BOARD_EVENT2) {
        return EVENT2_TIMER_TARGET - EVENT2 -> count;
    }
    if ((board_number == 6) || (board_number == 8) || (board_number == 17)) {
        return 0x10000 - FFE -> counter;
    }
    if ((board_number == 42) || (board_number == 40) || (board_number == 43) || (board_number == 304) || (board_number == 50) || (board_number == 368)) {
        return m42_cycles_to_fire();
    }
    if (board_number == 535) {
        return m535_cycles_to_fire();
    }
    if (board_number == BOARD_SB5013) {
        return sb5013_cycles_to_fire();
    }
    if (board_number == 117) {
        return m117_cycles_to_fire();
    }
    if (board_number == 106) {
        return m106_cycles_to_fire();
    }
    if (board_number == 35) {
        return m35_cycles_to_fire();
    }
    if ((board_number == 253) || (board_number == 529) || (board_number == 362)) {
        return m253_cycles_to_fire();
    }
    if (board_number == 183) {
        return m183_cycles_to_fire();
    }
    if (board_number == 222) {
        return m222_cycles_to_fire();
    }
    if ((board_number == 142) || (board_number == 56)) {
        return ks202_cycles_to_fire();
    }
    if (smc_board()) {
        return smc_cycles_to_fire();
    }
    if ((board_number == 163) || (board_number == 162)) {
        return nanjing_cycles_to_fire();
    }
    if ((board_number == 83) || (board_number == BOARD_YOKO)) {
        return cony_cycles_to_fire();
    }
    if ((board_number == 90) || (board_number == 209) || (board_number == 211)) {
        return jy_cycles_to_fire();
    }
    if (board_number == 18) {
        return ss88006_cycles_to_zero();
    }
    return 0xffffffff;
}

// $8000-$FFFF writes, mapper_write_select's default handler
__attribute__((noinline)) void board_write(uint16_t address, uint8_t data) {
    switch (board_number) {
        case BOARD_NSF:
            nsf_write(address, data);
            break;
        case 33: case 48:
            taito_tc0190_write(address, data);
            break;
        case 32:
            g101_write(address, data);
            break;
        case 65:
            h3001_write(address, data);
            break;
        case 18:
            ss88006_write(address, data);
            break;
        case 89: case 93:
            sunsoft_simple_write(address, data);
            break;
        case 210:
            namco210_write(address, data);
            break;
        case 77:
            irem77_write(address, data);
            break;
        case 13:
            cprom_write(address, data);
            break;
        case 180:
            unrom180_write(data);
            break;
        case 78:
            jf16_write(address, data);
            break;
        case 70: case 152:
            bandai70_write(address, data);
            break;
        case 72: case 92:
            jf17_write(address, data);
            break;
        case 94:
            un1rom94_write(address, data);
            break;
        case 188:
            karaoke188_write(address, data);
            break;
        case 105:
            nwc_write(address, data);
            break;
        case 6: case 8:
            ffe_write(address, data);
            break;
        case 42:
            m42_write(address, data);
            break;
        case 117:
            m117_write(address, data);
            break;
        case 106:
            m106_write(address, data);
            break;
        case 35:
            m35_write(address, data);
            break;
        case 253:
            m253_write(address, data);
            break;
        case 529:
            m529_write(address, data);
            break;
        case 362:
            m362_write(address, data);
            break;
        case 183:
            m183_write(address, data);
            break;
        case BOARD_SB5013:
            sb5013_write(address, data);
            break;
        case 40:
            m40_write(address, data);
            break;
        case 43:
            m43_write(address, data);
            break;
        case 222:
            m222_write(address, data);
            break;
        case 142: case 56:
            ks202_write(address, data);
            break;
        case 235:
            bmc235_write(address, data);
            break;
        case BOARD_830752:
            m396_write(address, data);
            break;
        case BOARD_PB011003:
            M314[3] = data;
            m314_apply();
            break;
        case BOARD_BS5:
            bs5_write(address);
            break;
        case BOARD_MMC1_RESET:
            m374_write(address, data);
            break;
        case BOARD_S009:
            M434[1] = data;
            m434_apply();
            break;
        case BOARD_89433:
            if (M403[2] & 4) {
                M403[1] = data;
                m403_apply();
            }
            break;
        case BOARD_GS2004:
            gs2004_write(data);
            break;
        case 29:
            m29_write(data);
            break;
        case 202:
            latch202_write(address);
            break;
        case BOARD_NTD03:
            ntd03_write(address);
            break;
        case 62:
            latch62_write(address, data);
            break;
        case 61:
            latch61_write(address);
            break;
        case 46:
            M46[1] = data;
            m46_apply();
            break;
        case 125:
            if ((address & 0xe000) == 0xc000) {
                NES_WINDOW_BYTE(address) = data;
            }
            break;
        case 519:
            m519_write(address, data);
            break;
        case 438:
            m438_write(address, data);
            break;
        case 449:
            M449[0] = address & 0xff;
            M449[1] = (address >> 8) & 3;
            M449[2] = data;
            m449_apply();
            break;
        case 167:
            M167[(address >> 13) & 3] = data;
            if ((address & 0xe000) == 0x8000) {
                ppu_sync();
                set_ppu_nametable_mirroring((data & 1) ? NAMETABLE_MIRRORING_VERTICAL : NAMETABLE_MIRRORING_HORIZONTAL);
            }
            m167_apply();
            break;
        case 302:
            m302_write(address, data);
            break;
        case 307:
            m307_write(address, data);
            break;
        case 305:
            M305[(address >> 11) & 3] = data;
            m305_bank((address >> 11) & 3);
            break;
        case 535:
            if ((address >= 0xb800) && (address < 0xd800)) {
                NES_WINDOW_BYTE(address) = data;
            } else if ((address & 0xf000) == 0xe000) {
                m535_irq_write(data);
            } else if ((address & 0xf000) == 0xf000) {
                M535 -> prg = data;
                set_wram_bank_pointer(prg_page(data));
            }
            break;
        case 103:
            switch (address & 0xf000) {
                case 0x8000:
                    M103[0] = data & 0x0f;
                    m103_apply();
                    break;
                case 0xb000: case 0xc000: case 0xd000:
                    if ((address >= 0xb800) && (address < 0xd800)) {
                        if (!M103[1]) {
                            NES_WINDOW_BYTE(address) = data;
                        } else {
                            m103_ram_write(WRAM + 0x2000 + (address - 0xa000), data);
                        }
                    }
                    break;
                case 0xe000:
                    ppu_sync();
                    set_ppu_nametable_mirroring((data & 8) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
                    break;
                case 0xf000:
                    M103[1] = (data & 0x10) != 0;
                    m103_apply();
                    break;
            }
            break;
        case 522:
            if ((address & 0xe000) == 0xc000) {
                NES_WINDOW_BYTE(address) = data;
            } else if ((address & 0xe001) == 0x8000) {
                M522_REGISTER = data & 7;
            } else if (((address & 0xe001) == 0x8001) && (M522_REGISTER >= 6)) {
                set_6502_bank_pointer(M522_REGISTER - 6, prg_page(data));
            }
            break;
        case 415:
            set_wram_bank_pointer(prg_page(data & 0x0f));
            ppu_sync();
            set_ppu_nametable_mirroring((data & 0x10) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
            break;
        case 309:
            if ((address & 0xe000) == 0x8000) {
                set_6502_bank_pointer(0, prg_page(data & 0x0f));
            } else if ((address & 0xe000) == 0xe000) {
                ppu_sync();
                set_ppu_nametable_mirroring((data & 8) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
            }
            break;
        case 530:
            ax5705_write(address, data);
            break;
        case BOARD_KS7012:
            if (address == 0xe0a0) {
                prg_32k(0);
            } else if (address == 0xee36) {
                prg_32k(1);
            }
            break;
        case BOARD_KS7013B:
            ppu_sync();
            set_ppu_nametable_mirroring((data & 1) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
            break;
        case BOARD_KS7016:
            ks7016_write(address);
            break;
        case 190:
            if ((address & 0xe000) == 0x8000) {
                prg_16k(0, data & 7);
            } else if ((address & 0xe000) == 0xc000) {
                prg_16k(0, (data & 7) | 8);
            } else if ((address & 0xa000) == 0xa000) {
                ppu_sync();
                chr_2k((address & 3) * 2, data * 2);
            }
            break;
        case 149:
            if (!chr_8k_mapped(data >> 7)) {
                ppu_sync();
                chr_8k(data >> 7);
            }
            break;
        case 51:
            M51[0] = data & 0x0f;
            if ((address & 0xe000) == 0xc000) {
                M51[1] = (M51[1] & 1) | ((data >> 3) & 2);
            }
            m51_apply();
            break;
        case BOARD_810544:
            latch261_write(address);
            break;
        case 81:
            prg_16k(0, (address >> 2) & 3);
            if (!chr_8k_mapped(address & 3)) {
                ppu_sync();
                chr_8k(address & 3);
            }
            break;
        case 171:
            if ((address & 0xf000) == 0xf000) {
                ppu_sync();
                chr_4k((address >> 7) & 1, data);
            }
            break;
        case 219:
            a9746_write(address, data);
            break;
        case BOARD_A65AS:
            a65as_write(data);
            break;
        case BOARD_K3046:
            prg_16k(0, data & 0x3f);
            prg_16k(2, (data & 0x38) | 7);
            break;
        case 203:
            prg_16k(0, data >> 2);
            prg_16k(2, data >> 2);
            if (!chr_8k_mapped(data & 3)) {
                ppu_sync();
                chr_8k(data & 3);
            }
            break;
        case 216:
            latch216_write(address);
            break;
        case 230:
            m230_write(data);
            break;
        case BOARD_YS2309:
            if (M439[2] & 0x80) {
                data = (data & 0x7f) | (M439[0] & 0x80);
            }
            if ((data ^ M439[0]) & 0x80) {
                ppu_sync();
                set_ppu_nametable_mirroring((data & 0x80) ? NAMETABLE_MIRRORING_HORIZONTAL : NAMETABLE_MIRRORING_VERTICAL);
            }
            M439[0] = data;
            m439_apply();
            break;
        case 176:
            fk23_write(address, data);
            break;
        case 112:
            ntdec112_write(address, data);
            break;
        case 132: case 36: case 173: case 136: case 172:
            txc132_write(address, data);
            break;
        case 156:
            daou156_write(address, data);
            break;
        case 83: case BOARD_YOKO:
            cony_write(address, data);
            break;
        case 58: case 59:
            latch58_59_write(address, data);
            break;
        case 57: case 227: case BOARD_HP898F: case 242: case 148: case 212: case 177: case 15: case 236: case BOARD_T262: case 226: case 221: case 63:
        case 200: case 201: case 225: case 108:
            latch_more_write(address, data);
            break;
        case 90: case 209: case 211:
            jy_write(address, data);
            break;
        case 241:
            prg_32k(data);
            break;
        case 97:
            tams1_97_write(address, data);
            break;
        case 28:
            a53_write(data);
            break;
        case 228:
            action52_write(address, data);
            break;
        case 185:
            cnrom185_write(data);
            break;
        case 144:
            agci144_write(address, data);
            break;
        case 20:
            fds_write(address, data);
            break;
        case 47: case 37: case 45: case 52: case BOARD_EVENT2:
            mmc3_board_write(address, data);
            break;
        case 12:
            if (smc_board()) {
                smc_write(address, data);
            } else {
                mmc3x_write(address, data);
            }
            break;
        case 134: case 224: case BOARD_COOLBOY: case BOARD_RESET_TXROM: case 195: case 215: case 189: case 196: case 249: case BOARD_ING022: case 115: case 248:
        case 74: case 191: case 192: case 194: case 114: case 197: case 205: case 395: case 267: case 377: case 198: case 445: case 516: case BOARD_YY841101: case BOARD_830118C: case 44: case 250: case 126:
        case 245: case 123: case 187: case BOARD_411120C: case BOARD_GN45: case BOARD_KOF97: case 254:
            mmc3x_write(address, data);
            break;
        case BOARD_HPXX:
            if (MMC3X -> outer[0] & 4) {
                ppu_sync();
                MMC3X -> outer[3] = data;
                mmc3x_apply();
            } else {
                mmc3x_write(address, data);
            }
            break;
        case 91:
            jy91_outer_write(address, data);
            break;
        case 121:
            m121_write(address, data);
            break;
        case BOARD_SGD: case BOARD_TGD:
            smc_write(address, data);
            break;
    }
}

void board_write_low(uint16_t address, uint8_t data) {
    if (board_number == 20) {
        fds_write_low(address, data);
    } else if (board_number == BOARD_NSF) {
        nsf_write_low(address, data);
    } else if ((board_number == 28) && (address >= 0x5000)) {
        a53_select = ((data & 0x80) >> 6) | (data & 1);
    } else if (board_number == 228) {
        action52_ram[address & 3] = data & 0x0f;
    } else if ((board_number == 79) || (board_number == 113)) {
        nina0306_write_low(address, data);
    } else if ((board_number == 132) || (board_number == 36) || (board_number == 173) || txc132_jv001()) {
        txc132_write(address, data);
    } else if (board_number == BOARD_EVENT2) {
        event2_write_low(address, data);
    } else if ((board_number == 6) || (board_number == 8) || (board_number == 17)) {
        ffe_write_low(address, data);
    } else if (board_number == 176) {
        fk23_write_low(address, data);
    } else if ((board_number == 178) || (board_number == BOARD_JNCOTA)) {
        waixing178_write_low(address, data);
    } else if (board_number == BOARD_89433) {
        m403_write_low(address, data);
    } else if (board_number == 43) {
        m43_write(address, data);
    } else if (board_number == 304) {
        m304_write(address, data);
    } else if (board_number == 50) {
        m50_write(address, data);
    } else if (board_number == 368) {
        m368_write(address, data);
    } else if (board_number == 521) {
        if (address == 0x5020) {
            prg_16k(0, data & 7);
        }
    } else if (board_number == 120) {
        if (address == 0x41ff) {
            set_wram_bank_pointer(prg_page(data));
        }
    } else if (board_number == 216) {
        if (address == 0x5000) {
            latch216_write(address);
        }
    } else if (board_number == 133) {
        if ((address & 0xe100) == 0x4100) {
            prg_32k((data >> 2) & 1);
            if (!chr_8k_mapped(data & 3)) {
                ppu_sync();
                chr_8k(data & 3);
            }
        }
    } else if (board_number == BOARD_PB011003) {
        if ((address & 0xf000) == 0x5000) {
            M314[address & 3] = data;
            m314_apply();
        }
    } else if ((board_number == 164) || (board_number == BOARD_YC0309)) {
        waixing164_write_low(address, data);
    } else if (board_number == 240) {
        board240_write_low(address, data);
    } else if ((board_number == 225) && (address >= 0x5800)) {
        ((uint8_t*)cpu_registers)[0x219 + (address & 3)] = data & 0x0f;
    } else if (board_number == 121) {
        m121_write_low(address, data);
    } else if ((board_number == 83) || (board_number == BOARD_YOKO)) {
        cony_write_low(address, data);
    } else if ((board_number == 90) || (board_number == 209) || (board_number == 211)) {
        jy_write_low(address, data);
    } else if (smc_board()) {
        smc_write_low(address, data);
    } else if ((board_number == 163) || (board_number == 162)) {
        nanjing_write_low(address, data);
    } else if ((board_number == 150) || ((board_number >= 137) && (board_number <= 139)) || (board_number == 141)) {
        sachen374_write(address, data);
    } else if (board_number == 198) {
        if (address >= 0x5000) {
            WRAM[0x2000 + (address & 0x0fff)] = data;
        }
    } else if ((board_number == 12) || (board_number == 224) || (board_number == BOARD_COOLBOY) || (board_number == 215) ||
        (board_number == BOARD_HPXX) || (board_number == 189) || (board_number == 249) || (board_number == 123) ||
        (board_number == 187) || (board_number == 445)) {
        mmc3x_outer_write(address, data);
    }
}

uint8_t* board_battery_data(uint32_t* size) {
    return (board_number == 20) ? fds_battery_data(size) : NULL;
}

void board_battery_prepare() {
    if (board_number == 20) {
        fds_battery_prepare();
    }
}

void board_battery_reset() {
    if (board_number == 20) {
        fds_battery_reset();
    }
}

void board_battery_loaded() {
    if (board_number == 20) {
        fds_battery_loaded();
    }
}

uint8_t board_read_low(uint16_t address, uint8_t open_bus) {
    if (cpu_registers -> VS_Flags & VS_SYSTEM) {
        return vs_read_low(address);
    }
    if (board_number == 20) {
        return fds_read_low(address, open_bus);
    }
    if (board_number == BOARD_NSF) {
        return nsf_read_low(address, open_bus);
    }
    if (board_number == 228) {
        return (open_bus & 0xf0) | action52_ram[address & 3];
    }
    if ((board_number == 368) && ((address & 0xf1ff) == 0x4122)) {
        return 0x8a | M43_SWAP;
    }
    if ((board_number == 198) && (address >= 0x5000)) {
        return WRAM[0x2000 + (address & 0x0fff)];
    }
    if (board_number == BOARD_EVENT2) {
        return event2_read_low(address, open_bus);
    }
    if (board_number == 176) {
        return fk23_read_low(address, open_bus);
    }
    if ((board_number == BOARD_SGD) || (board_number == BOARD_TGD)) {
        return gd_read_low(address, open_bus);
    }
    if (board_number == 163) {
        return nanjing_read_low(address, open_bus);
    }
    if (board_number == 150) {
        return sachen374_read(address, open_bus);
    }
    if (((board_number >= 137) && (board_number <= 139)) || (board_number == 141)) {
        return open_bus;
    }
    if ((board_number == 90) || (board_number == 209) || (board_number == 211)) {
        return jy_read_low(address, open_bus);
    }
    if ((board_number == 83) || (board_number == BOARD_YOKO)) {
        return cony_read_low(address, open_bus);
    }
    if ((board_number == 225) && (address >= 0x5800)) {
        return (open_bus & 0xf0) | ((uint8_t*)cpu_registers)[0x219 + (address & 3)];
    }
    if ((board_number == 132) || (board_number == 36) || (board_number == 173) || txc132_jv001()) {
        return txc132_read(address, open_bus);
    }
    if ((board_number == 121) && ((address & 0xf000) == 0x5000)) {
        return M121[4];
    }
    if ((board_number == 216) && (address == 0x5000)) {
        return 0;
    }
    if ((board_number == 187) && (address >= 0x5000)) {
        return 0x83;
    }
    if ((board_number == 143) && (address >= 0x4100)) {
        return (~address & 0x3f) | 0x40;
    }
    if ((board_number == 43) && (address >= 0x5000)) {
        return prg_page(8)[address & 0x0fff];
    }
    if ((board_number == 304) && (address >= 0x5000)) {
        // (the 4 kB before the last 8 kB)
        return prg_page(M304_LAST - 1)[0x1000 | (address & 0x0fff)];
    }
    if ((board_number == BOARD_HPXX) && ((address & 0xf000) == 0x5000)) {
        // (the DIP switch, bits 0-1: 0)
        return open_bus & 0xfc;
    }
    return open_bus;
}

// $6000-$7FFF writes and reads while board_6000 is set (mapper_write_6000, mapper_read_6000)
void board_write_6000(uint16_t address, uint8_t data) {
    switch (board_number) {
        case 91:
            jy91_write(address, data);
            break;
        case 80: case 207: case 82:
            x1_write_6000(address, data);
            break;
        case 184:
            sunsoft184_write(data);
            break;
        case 183:
            m183_write(address, data);
            break;
        case BOARD_YS2309:
            M439[1 + (address & 1)] = data;
            m439_apply();
            break;
        case BOARD_KS7013B:
            prg_16k(0, data);
            break;
        case 51:
            M51[1] = ((data >> 3) & 2) | ((data >> 1) & 1);
            m51_apply();
            break;
        case 46:
            M46[0] = data;
            m46_apply();
            break;
        case 125:
            set_wram_bank_pointer(prg_page(data & 0x0f));
            break;
        case 305: case 302:
            break;
        case 307:
            if (address < 0x7000) {
                NES_WINDOW_BYTE(address) = data;
            }
            break;
        case 103:
            // (the RAM under the ROM)
            m103_ram_write(WRAM + (address & 0x1fff), data);
            break;
        case 246:
            if (address >= 0x6800) {
                if (WRAM != NULL) {
                    NES_WINDOW_BYTE(address) = data;
                }
            } else if (address & 4) {
                ppu_sync();
                chr_2k((address & 3) * 2, data * 2);
            } else {
                set_6502_bank_pointer(address & 3, prg_page(data));
            }
            break;

        case BOARD_S009:
            M434[0] = data;
            ppu_sync();
            set_ppu_nametable_mirroring((data & 0x20) ? NAMETABLE_MIRRORING_VERTICAL : NAMETABLE_MIRRORING_HORIZONTAL);
            m434_apply();
            break;
        case 193:
            tc112_write(address, data);
            break;
        case 87:
            jf87_write(data);
            break;
        case 47: case 37:
            if (mmc3_board_ram_writable()) {
                mmc3_outer_select(data);
            }
            break;
        case 52:
            if (mmc3_board_ram_writable()) {
                if (!(MMC3_52_REGISTER & 0x80)) {
                    MMC3_52_REGISTER = data;
                    mmc3_outer_select(0);
                } else if (WRAM != NULL) {
                    NES_WINDOW_BYTE(address) = data;
                }
            }
            break;
        case 45:
            if (!(MMC3_45_REGISTERS[3] & 0x40)) {
                MMC3_45_REGISTERS[MMC3_45_INDEX] = data;
                MMC3_45_INDEX = (MMC3_45_INDEX + 1) & 3;
                mmc3_outer_select(0);
            } else if (WRAM != NULL) {
                NES_WINDOW_BYTE(address) = data;
            }
            break;
        case 86:
            jf13_write(address, data);
            break;
        case 134: case BOARD_COOLBOY: case 189: case 196: case BOARD_ING022: case 115: case 248: case 114: case 205: case 395: case 267: case 377:
        case BOARD_YY841101: case BOARD_830118C: case 126: case 187: case BOARD_411120C: case BOARD_GN45:
            mmc3x_outer_write(address, data);
            break;
        case BOARD_HP898F: case BOARD_SUPER40:
            latch_more_write(address, data);
            break;
        case 150: case 137: case 138: case 139: case 141:
            sachen374_write(address, data);
            break;
        case 140:
            jf11_write(data);
            break;
        case 210:
            if ((board_submapper == 1) && namco175_write_enable) {
                NES_WINDOW_BYTE(address) = data;
            }
            break;
    }
}

uint8_t board_read_6000(uint16_t address, uint8_t open_bus) {
    if (board_number == 254) {
        return (WRAM != NULL) ? (uint8_t)(NES_WINDOW_BYTE(address) ^ (MMC3X -> outer[0] ? 0 : MMC3X -> outer[1])) : open_bus;
    }
    if ((board_number == 80) || (board_number == 207) || (board_number == 82)) {
        return x1_ram_enabled(address) ? NES_WINDOW_BYTE(address) : open_bus;
    }
    if (board_number == 188) {
        return karaoke188_read_6000(open_bus);
    }
    if (board_number == 212) {
        return ((address & 0xe010) == 0x6000) ? (open_bus | 0x80) : open_bus;
    }
    // (Namco 340: no PRG RAM)
    return open_bus;
}



// a NES 2.0 ROM size in the exponent-multiplier form (a size nibble of 0xF): 2^E * (M * 2 + 1) bytes, E the LSB byte's
// bits 2-7 and M bits 0-1
static uint32_t nes2_exponent_size(uint8_t lsb) {
    uint32_t exponent = lsb >> 2;
    return (exponent < 30) ? ((1u << exponent) * ((lsb & 3) * 2 + 1)) : 0;
}

// a NES 2.0 header with its PRG or CHR ROM size in the exponent-multiplier form, as load_nes_file doesn't read it (Vs.
// Gumshoe's 40 kB of PRG ROM, Vs. Tetris's 24 kB, Raid on Bungeling Bay's 40 kB): the file again with the sizes in whole
// units (16 kB PRG, 8 kB CHR), each ROM padded with $FF up to them. Mapper 99's $8000 bank (vs_write_4016) and every board's banks stay where they were; the ROM's CRC32
// (load_nes_file's, after the conversion) is the padded file's
static uint8_t* nes2_exponent_cartridge(uint8_t* data, uint32_t* size) {
    if ((*size < 16) || memcmp(data, "NES\x1a", 4) || ((data[7] & 0x0c) != 0x08) ||
        (((data[9] & 0x0f) != 0x0f) && ((data[9] & 0xf0) != 0xf0))) {
        return data;
    }
    uint32_t trainer = (data[6] & 4) ? 512 : 0;
    uint32_t prg = ((data[9] & 0x0f) == 0x0f) ? nes2_exponent_size(data[4]) : ((((data[9] & 0x0f) << 8) | data[4]) * 0x4000);
    uint32_t chr = ((data[9] & 0xf0) == 0xf0) ? nes2_exponent_size(data[5]) : ((((data[9] & 0xf0) << 4) | data[5]) * 0x2000);
    uint32_t prg_units = (prg + 0x3fff) / 0x4000, chr_units = (chr + 0x1fff) / 0x2000;
    if ((prg_units > 0xfff) || (chr_units > 0xfff) || ((uint64_t)16 + trainer + prg + chr > *size)) {
        return data;
    }
    uint32_t rest = *size - 16 - trainer - prg - chr;
    uint32_t new_size = 16 + trainer + prg_units * 0x4000 + chr_units * 0x2000 + rest;
    uint8_t* out = malloc(new_size);
    if (out == NULL) {
        return data;
    }
    printf(" ...NES 2.0 exponent-multiplier sizes: PRG ROM %lu bytes, CHR ROM %lu bytes\n", (unsigned long)prg,
        (unsigned long)chr);
    memset(out, 0xff, new_size);
    memcpy(out, data, 16 + trainer);
    out[4] = prg_units & 0xff;
    out[5] = chr_units & 0xff;
    out[9] = ((chr_units >> 8) << 4) | (prg_units >> 8);
    uint8_t* to = out + 16 + trainer;
    const uint8_t* from = data + 16 + trainer;
    // (under 32 kB, its end at $FFFF, where the vectors are: Vs. Tetris's 24 kB at $A000-$FFFF; above, its start at
    // $8000, mapper 99's $8000 bank of 0 or 4 among Vs. Gumshoe's 5)
    memcpy(to + ((prg < 0x8000) ? (prg_units * 0x4000 - prg) : 0), from, prg);
    memcpy(to + prg_units * 0x4000, from + prg, chr);
    memcpy(to + prg_units * 0x4000 + chr_units * 0x2000, from + prg + chr, rest);
    free(data);
    *size = new_size;
    return out;
}

// Bandai's Datach Joint ROM System (mapper 157: Dragon Ball Z Gekitou Tenkaichi Budokai, SD Gundam Wars, Ultraman Club,
// Yu Yu Hakusho...) as an LZ93D50 board (mapper 16 submapper 5, mapper.c's bandai_board): its registers at $8000-$FFFF,
// the Datach's 24C02, its 8 kB of CHR RAM unbanked (bandai_write banks only CHR ROM). Not emulated: the barcode reader
// ($6000-$7FFF bit 3 reads 0: no card), and Battle Rush's own 24C01. A NES 2.0 header's mapper 157 becomes 16 / 5 here,
// an iNES one's through the game database (board_db_mapper)
static void nes2_datach_cartridge(uint8_t* data, uint32_t size) {
    if ((size >= 16) && !memcmp(data, "NES\x1a", 4) && ((data[7] & 0x0c) == 0x08) &&
        ((((data[8] & 0x0f) << 8) | (data[7] & 0xf0) | (data[6] >> 4)) == 157)) {
        printf(" ...Datach (mapper 157): an LZ93D50 board with its 24C02 (no barcode reader)\n");
        data[6] &= 0x0f;
        data[7] = (data[7] & 0x0f) | 0x10;
        data[8] = 0x50;
    }
}

/*
 * A game's battery RAM as no save left it (input_ports_start: main.c's game start, after its battery load, before the
 * CPU's first instruction; a movie from power on comes here too): $FF in place of zeros for the games that take a
 * zeroed RAM for a save. Famicom Jump II (Bandai 153) reads all zeros as a valid save and stops in its BRK handler's
 * trap; real SRAM powers up with something in it (Mesen 2 fills it at random). Only when the RAM is all zeros, so a
 * save that was loaded is left as it is. Through the CPU's $6000-$7FFF window, as its reads see it
 */
static const uint32_t fresh_wram_ff_crcs[] FRNES_COLD_RODATA = {
    0x3f15d20d,    // Famicom Jump II: Saikyou no 7-nin
};

FRNES_COLD_TEXT void board_fresh_wram(void) {
#ifdef FRNES_RESET_PPU
    frnes_power_on_capture();
#endif
    if (smc_board()) {
        smc_game_start();
    }
    if (WRAM == NULL) {
        return;
    }
    uint32_t i;
    for (i = 0; i < sizeof(fresh_wram_ff_crcs) / sizeof(fresh_wram_ff_crcs[0]); i++) {
        if (fresh_wram_ff_crcs[i] == currentCRC32) {
            break;
        }
    }
    if (i == sizeof(fresh_wram_ff_crcs) / sizeof(fresh_wram_ff_crcs[0])) {
        return;
    }
    for (i = 0; i < 0x2000; i++) {
        if (NES_WINDOW_BYTE(0x6000 + i)) {
            return;
        }
    }
    for (i = 0; i < 0x2000; i++) {
        NES_WINDOW_BYTE(0x6000 + i) = 0xff;
    }
    printf(" ...battery RAM: none saved, filled with $FF for this game\n");
}

// the PRG and CHR ROM sizes load_nes_file takes from an iNES / NES 2.0 header: an exponent-multiplier size as
// nes2_exponent_cartridge converts it (rounded up to whole 16 / 8 kB units, when the file holds it all; else as the
// header's bytes read)
static void cartridge_rom_sizes(const uint8_t* data, uint32_t size, uint32_t* prg, uint32_t* chr, uint32_t* exact) {
    uint32_t nes2 = (data[7] & 0x0c) == 0x08;
    *prg = ((nes2 ? ((data[9] & 0x0f) << 8) : 0) | data[4]) * 0x4000;
    *chr = ((nes2 ? ((data[9] & 0xf0) << 4) : 0) | data[5]) * 0x2000;
    *exact = *prg + *chr;
    if (nes2 && (((data[9] & 0x0f) == 0x0f) || ((data[9] & 0xf0) == 0xf0))) {
        uint32_t trainer = (data[6] & 4) ? 512 : 0;
        uint32_t exact_prg = ((data[9] & 0x0f) == 0x0f) ? nes2_exponent_size(data[4]) : *prg;
        uint32_t exact_chr = ((data[9] & 0xf0) == 0xf0) ? nes2_exponent_size(data[5]) : *chr;
        uint32_t prg_units = (exact_prg + 0x3fff) / 0x4000, chr_units = (exact_chr + 0x1fff) / 0x2000;
        if ((prg_units <= 0xfff) && (chr_units <= 0xfff) && ((uint64_t)16 + trainer + exact_prg + exact_chr <= size)) {
            *prg = prg_units * 0x4000;
            *chr = chr_units * 0x2000;
            *exact = exact_prg + exact_chr;
        }
    }
}

/*
 * Whether load_nes_file can take the file's ROM (cartridge_size_check's and frnes_rom_support's rule): more PRG or CHR
 * ROM than ROM_pages / VROM_pages hold (MAXIMUM_ROM_SIZE / MAXIMUM_VROM_SIZE), or a file shorter than its header,
 * trainer and ROM (load_nes_file copied the ROM from past the end of its buffer)
 */
#define CARTRIDGE_ROM_OK 0
#define CARTRIDGE_ROM_TOO_LARGE 1
#define CARTRIDGE_ROM_TRUNCATED 2
static int cartridge_rom_check(const uint8_t* data, uint32_t size, uint32_t* prg, uint32_t* chr) {
    uint32_t exact;
    cartridge_rom_sizes(data, size, prg, chr, &exact);
    if ((*prg > MAXIMUM_ROM_SIZE) || (*chr > MAXIMUM_VROM_SIZE)) {
        return CARTRIDGE_ROM_TOO_LARGE;
    }
    return ((uint64_t)16 + ((data[6] & 4) ? 512 : 0) + exact > size) ? CARTRIDGE_ROM_TRUNCATED : CARTRIDGE_ROM_OK;
}

// a .nes file with more PRG or CHR ROM than ROM_pages / VROM_pages hold (MAXIMUM_ROM_SIZE / MAXIMUM_VROM_SIZE): NULL
// (load_nes_file fails; the frontend reads the sizes from the header for its message), else the file
static uint8_t* cartridge_size_check(uint8_t* data, uint32_t size) {
    if ((size < 16) || memcmp(data, "NES\x1a", 4)) {
        return data;
    }
    uint32_t prg, chr;
    switch (cartridge_rom_check(data, size, &prg, &chr)) {
        case CARTRIDGE_ROM_OK:
            return data;
        case CARTRIDGE_ROM_TOO_LARGE:
            printf(" ...ERROR: %lu kB of PRG ROM and %lu kB of CHR ROM: FrNES takes %lu kB of each at most\n",
                (unsigned long)(prg / 1024), (unsigned long)(chr / 1024), (unsigned long)(MAXIMUM_ROM_SIZE / 1024));
            break;
        default:
            printf(" ...ERROR: truncated ROM file: %lu bytes, its header says %lu kB of PRG ROM and %lu kB of CHR ROM\n",
                (unsigned long)size, (unsigned long)(prg / 1024), (unsigned long)(chr / 1024));
            break;
    }
    free(data);
    return NULL;
}

// load_nes_file's file into a cartridge (mapper.h): board_number says whether it was an NSF tune until board_override
// has looked (no variable of its own)
__attribute__((cold, noinline)) uint8_t* cartridge_convert(uint8_t* data, uint32_t* size) {
    // ($4016 / $4017 reads' open bus bits: the NES's, until a Vs. System game's set up changes them (vs_setup). Here,
    // where every game's load comes, rather than in initialize_registers, which LTO inlines into the test builds' main)
    cpu_registers -> PAD_OpenMask = 0xe0;
    uint8_t nsf_tune = (*size >= 5) && (memcmp(data, "NESM\x1a", 5) == 0);
    data = fds_cartridge(data, size);
    if (data != NULL) {
        data = nsf_cartridge(data, size);
    }
    if (data != NULL) {
        data = nes2_exponent_cartridge(data, size);
        nes2_datach_cartridge(data, *size);
        data = smc_cartridge(data, size);
        data = cartridge_size_check(data, *size);
    }
    if (data != NULL) {
        ffe_keep_trainer(data, *size);
        gd_keep_trainer(data, *size);
    }
    board_number_set((nsf_tune && (data != NULL)) ? BOARD_NSF : 0);
    return data;
}

// nsf.c's entry points until it's linked in (weak: its own win): no tune is ever loaded
__attribute__((weak, cold)) uint8_t* nsf_cartridge(uint8_t* data, uint32_t* size) {
    (void)size;
    return data;
}

__attribute__((weak, cold)) void nsf_init() {
}

__attribute__((weak, cold)) void nsf_write(uint16_t address, uint8_t data) {
    (void)address;
    (void)data;
}

__attribute__((weak, cold)) void nsf_write_low(uint16_t address, uint8_t data) {
    (void)address;
    (void)data;
}

__attribute__((weak, cold)) uint8_t nsf_read_low(uint16_t address, uint8_t open_bus) {
    (void)address;
    return open_bus;
}

// mapper_init's default branch: 0 when the mapper is one of these
__attribute__((cold, noinline)) int board_init(uint32_t number) {
    board_number_set(board_id(number));
    board_6000 = 0;
    board_timer = (number == 65) || (number == 18) || (number == 20) || (number == 105) || (number == 555) || (number == 6) ||
        (number == 8) || (number == 17) || (number == 42) || (number == 40) || (number == 43) || (number == 304) || (number == 50) || (number == 368) || (number == 535) || (number == 359) || (number == 117) || (number == 106) || (number == 35) || (number == 253) || (number == 529) || (number == 362) || (number == 183) || (number == 222) || (number == 142) || (number == 56) || (number == 90) || (number == 209) || (number == 211) ||
        ((number == 12) && (board_submapper == 1)) || (number == 561) || (number == 562) || (number == 163) || (number == 162) || (number == 83) || (number == 264);
    board_low = (number == 20) || (number == 28) || (number == 228) || (number == 79) || (number == 113) || (number == 132) || (number == 36) || (number == 173) || (number == 136) || (number == 172) || (number == 555) || (number == 6) ||
        (number == 8) || (number == 17) || (number == 176) || (number == 178) || (number == 551) || (number == 314) || (number == 403) || (number == 133) || (number == 216) || (number == 43) || (number == 143) || (number == 123) || (number == 198) || (number == 445) || (number == 187) || (number == 120) || (number == 521) || (number == 304) || (number == 50) || (number == 368) || (number == 164) || (number == 558) || (number == 240) || (number == 225) || (number == 83) || (number == 264) || (number == 121) || (number == 90) || (number == 209) ||
        (number == 211) || (number == NSF_BOARD) || (number == 12) || (number == 224) || (number == 268) || (number == 215) || (number == 260) || (number == 189) || (number == 249) ||
        (number == 561) || (number == 562) || (number == 163) || (number == 162) || (number == 150) ||
        ((number >= 137) && (number <= 139)) || (number == 141);
    board_low_unsynced = (number == 20);
    board_own_irqs = (number == 20);
    board_saves = (number == 20);
    switch (number) {
        case 33: case 48:
            taito_tc0190_init();
            break;
        case 80: case 207: case 82:
            x1_init();
            break;
        case 32:
            g101_init();
            break;
        case 65:
            h3001_init();
            break;
        case 18:
            ss88006_init();
            break;
        case 89: case 93: case 184:
            sunsoft_simple_init();
            break;
        case 210:
            namco210_init();
            break;
        case 77:
            irem77_init();
            break;
        case 13:
            cprom_init();
            break;
        case 180:
            unrom180_init();
            break;
        case 78:
            jf16_init();
            break;
        case 70: case 152:
            bandai70_init();
            break;
        case 72: case 92:
            jf17_init();
            break;
        case 94: case 97:
            prg16_boards_init();
            break;
        case 140:
            jf13_init();
            break;
        case 188:
            karaoke188_init();
            break;
        case 105:
            nwc_init();
            break;
        case 6: case 8: case 17:
            ffe_init();
            break;
        case 42: case 40: case 43: case 304: case 50: case 368:
            m42_init();
            break;
        case 359:
            sb5013_init();
            break;
        case 117:
            m117_init();
            break;
        case 106:
            m106_init();
            break;
        case 35:
            m35_init();
            break;
        case 253:
            m253_init();
            break;
        case 529:
            m529_init();
            break;
        case 362:
            m362_init();
            break;
        case 183:
            m183_init();
            break;
        case 222:
            m222_init();
            break;
        case 142: case 56:
            ks202_init();
            break;
        case 193: case 235: case 60: case 396: case 314: case 286: case 374: case 403: case 434: case 283: case 439: case 29: case 133: case 202: case 203: case 216: case 230: case 285: case 336: case 219: case 246: case 171: case 290: case 81: case 62: case 261: case 143: case 149: case 51: case 61: case 190: case 120: case 346: case 312: case 306: case 309: case 521: case 530: case 46: case 415: case 125: case 522: case 103: case 535: case 305: case 302: case 307: case 167: case 519: case 438: case 449:
            small_boards_init();
            break;
        case 58: case 59: case 241: case 178: case 551: case 164: case 558: case 240:
            latch_boards_init();
            break;
        case 57: case 227: case 319: case 242: case 148: case 212: case 177: case 332: case 15: case 236: case 265: case 226: case 221: case 63:
        case 200: case 201: case 225: case 108:
            latch_more_init();
            break;
        case 150: case 137: case 138: case 139: case 141:
            sachen374_init();
            break;
        case 90: case 209: case 211:
            jy_init();
            break;
        case 176:
            // (the MMC3 for its registers and IRQ counter, its banks mapped by fk23_apply)
            mmc3_board_init();
            fk23_init();
            break;
        case 83: case 264:
            cony_init();
            break;
        case 91:
            // (the MMC3 for its IRQ counter, its banks mapped by jy91_apply)
            mmc3_board_init();
            jy91_init();
            break;
        case 28:
            a53_init();
            break;
        case 228:
            action52_init();
            break;
        case 79: case 113:
        case 144:
            nina0306_init();
            break;
        case 112:
            ntdec112_init();
            break;
        case 132: case 36: case 173: case 136: case 172:
            txc132_init();
            break;
        case 156:
            daou156_init();
            break;
        case 47: case 37: case 45: case 52: case 555:
            // the MMC3 with the outer bank in the page tables: the MMC3's own code and dispatch from here on
            mmc3_board_init();
            board_mmc3_outer_init();
            break;
        case 12: case 134: case 224: case 268: case 313: case 195: case 215: case 260: case 121: case 189: case 196: case 249:
        case 534: case 422: case 115: case 248: case 74: case 191: case 192: case 194: case 114: case 197: case 205: case 395: case 267: case 377: case 198: case 445: case 516: case 361: case 348: case 44: case 250: case 126: case 245: case 123: case 187: case 287: case 366: case 263: case 254:
            if (number == 121) {
                memset(M121, 0, 8);
                M121[3] = 0x80;
            }
            // (215's submapper 1 and 12's past 1 aren't loaded: board_supported)
            if ((number == 12) && (board_submapper == 1)) {
                smc_init();
                break;
            }
            mmc3_board_init();
            mmc3x_init();
            break;
        case 561: case 562:
            gd_init();
            break;
        case 163: case 162:
            nanjing_init();
            break;
        case 185:
            cnrom185_init();
            break;
        case 87:
            jf87_init();
            break;
        case 86:
            jf13_init();
            break;
        case 20:
            fds_init();
            break;
        case NSF_BOARD:
            nsf_init();
            break;
        case 99:
            vs99_init();
            break;
        default:
            board_timer = 0;
            return -1;
    }
    return 0;
}

/*
 * The Namco 163's sound RAM as the CPU reads it back: the chip updates one channel every 15 CPU cycles (channel 7, then
 * down through the ($7F >> 4 & 7) + 1 enabled ones), adding its frequency to its phase modulo its wave length. Games
 * read the phases (Rolling Thunder uses them as random numbers), so reads and writes of the RAM bring the phases up
 * to the current cycle first, as Mesen 2's Namco163Audio runs them. apu_synth plays the sound on its own.
 */
// (where Mesen 2's count is at our cycle 0: found against its trace of Rolling Thunder, which matches with it)
#define N163_FIRST_COUNT 9
static uint32_t n163_cycle;
static uint8_t n163_count, n163_channel, n163_halt;

void board_n163_reset() {
    n163_cycle = 0;
    n163_count = N163_FIRST_COUNT;
    n163_channel = 7;
    n163_halt = 0;
}

void board_n163_halt(uint8_t halt) {
    n163_halt = halt;
}

uint8_t board_n163_halted(void) {
    return n163_halt;
}

static void n163_update_channel(uint8_t* ram, uint32_t channel) {
    uint8_t* r = &ram[0x40 + channel * 8];
    uint32_t phase = r[1] | (r[3] << 8) | (r[5] << 16);
    uint32_t frequency = r[0] | (r[2] << 8) | ((r[4] & 3) << 16);
    uint32_t length = 256 - (r[4] & 0xfc);
    phase = (phase + frequency) % (length << 16);
    r[1] = phase & 0xff;
    r[3] = (phase >> 8) & 0xff;
    r[5] = (phase >> 16) & 0xff;
}

void board_n163_sync(uint8_t* ram, uint32_t now) {
    uint32_t cycles = now - n163_cycle;
    n163_cycle = now;
    if (n163_halt) {
        return;
    }
    uint32_t total = n163_count + cycles;
    uint32_t updates = total / 15;
    n163_count = total % 15;
    // Each update adds a channel's frequency to its phase, modulo its wave length, the channels taken in turn from 7
    // down to the last enabled one. Updates in a row on the same registers (no write between two syncs) come to each
    // channel's phase + its updates' count x its frequency, modulo its length: one step per channel instead of one
    // per 15 cycles (Mappy Kids's map and slot machine: thousands of updates a frame, each with a division)
    uint32_t last = 7 - ((ram[0x7f] >> 4) & 7);
    // (a channel past the last enabled one, left by a write that enabled fewer: its update first, as one at a time)
    while (updates && (n163_channel < last)) {
        n163_update_channel(ram, n163_channel);
        n163_channel = 7;
        updates--;
    }
    if (!updates) {
        return;
    }
    uint32_t count = 8 - last, first = 7 - n163_channel;
    for (uint32_t j = 0; j < count; j++) {
        // (the j-th channel in turn from the current one gets the updates j, j + count, ...)
        uint32_t taken = (updates > j) ? (updates - j - 1) / count + 1 : 0;
        if (!taken) {
            break;
        }
        uint32_t channel = 7 - ((first + j) % count);
        uint8_t* r = &ram[0x40 + channel * 8];
        uint32_t phase = r[1] | (r[3] << 8) | (r[5] << 16);
        uint32_t frequency = r[0] | (r[2] << 8) | ((r[4] & 3) << 16);
        uint32_t length = (256 - (r[4] & 0xfc)) << 16;
        phase = (uint32_t)(((uint64_t)phase + (uint64_t)taken * frequency) % length);
        r[1] = phase & 0xff;
        r[3] = (phase >> 8) & 0xff;
        r[5] = (phase >> 16) & 0xff;
    }
    n163_channel = 7 - ((first + updates) % count);
}

#include "state.h"

#ifndef FRNES_HOST
/*
 * (memory.h; host_memory.c has the host's) the page a slot shows, read back from the MMU: the UTLB entry mapping the
 * slot's address, from the memory mapped UTLB address and data arrays (read from P2, uncached, as the SH4's manual
 * has them accessed), its physical page as the P1 address the buffers have. Here, outside the LTO code, and through
 * the arrays instead of memory.c's handles and shadow table: a getter in memory.c, or those becoming visible to it,
 * moved the release's ordinary .text
 */
#define UTLB_ADDRESS_ARRAY 0xf6000000
#define UTLB_DATA_ARRAY 0xf7000000
static FRNES_BOARD_TEXT __attribute__((noinline)) uintptr_t utlb_physical(uintptr_t address) {
    for (uint32_t entry = 0; entry < 64; entry++) {
        uint32_t tag = *(volatile uint32_t*)(UTLB_ADDRESS_ARRAY | (entry << 8));
        if ((tag & 0x100) && ((tag & 0xfffffc00) == address)) {
            return 0x80000000 | (*(volatile uint32_t*)(UTLB_DATA_ARRAY | (entry << 8)) & 0x1ffffc00);
        }
    }
    return 0;
}

FRNES_BOARD_TEXT uint8_t* memory_slot_page(uint32_t slot) {
    uintptr_t address;
    if (slot < MEMORY_SLOT_WRAM) {
        address = ROMBANK0_ADDR + (slot << 13);
    } else if (slot == MEMORY_SLOT_WRAM) {
        if (WRAM == NULL) {
            return NULL;
        }
        address = WRAM_ADDR;
    } else if (slot < MEMORY_SLOT_NAMETABLE) {
        address = PATTERN_TABLES_ADDR + ((slot - MEMORY_SLOT_CHR) << 10);
    } else {
        address = NAMETABLE0_ADDR + ((slot - MEMORY_SLOT_NAMETABLE) << 10);
    }
    uintptr_t (*in_p2)(uintptr_t) = (uintptr_t (*)(uintptr_t))(((uintptr_t)utlb_physical & 0x1fffffff) | 0xa0000000);
    return (uint8_t*)in_p2(address);
}
#endif

_Static_assert(MEMORY_SLOTS == sizeof(((board_window_t*)0) -> page) / sizeof(uint8_t*), "board_window_t");
#define WINDOW_NONE 0
#define WINDOW_ROM 1
#define WINDOW_VROM 2
#define WINDOW_CHR_RAM 3
#define WINDOW_VRAM 4
#define WINDOW_WRAM 5
#define WINDOW_BOARD 6
#define WINDOW_UNKNOWN 0xffffffff

// (the buffers through the page tables, cartridge.c, and VRAM / WRAM's size from mapper.c's hooks: ROM, VROM,
// cartridge_VRAM, VRAM and WRAM_size are only the LTO code's in the release, and a reference from here moved it)
static FRNES_BOARD_TEXT uint8_t* window_area(uint32_t area, const board_memory_t* memory, uint32_t* size) {
    // (the MMC3 outer bank boards' page tables are the selected block: the whole ROM is in the tables as loaded)
    if (((cartridge_mapper_number == 47) || (cartridge_mapper_number == 37)) && board_state &&
        ((area == WINDOW_ROM) || (area == WINDOW_VROM))) {
        const struct mmc3_outer* o = board_state;
        if (area == WINDOW_ROM) {
            *size = o->rom_pages * 0x2000;
            return o->rom[0];
        }
        *size = o->vrom_pages * 0x400;
        return o->vrom_pages ? o->vrom[0] : NULL;
    }
    switch (area) {
        case WINDOW_ROM: *size = num_8k_ROM_pages * 0x2000; return ROM_pages[0];
        case WINDOW_VROM: *size = num_1k_VROM_pages * 0x400; return num_1k_VROM_pages ? VROM_pages[0] : NULL;
        case WINDOW_CHR_RAM: *size = num_1k_VRAM_pages * 0x400; return num_1k_VRAM_pages ? (num_1k_VROM_pages ? VRAM_pages[0] : VROM_pages[0]) : NULL;
        case WINDOW_VRAM: *size = 0x3000; return memory -> vram;
        case WINDOW_WRAM: *size = memory -> wram_size; return WRAM;
        // (185's disabled CHR: a buffer of the bus's values, board_state; 236's mode 1 PRG copy)
        case WINDOW_BOARD:
            *size = (cartridge_mapper_number == 185) ? 0x2000 : ((cartridge_mapper_number == 236) || (cartridge_mapper_number == 235)) ? 0x8000 : 0;
            return ((cartridge_mapper_number == 185) || (cartridge_mapper_number == 236) || (cartridge_mapper_number == 235)) ? board_state : NULL;
    }
    *size = 0;
    return NULL;
}

FRNES_BOARD_TEXT void board_window_save(state_writer_t* writer, const board_memory_t* memory) {
    for (uint32_t slot = 0; slot < MEMORY_SLOTS; slot++) {
        uint8_t* page = memory_slot_page(slot);
        uint32_t ref = page ? WINDOW_UNKNOWN : WINDOW_NONE;
        for (uint32_t area = WINDOW_ROM; page && (area <= WINDOW_BOARD); area++) {
            uint32_t size;
            uint8_t* base = window_area(area, memory, &size);
            if (base && (page >= base) && (page < base + size)) {
                ref = (area << 24) | (uint32_t)(page - base);
                break;
            }
        }
        STATE_PUT_VAR(writer, ref);
    }
}

// the slots' pages from a state, checked: 0, or -1 (a page outside its buffer, one the save couldn't place, WRAM where
// this cartridge has none, or a nametable from CHR ROM: Sunsoft-4's, not saved yet) with nothing changed
FRNES_BOARD_TEXT int board_window_read(state_reader_t* reader, board_window_t* window, const board_memory_t* memory) {
    // (a board mapping nametables from CHR ROM maps them itself after the load: BOARD_OWN_NAMETABLES)
    uint32_t own_nametables = memory -> flags & BOARD_OWN_NAMETABLES;
    for (uint32_t slot = 0; slot < MEMORY_SLOTS; slot++) {
        uint32_t ref, size;
        if (STATE_GET_VAR(reader, ref) != 0) {
            return -1;
        }
        uint8_t* base = window_area(ref >> 24, memory, &size);
        uint32_t offset = ref & 0xffffff;
        uint32_t length = (slot < MEMORY_SLOT_CHR) ? 0x2000 : 0x400;
        int bad = (ref == WINDOW_NONE) ? ((slot != MEMORY_SLOT_WRAM) || WRAM) : (!base || (offset + length > size));
        if (bad || ((slot >= MEMORY_SLOT_NAMETABLE) && !own_nametables && ((ref >> 24) != WINDOW_VRAM))) {
            return -1;
        }
        window -> page[slot] = (ref == WINDOW_NONE) ? NULL : base + offset;
    }
    return 0;
}

FRNES_BOARD_TEXT void board_window_apply(const board_window_t* window, const board_memory_t* memory) {
    for (uint32_t slot = MEMORY_SLOT_PRG; slot < MEMORY_SLOT_WRAM; slot++) {
        set_6502_bank_pointer(slot, window -> page[slot]);
    }
    if (window -> page[MEMORY_SLOT_WRAM]) {
        set_wram_bank_pointer(window -> page[MEMORY_SLOT_WRAM]);
    }
    for (uint32_t slot = 0; slot < 8; slot++) {
        set_ppu_pattern_table_bank_pointer(slot, window -> page[MEMORY_SLOT_CHR + slot]);
    }
    if (memory -> flags & BOARD_OWN_NAMETABLES) {
        return;
    }
    const uint8_t* const* nametables = (const uint8_t* const*)&window -> page[MEMORY_SLOT_NAMETABLE];
    set_ppu_nametable_pages((uint8_t)((nametables[0] - memory -> vram) >> 10), (uint8_t)((nametables[1] - memory -> vram) >> 10),
        (uint8_t)((nametables[2] - memory -> vram) >> 10), (uint8_t)((nametables[3] - memory -> vram) >> 10));
}

/*
 * The state the boards here keep (mapper.c's board hooks take it after the window and the mapper's own registers):
 * the board's registers and counters by value, for the boards board_init sets up, and the Namco 163's sound RAM
 * update position (board_n163_sync). board_extra_size is its size for the loaded board (a load checks it first)
 */
#define G101_STATE(X, A) X(g101_prg0) X(g101_mode)
#define H3001_STATE(X, A) X(h3001_counter) X(h3001_reload) X(h3001_enabled)
#define SS88006_STATE(X, A) A(ss88006_prg) A(ss88006_chr) A(ss88006_reload) X(ss88006_enabled) X(ss88006_counter) \
    X(ss88006_mask)
#define X1_STATE(X, A) A(x1_registers)
#define NAMCO175_STATE(X, A) X(namco175_write_enable)
#define IREM77_STATE(X, A) X(irem77_prg)
#define A53_STATE(X, A) X(a53_select) A(a53_regs) X(a53_mirroring)
#define ACTION52_STATE(X, A) A(action52_ram)
#define N163_SYNC_STATE(X, A) X(n163_cycle) X(n163_count) X(n163_channel) X(n163_halt)
#define BOARD_NO_STATE(X, A)

// which list a board's state is (the Namco 163's, or one board_init set up), -1 for the boards whose state can't be
// saved
static FRNES_BOARD_TEXT int board_extra_kind(uint32_t number) {
    if (number == 19) {
        return 1;
    }
    if (board_id(number) != board_number) {
        // (the board board_override gave the header's number: 552 runs as the X1-017's 82, a CRC override as its own.
        // Not mapper.c's number: the MMC3 boards here run as its 4)
        if ((board_number <= BOARD_COOLBOY) || (board_number == BOARD_SGD) || (board_number == BOARD_TGD) ||
            (board_number == BOARD_HP898F) || (board_number == BOARD_RESET_TXROM) || (board_number == BOARD_SUPER40) ||
            (board_number == BOARD_HPXX) || (board_number == BOARD_YOKO) || (board_number == BOARD_YC0309) ||
            (board_number == BOARD_ING022) || (board_number == BOARD_T262)) {
            return -1;
        }
        number = board_number;
    }
    switch (number) {
        case 32: return 2;
        case 65: return 3;
        case 18: return 4;
        case 80: case 207: case 82: return 5;
        case 210: return 6;
        case 77: return 7;
        case 28: return 8;
        case 228: return 9;
        case 33: case 48: case 89: case 93: case 184: case 13: case 180: case 78: case 79: case 113: case 112: case 132: case 36: case 173: case 136: case 172: case 156: case 185: case 87: case 86:
        case 99: case 144: case 70: case 152: case 72: case 92: case 94: case 97: case 140: case 188: case 105: case 6: case 8: case 17: case 42: case 40: case 43: case 304: case 50: case 368: case 359: case 117: case 106: case 35: case 253: case 529: case 362: case 183: case 222: case 142: case 56: case 193: case 235: case 60: case 396: case 314: case 286: case 374: case 403: case 434: case 283: case 439: case 29: case 133: case 202: case 203: case 216: case 230: case 285: case 336: case 219: case 246: case 171: case 290: case 81: case 62: case 261: case 143: case 149: case 51: case 61: case 190: case 120: case 346: case 312: case 306: case 309: case 521: case 530: case 46: case 415: case 125: case 522: case 103: case 535: case 305: case 302: case 307: case 167: case 519: case 438: case 449: case 58: case 59:
        case 241: case 178: case 551: case 164: case 558: case 240: case 90: case 209: case 211: case 57: case 227: case 319: case 150:
        case 137: case 138: case 139: case 141: case 242: case 148: case 212:
        case 177: case 332: case 15: case 265: case 226: case 221: case 63: case 200: case 201: case 225: case 108:
            return 0;
        case 236:
            return 20;
        case 47: case 37:
            return 10;
        case 555:
            return 12;
        case 45: case 52:
            return 13;
        case 176:
            return 14;
        case 12:
            return (board_submapper == 1) ? 16 : 15;
        case 134: case 224: case 268: case 313: case 195: case 215: case 260: case 121: case 189: case 196: case 249:
        case 534: case 422: case 115: case 248: case 74: case 191: case 192: case 194: case 114: case 197: case 205: case 395: case 267: case 377: case 198: case 445: case 516: case 361: case 348: case 44: case 250: case 126: case 245: case 123: case 187: case 287: case 366: case 263: case 254:
            return 15;
        case 91:
            return 17;
        case 83: case 264:
            return 19;
        case 561: case 562:
            return 16;
        case 163: case 162:
            return 18;
        case 20:
            return 11;
        case BOARD_NSF:
            // (the NSF player: no save states; its cartridge number, NSF_BOARD, isn't board_number either)
            return -1;
    }
    return -1;
}

#define BOARD_EXTRA_CASES(F) F(0, BOARD_NO_STATE) F(1, N163_SYNC_STATE) F(2, G101_STATE) F(3, H3001_STATE) \
    F(4, SS88006_STATE) F(5, X1_STATE) F(6, NAMCO175_STATE) F(7, IREM77_STATE) F(8, A53_STATE) F(9, ACTION52_STATE)

// The MMC3 outer bank boards' bank: where the page tables start in the tables as loaded (mmc3_outer_select doesn't
// keep the value written), and the value that selects that block again
static FRNES_BOARD_TEXT void mmc3_outer_bases(uint32_t* prg_base, uint32_t* chr_base) {
    const struct mmc3_outer* o = board_state;
    *prg_base = *chr_base = 0;
    for (uint32_t i = 0; i < o->rom_pages; i++) {
        if (o->rom[i] == ROM_pages[0]) {
            *prg_base = i;
            break;
        }
    }
    for (uint32_t i = 0; i < o->vrom_pages; i++) {
        if (o->vrom[i] == VROM_pages[0]) {
            *chr_base = i;
            break;
        }
    }
}

static FRNES_BOARD_TEXT int mmc3_outer_value(uint32_t prg_base, uint32_t chr_base) {
    for (uint32_t value = 0; value < 8; value++) {
        uint32_t base, chr;
        if (cartridge_mapper_number == 47) {
            base = (value & 1) * 16;
            chr = (value & 1) * 128;
        } else {
            base = (value <= 2) ? 0 : (value == 3) ? 8 : (value == 7) ? 24 : 16;
            chr = (value & 4) ? 128 : 0;
        }
        const struct mmc3_outer* o = board_state;
        if (((base % o->rom_pages) == prg_base) && (!o->vrom_pages || ((chr % o->vrom_pages) == chr_base))) {
            return (int)value;
        }
    }
    return -1;
}

// -1: this board's state can't be saved
FRNES_BOARD_TEXT int board_extra_size(uint32_t number) {
#define BOARD_EXTRA_SIZE_CASE(kind, list) case kind: return (int)STATE_SIZE(list);
    switch (board_extra_kind(number)) {
        BOARD_EXTRA_CASES(BOARD_EXTRA_SIZE_CASE)
        case 10: return 1;
        case 11: return (int)fds_state_size();
        case 12: return 0;
        case 13: return 0;
        case 14: return 0;
        case 15: return 0;
        case 16: return 0;
        case 17: return 0;
        case 18: return 0;
        case 19: return 0;
        case 20: return 0;
    }
    return -1;
}

FRNES_BOARD_TEXT void board_extra_save(state_writer_t* writer, uint32_t number) {
#define BOARD_EXTRA_SAVE_CASE(kind, list) case kind: { STATE_SAVE(list) } break;
    switch (board_extra_kind(number)) {
        BOARD_EXTRA_CASES(BOARD_EXTRA_SAVE_CASE)
        case 10: {
            uint32_t prg_base, chr_base;
            mmc3_outer_bases(&prg_base, &chr_base);
            uint8_t value = (uint8_t)mmc3_outer_value(prg_base, chr_base);
            STATE_PUT_VAR(writer, value);
        } break;
        case 11:
            fds_state_save(writer);
            break;
    }
}

// (after board_extra_size's check)
FRNES_BOARD_TEXT void board_extra_load(state_reader_t* reader, uint32_t number) {
#define BOARD_EXTRA_LOAD_CASE(kind, list) case kind: { STATE_LOAD(list) } break;
    switch (board_extra_kind(number)) {
        BOARD_EXTRA_CASES(BOARD_EXTRA_LOAD_CASE)
        case 10: {
            // (the outer bank's page tables; the window maps the MMC3's banks in them. Not mmc3_reapply: its ppu_sync
            // would run the PPU state just loaded)
            uint8_t value = 0;
            STATE_GET_VAR(reader, value);
            mmc3_outer_tables(value);
        } break;
        case 12:
            // (mapper 555: its outer register in the register page, loaded with it)
            mmc3_outer_tables(EVENT2 -> outer);
            break;
        case 13:
            // (mappers 45 and 52: their registers in the register page, loaded with it)
            mmc3_outer_tables(0);
            break;
        case 14:
            // (mapper 176: its registers in the register page; its banks from them, over the window's)
            fk23_apply();
            break;
        case 15:
            // (12, 134, 224, 268, 313, 195: their registers in the register page)
            mmc3x_apply();
            break;
        case 17:
            // (91: its registers in the register page)
            jy91_apply();
            break;
        case 20:
            // (236: its latches in the register page; mode 1's copy made again)
            latch236_apply();
            break;
        case 19:
            // (83: its registers in the register page; the line events from the PPU's position again)
            CONY -> next_line_set = 0;
            cony_apply();
            break;
        case 18:
            // (Nanjing: its registers in the register page; the switch points from the PPU's position again)
            NANJING -> next_set = 0;
            break;
        case 16:
            // (the Super Magic Card: its registers in the register page; the line events from its position again)
            SMC -> next_line_set = 0;
            break;
        case 11:
            fds_state_load(reader);
            break;
    }
}

/*
 * The board hooks (state.h's mapper_board_state), for every board whose state can be saved: the window (the page each
 * slot shows), mapper.c's variables (mapper_state: all of its boards', by value), then the part kept here
 * (board_extra_*: board_init's boards, the Namco 163's sound RAM position, the MMC3 outer bank, the FDS). A load checks
 * the version, the size and the window first, then loads the variables and the part here, maps the window and lets
 * the boards map again what they derive from their registers. Here, out of the link time optimized code: in mapper.c
 * these hooks moved its partitions, and with them ordinary .text and .bss
 */
#define BOARD_STATE_VERSION 1

// the part kept here: its size, 0 for mapper.c's boards
static FRNES_BOARD_TEXT uint32_t board_extra_bytes() {
    int size = board_extra_size(cartridge_mapper_number);
    return (size > 0) ? (uint32_t)size : 0;
}

static FRNES_BOARD_TEXT void board_info(board_memory_t* memory) {
    mapper_state(MAPPER_STATE_INFO, NULL, memory);
    memory -> flags = ((memory -> mapper == 19) || (memory -> mapper == 68)) ? BOARD_OWN_NAMETABLES : 0;
}

static FRNES_BOARD_TEXT void board_state_save(state_writer_t* writer) {
    board_memory_t memory;
    board_info(&memory);
    board_window_save(writer, &memory);
    uint32_t size = mapper_state(MAPPER_STATE_SIZE, NULL, NULL);
    uint8_t* variables = aligned_alloc(32, (size + 31) & ~31u);
    if (variables == NULL) {
        writer -> failed = 1;
        return;
    }
    mapper_state(MAPPER_STATE_SAVE, variables, NULL);
    state_put(writer, variables, size);
    free(variables);
    if (board_extra_bytes()) {
        board_extra_save(writer, cartridge_mapper_number);
    }
}

static FRNES_BOARD_TEXT int board_state_load(state_reader_t* reader, uint16_t version) {
    board_memory_t memory;
    board_window_t window;
    board_info(&memory);
    uint32_t size = mapper_state(MAPPER_STATE_SIZE, NULL, NULL);
    uint32_t extra = board_extra_bytes();
    if ((version != BOARD_STATE_VERSION) || (reader -> size - reader -> at != BOARD_WINDOW_SIZE + size + extra) ||
        (board_window_read(reader, &window, &memory) != 0)) {
        return -1;
    }
    uint8_t* variables = aligned_alloc(32, (size + 31) & ~31u);
    if (variables == NULL) {
        return -1;
    }
    state_get(reader, variables, size);
    mapper_state(MAPPER_STATE_LOAD, variables, NULL);
    free(variables);
    // (with no bytes of its own too: the boards whose registers are in the register page map their banks from them)
    if (board_extra_size(cartridge_mapper_number) >= 0) {
        board_extra_load(reader, cartridge_mapper_number);
    }
    board_window_apply(&window, &memory);
    mapper_state(MAPPER_STATE_AFTER, NULL, NULL);
    // (the Vs. System's odd frame timer, which vs_odd_timer_start starts at the first $4016 write: running in the state
    // when its board timer is, else a load into a fresh game would start it again at the next write)
    if ((cpu_registers -> VS_Flags & VS_SYSTEM) && vs_rgb_timer_board() && mapper_cpu_timer) {
        board_timer = 1;
    }
    return 0;
}

static const board_state_t board_state_hooks FRNES_COLD_RODATA = { BOARD_STATE_VERSION, board_state_save, board_state_load };
static const board_state_t board_state_none FRNES_COLD_RODATA = { BOARD_STATE_VERSION, NULL, NULL };

// the loaded board's state hooks (state.h), NULL for boards whose state can't be saved
FRNES_BOARD_TEXT const board_state_t* mapper_board_state(uint32_t number) {
    (void)number;
    board_memory_t memory;
    board_info(&memory);
    switch (memory.mapper) {
        case 0:
            return &board_state_none;
        // mapper.c's boards (4 is also the MMC3 outer bank boards, 47 / 37; 64 Tengen 800037, 158)
        case 1: case 2: case 3: case 7: case 11: case 30: case 34: case 66: case 75: case 111:
        case 4: case 118: case 119: case 9: case 10: case 69: case 24: case 26: case 85: case 21: case 22: case 23: case 25:
        case 64: case 5: case 19: case 16: case 153: case 159: case 206: case 76: case 88: case 95: case 154: case 71:
        case 232: case 68: case 73: case 67:
            return &board_state_hooks;
        default:
            // the boards here (board_init), those whose state can be saved
            return (board_extra_size(cartridge_mapper_number) >= 0) ? &board_state_hooks : NULL;
    }
}

/*
 * input.h's settings. The sources start as INPUT_SOURCE_AUTO (kept as source - INPUT_SOURCE_AUTO in vs_settings, so
 * the cold .bss's zero is the default)
 */
FRNES_BOARD_TEXT void input_set_port_source(uint8_t nes_port, uint8_t source) {
    if (nes_port < 4) {
        vs_settings[VS_SETTING_SOURCES + nes_port] = (uint8_t)(source - INPUT_SOURCE_AUTO);
    }
}

FRNES_BOARD_TEXT uint8_t input_port_source(uint8_t nes_port) {
    return (nes_port < 4) ? (uint8_t)(vs_settings[VS_SETTING_SOURCES + nes_port] + INPUT_SOURCE_AUTO) : INPUT_SOURCE_NONE;
}

FRNES_BOARD_TEXT void input_set_port_device(uint8_t nes_port, uint8_t device) {
    if (nes_port < 2) {
        vs_settings[nes_port ? VS_SETTING_ZAPPER_PORT : VS_SETTING_PORT0_DEVICE] = device;
    }
}

FRNES_BOARD_TEXT void input_set_ports_disabled(uint8_t mask) {
    vs_settings[VS_SETTING_DISABLED] = mask;
}

FRNES_BOARD_TEXT uint8_t input_ports_disabled(void) {
    return vs_settings[VS_SETTING_DISABLED];
}

FRNES_BOARD_TEXT int input_paddle_chosen(void) {
    return vs_settings[VS_SETTING_ZAPPER_PORT] == INPUT_DEVICE_PADDLE;
}

FRNES_BOARD_TEXT void input_set_four_score(uint8_t mode) {
    vs_settings[VS_SETTING_FOUR_SCORE] = mode;
}

FRNES_BOARD_TEXT uint8_t input_four_score_active(void) {
    return cpu_registers ? PAD_INPUT -> four_score : INPUT_FOUR_SCORE_NONE;
}

FRNES_BOARD_TEXT uint8_t input_port_device_active(uint8_t nes_port) {
    if ((cpu_registers != NULL) && (cpu_registers -> VS_Flags & VS_VAUS) && (nes_port == 1)) {
        return INPUT_DEVICE_PADDLE;
    }
    if ((cpu_registers == NULL) || !(cpu_registers -> VS_Flags & VS_ZAPPER)) {
        return INPUT_DEVICE_PAD;
    }
    for (int i = 0; i < ZAPPER_SLOTS; i++) {
        if (ZAPPER_SLOT(i) -> kind && (ZAPPER_NES_PORT(ZAPPER_SLOT(i)) == nes_port)) {
            return INPUT_DEVICE_ZAPPER;
        }
    }
    return INPUT_DEVICE_PAD;
}

/*
 * The strobe's fall ($4016 write, 6502_writebyte.c, only with PAD_INPUT -> strobe_fall set). Vs. Ice Climber's
 * protection (hardware type 4: PAD_STROBE_VS_START): both pads' bit 3 (Start) set, as the game wants it. The four
 * players' bits (PAD_STROBE_FOUR_SCORE).
 * The NES Four Score: after each port's 8 bits ($4016 player 1's, $4017 player 2's), player 3's / 4's 8 bits, then the
 * signature ($10 on $4016, $20 on $4017, low bit first), then 1s. The Famicom four player adapter: players 3 and 4 on
 * bit 1 of $4016 / $4017, 8 bits, then 0s
 */
FRNES_BOARD_TEXT void input_strobe_fall(void) {
    _pad_input_t* input = PAD_INPUT;
    if (input -> strobe_fall & PAD_STROBE_VAUS) {
        vaus_strobe();
    }
    if (input -> strobe_fall & PAD_STROBE_VS_START) {
        cpu_registers -> PAD1_Latch |= NES_PAD_START;
        cpu_registers -> PAD2_Latch |= NES_PAD_START;
    }
    if (!(input -> strobe_fall & PAD_STROBE_FOUR_SCORE)) {
        return;
    }
    if (input -> four_score == INPUT_FOUR_SCORE_NES) {
        input -> tail_inverted[0] = ~(input -> pad3 | (0x10 << 8) | 0xff0000u);
        input -> tail_inverted[1] = ~(input -> pad4 | (0x20 << 8) | 0xff0000u);
        input -> d1[0] = input -> d1[1] = 0;
    } else {
        input -> tail_inverted[0] = input -> tail_inverted[1] = 0;
        input -> d1[0] = input -> pad3;
        input -> d1[1] = input -> pad4;
    }
}


/*
 * The Zapper turned off or on in the game (the frontend's menu, between frames; on again at each load): off, it stays
 * plugged in but idle (no trigger, no light, no timer events), the reticle hidden, and its controller drives the pad
 * as usual; on, the controller's stick, D-pad, A and B are the Zapper's
 */
FRNES_BOARD_TEXT void zapper_set_enabled(uint8_t nes_port, uint8_t on) {
    if ((cpu_registers == NULL) || (nes_port > 1)) {
        return;
    }
    uint8_t bit = 1 << nes_port;
    PAD_INPUT -> zapper_off = on ? (PAD_INPUT -> zapper_off & ~bit) : (PAD_INPUT -> zapper_off | bit);
    if (!on && (cpu_registers -> VS_Flags & VS_ZAPPER)) {
        for (int i = 0; i < ZAPPER_SLOTS; i++) {
            zapper_state_t* z = ZAPPER_SLOT(i);
            if (!z -> kind || (ZAPPER_NES_PORT(z) != nes_port)) {
                continue;
            }
            z -> sense = 0;
            zapper_light(z, 0);
            if (z -> kind != ZAPPER_VS) {
                ((z -> kind == ZAPPER_PORT1) ? &cpu_registers -> PAD1_Extra : &cpu_registers -> PAD2_Extra)[0] &= ~ZAPPER_TRIGGER;
            }
        }
    }
}

FRNES_BOARD_TEXT uint8_t zapper_enabled(uint8_t nes_port) {
    return (cpu_registers == NULL) || (nes_port > 1) || !(PAD_INPUT -> zapper_off & (1 << nes_port));
}

/*
 * Movies with players 3 and 4 or the Zappers (input_recording.h's version 4, movie.c): the extension block for the
 * loaded game (0 when it needs none: no Zapper, no four players), each frame's extension from what the frame used,
 * and a played frame's applied, in place of the controllers. Flags: INPUT_EXT_ZAPPER_ON(n) the Zapper on NES port n
 * on (zapper_set_enabled), INPUT_EXT_TRIGGER(n) its trigger; x0 / y0 and x1 / y1 the Zappers' aim (y 0xff: off the
 * screen)
 */
FRNES_BOARD_TEXT int input_ext_block(input_recording_ext_t* ext) {
    memset(ext, 0, sizeof(*ext));
    ext -> block_version = INPUT_RECORDING_EXT_BLOCK_VERSION;
    ext -> port_device[0] = input_port_device_active(0);
    ext -> port_device[1] = input_port_device_active(1);
    ext -> four_score = input_four_score_active();
    return (cpu_registers -> VS_Flags & (VS_ZAPPER | VS_VAUS)) || ext -> four_score;
}

FRNES_BOARD_TEXT void input_ext_capture(input_recording_ext_run_t* ext) {
    memset(ext, 0, sizeof(*ext));
    ext -> pad3 = PAD_INPUT -> pad3;
    ext -> pad4 = PAD_INPUT -> pad4;
    ext -> y0 = ext -> y1 = 0xff;
    // (the Vaus paddle: its knob in x1, its button the trigger's flag)
    if (cpu_registers -> VS_Flags & VS_VAUS) {
        ext -> x1 = vaus_knob();
        ext -> flags |= vaus_fire() ? INPUT_EXT_TRIGGER(1) : 0;
    }
    if (!(cpu_registers -> VS_Flags & VS_ZAPPER)) {
        return;
    }
    for (int i = 0; i < ZAPPER_SLOTS; i++) {
        const zapper_state_t* z = ZAPPER_SLOT(i);
        if (!z -> kind) {
            continue;
        }
        uint8_t port = ZAPPER_NES_PORT(z);
        int trigger = (z -> kind == ZAPPER_VS) ? ((cpu_registers -> PAD1_Latch & ZAPPER_VS_TRIGGER) != 0) :
            ((((z -> kind == ZAPPER_PORT1) ? cpu_registers -> PAD1_Extra : cpu_registers -> PAD2_Extra) & ZAPPER_TRIGGER) != 0);
        ext -> flags |= (zapper_enabled(port) ? INPUT_EXT_ZAPPER_ON(port) : 0) | (trigger ? INPUT_EXT_TRIGGER(port) : 0);
        if (port) {
            ext -> x1 = z -> x;
            ext -> y1 = z -> y;
        } else {
            ext -> x0 = z -> x;
            ext -> y0 = z -> y;
        }
    }
}

FRNES_BOARD_TEXT void input_ext_apply(const input_recording_ext_run_t* ext) {
    PAD_INPUT -> pad3 = ext -> pad3;
    PAD_INPUT -> pad4 = ext -> pad4;
    if (cpu_registers -> VS_Flags & VS_VAUS) {
        vaus_set((uint8_t)ext -> x1, (ext -> flags & INPUT_EXT_TRIGGER(1)) != 0);
    }
    if (!(cpu_registers -> VS_Flags & VS_ZAPPER)) {
        return;
    }
    for (uint8_t port = 0; port < 2; port++) {
        if (input_port_device_active(port) != INPUT_DEVICE_ZAPPER) {
            continue;
        }
        zapper_set_enabled(port, (ext -> flags & INPUT_EXT_ZAPPER_ON(port)) != 0);
        uint16_t x = port ? ext -> x1 : ext -> x0, y = port ? ext -> y1 : ext -> y0;
        zapper_port_aim(port, x, (y >= 0xf0) ? -1 : y, (ext -> flags & INPUT_EXT_TRIGGER(port)) != 0);
    }
}

/*
 * frnes.h's frnes_rom_support: whether the loader takes the ROM, by its header (and CRC32), with the loader's own rules
 * (cartridge_rom_sizes, the game database's mapper, board_alias, board_crc_override, board_supported_submapper), and
 * whether it needs a controller FrNES doesn't emulate (its default expansion device: the NES 2.0 header's, or the Vs.
 * / Zapper database's). Reads no cartridge state and prints nothing
 */
// the default expansion devices FrNES plays with (the NES 2.0 byte 15 values): none given, standard controllers, the
// Four Score and the Famicom's four player adapter, the Vs. System's controllers and Zapper, the Zapper (one or two),
// the Arkanoid paddles; and those a game runs without (the Famicom 3D System's glasses, the Data Recorder, Turbo File,
// Battle Box, Sharp C1's cassette, a multicart's pads, swapped controllers, the reserved 6)
static int rom_device_played(uint8_t device) {
    switch (device) {
        case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07: case 0x08: case 0x09:
        case 0x0f: case 0x10: case 0x11:
        case 0x1d: case 0x20: case 0x21: case 0x22: case 0x2a: case 0x30: case 0x31:
            return 1;
    }
    return 0;
}

static const char* rom_device_name(uint8_t device) {
    switch (device) {
        case 0x0a: return "Needs a Hyper Shot (not emulated)";
        case 0x0b: case 0x0c: return "Needs a Power Pad (not emulated)";
        case 0x0d: case 0x0e: return "Needs a Family Trainer (not emulated)";
        case 0x12: return "Needs a Konami Hyper Shot (not emulated)";
        case 0x13: return "Needs a pachinko dial (not emulated)";
        case 0x14: return "Needs a punching bag (not emulated)";
        case 0x15: return "Needs a mahjong pad (not emulated)";
        case 0x16: return "Needs a Party Tap (not emulated)";
        case 0x17: return "Needs an Oeka Kids tablet (not emulated)";
        case 0x18: return "Needs a Barcode Battler (not emulated)";
        case 0x19: return "Needs a Miracle Piano (not emulated)";
        case 0x1a: return "Needs a Pokkun Moguraa (not emulated)";
        case 0x1b: return "Needs a Top Rider bike (not emulated)";
        case 0x1c: return "Needs Double-Fisted pads (not emulated)";
        case 0x1f: case 0x2e: return "Needs R.O.B. (not emulated)";
        case 0x23: case 0x24: case 0x25: case 0x26: case 0x27: case 0x28: case 0x35: case 0x36: case 0x3e: case 0x3f:
            return "Needs a keyboard (not emulated)";
        case 0x29: return "Needs an SNES mouse (not emulated)";
        case 0x2d: return "Needs a U-Force (not emulated)";
        case 0x32: return "Needs a Sudoku pad (not emulated)";
    }
    return "Needs a controller FrNES lacks";
}

int frnes_rom_support(const uint8_t* header, uint32_t file_size, uint32_t crc32, const char** reason) {
    static char text[48];
    const char* why = NULL;
    int result = FRNES_ROM_SUPPORTED;
    if ((header == NULL) || (file_size < 16) || memcmp(header, "NES\x1a", 4)) {
        why = "Not an iNES ROM";
        result = FRNES_ROM_UNSUPPORTED;
    } else {
        // (load_nes_file's: an archaic header's bytes 7-15 ignored)
        uint8_t h[16];
        memcpy(h, header, 16);
        if (((h[7] & 0x0c) == 0x04) || ((h[7] & 0x0c) == 0x0c)) {
            memset(&h[7], 0, 9);
        }
        uint32_t nes2 = (h[7] & 0x0c) == 0x08;
        uint32_t mapper = (h[6] >> 4) | (h[7] & 0xf0) | (nes2 ? ((h[8] & 0x0f) << 8) : 0);
        uint32_t submapper = nes2 ? (h[8] >> 4) : 0;
        // (an iNES header's mapper from the game database, by the ROM's CRC32 after its header: its "PRG+CHR" CRC32
        // unless the file has a trainer, which this doesn't see)
        if (!nes2 && crc32 && !(h[6] & 4)) {
            int32_t index = board_db_index(crc32);
            if (index >= 0) {
                int32_t board = board_db_board(index);
                mapper = board & 0xfff;
                submapper = (uint32_t)board >> 12;
            }
        }
        uint32_t prg, chr;
        int size_check = cartridge_rom_check(h, file_size, &prg, &chr);
        uint32_t board = board_alias(mapper), board_sub = submapper;
        if ((board == mapper) && crc32) {
            board_crc_override(crc32, &board, &board_sub);
        }
        if (size_check == CARTRIDGE_ROM_TRUNCATED) {
            why = "Truncated ROM file";
            result = FRNES_ROM_UNSUPPORTED;
        } else if (size_check == CARTRIDGE_ROM_TOO_LARGE) {
            snprintf(text, sizeof(text), "Too large: %lu kB %s (%lu kB max)", (unsigned long)((prg > MAXIMUM_ROM_SIZE) ? prg : chr) / 1024,
                (prg > MAXIMUM_ROM_SIZE) ? "PRG" : "CHR", (unsigned long)MAXIMUM_ROM_SIZE / 1024);
            why = text;
            result = FRNES_ROM_UNSUPPORTED;
        } else if (!board_supported_submapper(board, board_sub)) {
            if (board_sub) {
                snprintf(text, sizeof(text), "Mapper %lu.%lu not supported", (unsigned long)mapper, (unsigned long)board_sub);
            } else {
                snprintf(text, sizeof(text), "Mapper %lu not supported", (unsigned long)mapper);
            }
            why = text;
            result = FRNES_ROM_UNSUPPORTED;
        } else {
            uint8_t device = nes2 ? (h[15] & 0x3f) : (crc32 ? ((vs_table_lookup(crc32) >> 8) & 0x3f) : 0);
            if (!rom_device_played(device)) {
                why = rom_device_name(device);
                result = FRNES_ROM_NEEDS_DEVICE;
            }
        }
    }
    if (reason) {
        *reason = why;
    }
    return result;
}
