#include <kos.h>
#include <stdlib.h>
#include <string.h>

#include "gamegenie.h"
#include "cartridge.h"
#include "memory.h"
#include "state.h"
/*
 * Game Genie codes, applied to the ROM once it's loaded (after its CRC32 is taken) and mapped, so reads cost nothing.
 * The Genie answers reads of its address with its value (an 8-letter code only when the read would have given its
 * compare value), whatever bank the game has there. So the code is written into every PRG byte that can appear at its
 * address: in a window the board banks in steps of W bytes, every p with p % W == the address's offset % W (and, for
 * an 8-letter code, ROM[p] == compare); in a window the board never banks, the one byte mapped there (written through
 * the CPU's window). (Differs from the hardware only where a game maps one bank at two windows.)
 *
 * Every byte changed is kept with what it was (undo), so the codes can be changed while the game runs: the old ones
 * come off, last first, and the new ones go on.
 */
static const char gamegenie_letters[17] = "APZLGITYEOXUKSVN";

FRNES_COLD_TEXT static int gamegenie_decode(const char* code, uint32_t* address, uint8_t* value, int* compare) {
    size_t length = strlen(code);
    int n[8];
    if ((length != 6) && (length != 8)) {
        return -1;
    }
    for (size_t i = 0; i < length; i++) {
        const char* letter = strchr(gamegenie_letters, code[i] & ~0x20);
        if ((letter == NULL) || (code[i] == '\0')) {
            return -1;
        }
        n[i] = letter - gamegenie_letters;
    }
    *address = 0x8000 | ((n[3] & 7) << 12) | ((n[5] & 7) << 8) | ((n[4] & 8) << 8) | ((n[2] & 7) << 4) | ((n[1] & 8) << 4) |
        (n[4] & 7) | (n[3] & 8);
    *value = ((n[1] & 7) << 4) | ((n[0] & 8) << 4) | (n[0] & 7) | (((length == 6) ? n[5] : n[7]) & 8);
    *compare = (length == 6) ? -1 : (((n[7] & 7) << 4) | ((n[6] & 8) << 4) | (n[6] & 7) | (n[5] & 8));
    return 0;
}

/*
 * The step the board banks the PRG window holding address in (bytes), or 0 if that window is fixed: NROM and the
 * CHR-only boards fixed; UxROM's 16 kB at $8000 (its last bank fixed at $C000; UNROM 180 the other way round); MMC1,
 * Bandai, Sunsoft-4 16 kB; the 32 kB boards (AxROM, Color Dreams, BNROM / NINA-001, GxROM, Quattro); the rest (MMC3,
 * MMC5, the VRCs, FME-7, Namco, Taito...) 8 kB
 */
FRNES_COLD_TEXT static uint32_t gamegenie_window_step(uint32_t mapper, uint32_t address) {
    switch (mapper) {
        case 0: case 3: case 13: case 87: case 185:
            return 0;
        case 2: case 71: case 30:
            return (address < 0xc000) ? 0x4000 : 0;
        case 180:
            return (address < 0xc000) ? 0 : 0x4000;
        case 1: case 16: case 68: case 153: case 159:
            return 0x4000;
        case 7: case 11: case 34: case 66: case 232:
            return 0x8000;
        default:
            return 0x2000;
    }
}

typedef struct {
    volatile uint8_t* at;
    uint8_t was;
} gamegenie_patch_t;
struct gamegenie_undo {
    uint32_t count, capacity;
    gamegenie_patch_t patches[];
};

FRNES_COLD_TEXT static void gamegenie_patch(gamegenie_undo_t** undo_list, volatile uint8_t* at, uint8_t value) {
    gamegenie_undo_t* undo = *undo_list;
    if ((undo == NULL) || (undo -> count == undo -> capacity)) {
        uint32_t capacity = undo ? undo -> capacity * 2 : 256;
        gamegenie_undo_t* grown = realloc(undo, sizeof(gamegenie_undo_t) + capacity * sizeof(gamegenie_patch_t));
        if (grown == NULL) {
            // (out of memory: not changed, as it couldn't be put back)
            return;
        }
        if (undo == NULL) {
            grown -> count = 0;
        }
        grown -> capacity = capacity;
        undo = *undo_list = grown;
    }
    undo -> patches[undo -> count].at = at;
    undo -> patches[undo -> count].was = *at;
    undo -> count++;
    *at = value;
}

FRNES_COLD_TEXT void gamegenie_apply(const char (*codes)[9], gamegenie_undo_t** undo_list) {
    if ((cartridge_mapper_number == 20) || (ROM == NULL)) {
        return;
    }
    // the codes on before come off, the last byte changed first
    gamegenie_undo_t* undo = *undo_list;
    if (undo != NULL) {
        while (undo -> count) {
            undo -> count--;
            *undo -> patches[undo -> count].at = undo -> patches[undo -> count].was;
        }
    }
    uint32_t rom_bytes = num_8k_ROM_pages * 0x2000;
    for (int i = 0; (codes != NULL) && (i < FRNES_CHEATS); i++) {
        uint32_t address;
        uint8_t value;
        int compare;
        if (!codes[i][0] || (gamegenie_decode(codes[i], &address, &value, &compare) != 0)) {
            continue;
        }
        uint32_t step = gamegenie_window_step(cartridge_mapper_number, address);
        uint32_t patched = 0;
        if ((step == 0) || (step > rom_bytes)) {
            volatile uint8_t* mapped = (volatile uint8_t*)(ROMBANK0_ADDR + (address - 0x8000));
            if ((compare < 0) || (*mapped == compare)) {
                gamegenie_patch(undo_list, mapped, value);
                patched = 1;
            }
        } else {
            for (uint32_t p = (address - 0x8000) % step; p < rom_bytes; p += step) {
                if ((compare < 0) || (ROM[p] == compare)) {
                    gamegenie_patch(undo_list, &ROM[p], value);
                    patched++;
                }
            }
        }
        printf("GAME GENIE %s: $%04lX = $%02X%s, %lu byte%s\n", codes[i], (unsigned long)address, value,
            (compare < 0) ? "" : " (where it was the compare value)", (unsigned long)patched, (patched == 1) ? "" : "s");
    }
    // (the ROM pages are read through the CPU window's virtual addresses as well as written through ROM's; by range, each
    // call under KOS's whole-cache threshold: dcache_purge_all() brings in a 16 kB .bss buffer)
    for (uint32_t at = 0; at < rom_bytes; at += 0x8000) {
        dcache_purge_range((uintptr_t)ROM + at, ((rom_bytes - at) < 0x8000) ? (rom_bytes - at) : 0x8000);
    }
    dcache_purge_range(ROMBANK0_ADDR, 0x8000);
}

FRNES_COLD_TEXT void gamegenie_free(gamegenie_undo_t** undo_list) {
    free(*undo_list);
    *undo_list = NULL;
}

gamegenie_undo_t* gamegenie_undo FRNES_COLD_BSS;
