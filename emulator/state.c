#include <kos.h>
#include <stdlib.h>
#include <string.h>

#include "state.h"
#include "fds.h"
#include "cartridge.h"
#include "memory.h"

/*
 * Save states (state.h): the file's framing, where states are kept, and the order modules are saved and loaded in.
 * Everything here is cold: it runs only between frames, when a hotkey asks for it.
 */

#define STATE_MAGIC "FRNESST1"
#define STATE_FORMAT 1

// where states go: a build's own directory (FRNES_STATE_DIRECTORY, e.g. on /pc), else the SD card's FrNES folder
// (no static storage for states in memory: new .bss would move the heap's start, which perf pins)
#ifndef FRNES_STATE_DIRECTORY
#define FRNES_STATE_DIRECTORY "/sd/FrNES/states"
#endif
#define STATE_SLOTS 10

FRNES_COLD_TEXT void state_put(state_writer_t* writer, const void* data, uint32_t size) {
    if (writer -> failed) {
        return;
    }
    if (writer -> size + size > writer -> capacity) {
        uint32_t capacity = writer -> capacity ? writer -> capacity : 64 * 1024;
        while (capacity < writer -> size + size) {
            capacity *= 2;
        }
        uint8_t* grown = realloc(writer -> data, capacity);
        if (grown == NULL) {
            writer -> failed = 1;
            return;
        }
        writer -> data = grown;
        writer -> capacity = capacity;
    }
    memcpy(writer -> data + writer -> size, data, size);
    writer -> size += size;
}

FRNES_COLD_TEXT int state_get(state_reader_t* reader, void* data, uint32_t size) {
    if (reader -> at + size > reader -> size) {
        return -1;
    }
    memcpy(data, reader -> data + reader -> at, size);
    reader -> at += size;
    return 0;
}

// (a board with no state of its own: the boards whose registers don't change the machine past power on)
__attribute__((weak)) const board_state_t* mapper_board_state(uint32_t mapper_number) {
    static const board_state_t none = { 1, NULL, NULL };
    return (mapper_number == 0) ? &none : NULL;
}

// a chunk: its header, then what save writes, its size filled in after
FRNES_COLD_TEXT static void chunk(state_writer_t* writer, const char* id, uint16_t version, void (*save)(state_writer_t*)) {
    state_chunk_t header;
    memcpy(header.id, id, 4);
    header.version = version;
    header.reserved = 0;
    header.size = 0;
    uint32_t at = writer -> size;
    state_put(writer, &header, sizeof(header));
    if (save != NULL) {
        save(writer);
    }
    if (!writer -> failed) {
        ((state_chunk_t*)(writer -> data + at)) -> size = writer -> size - at - sizeof(header);
    }
}

/*
 * The operand cache around a save or a load: the machine's memory is written through the NES windows (the MMU's aliases
 * of RAM and its mirrors, PRG RAM, the PRG windows that can show RAM, CHR RAM, the nametables, OAM and the palette, the
 * register page) and read and written here through its buffers (their P1 addresses), and the cache is indexed by
 * virtual address, so the same memory can have a line under each. Purged, so a save reads what the game wrote and a
 * load leaves no stale line, in either alias, to be read or written back over it. By range, each call under KOS's
 * 39936 byte threshold: dcache_purge_all() is a 16 kB .bss buffer (KOS's inline arch_dcache_purge_all)
 */
#define STATE_PURGE_STEP 0x8000
FRNES_COLD_TEXT static void state_purge(uintptr_t start, uint32_t size) {
    for (uint32_t at = 0; at < size; at += STATE_PURGE_STEP) {
        dcache_purge_range(start + at, ((size - at) < STATE_PURGE_STEP) ? (size - at) : STATE_PURGE_STEP);
    }
}

FRNES_COLD_TEXT static void state_purge_windows(void) {
    // (the mapped pages only: $2000-$5FFF isn't, and $6000-$7FFF only with PRG RAM)
    state_purge(RAM_ADDR, 0x2000);
    if (WRAM != NULL) {
        state_purge(WRAM_ADDR, 0x2000);
    }
    state_purge(ROMBANK0_ADDR, 0x8000);
    state_purge(PPU_WINDOW_BASE, 0x4000);
    state_purge(SPRRAM_ADDR, 0x400);
    state_purge(REGISTERS_ADDR, 0x400);
}

FRNES_COLD_TEXT static void state_purge_buffers(void) {
    state_purge((uintptr_t)RAM, 0x800);
    state_purge((uintptr_t)register_buffer, 0x400);
    state_purge((uintptr_t)VRAM, 0x3000);
    state_purge((uintptr_t)ppu_memory_buffer, 0x400);
    if (WRAM != NULL) {
        state_purge((uintptr_t)WRAM, WRAM_size);
    }
    if (cartridge_VRAM != NULL) {
        state_purge((uintptr_t)cartridge_VRAM, num_VRAM_bytes_allocated);
    }
}

// the whole machine into a writer; 0, or -1 if the board can't be saved or memory ran out
FRNES_COLD_TEXT static int state_build(state_writer_t* writer) {
    const board_state_t* board = mapper_board_state(cartridge_mapper_number);
    if (board == NULL) {
        printf("STATE: mapper %lu's state can't be saved yet\n", (unsigned long)cartridge_mapper_number);
        return -1;
    }
    state_header_t header;
    memcpy(header.magic, STATE_MAGIC, 8);
    header.format = STATE_FORMAT;
    header.crc32 = currentCRC32;
    header.mapper = (uint16_t)cartridge_mapper_number;
    header.submapper = (uint8_t)cartridge_submapper;
    header.reserved = 0;
    state_put(writer, &header, sizeof(header));
    // (what the game wrote through the windows into memory, and no stale buffer line read)
    state_purge_windows();
    state_purge_buffers();
    chunk(writer, "CPU ", CPU_STATE_VERSION, cpu_state_save);
    chunk(writer, "PPU ", PPU_STATE_VERSION, ppu_state_save);
    chunk(writer, "APU ", APU_STATE_VERSION, apu_state_save);
    chunk(writer, "SYNT", SYNTH_STATE_VERSION, apu_synth_state_save);
    chunk(writer, "RAM ", MEMORY_STATE_VERSION, memory_state_save);
    chunk(writer, "BORD", board -> version, board -> save);
    // (an FDS game's disks as it wrote them: its own chunk, fds.c's)
    fds_disk_state_chunk(writer);
    return writer -> failed ? -1 : 0;
}

// a chunk of a state by its id; 0, or -1 if it's missing
FRNES_COLD_TEXT static int state_find(const uint8_t* data, uint32_t size, const char* id, state_reader_t* reader, uint16_t* version) {
    uint32_t at = sizeof(state_header_t);
    while (at + sizeof(state_chunk_t) <= size) {
        const state_chunk_t* chunk = (const state_chunk_t*)(data + at);
        uint32_t end = at + sizeof(state_chunk_t) + chunk -> size;
        if (end > size) {
            return -1;
        }
        if (memcmp(chunk -> id, id, 4) == 0) {
            reader -> data = data + at + sizeof(state_chunk_t);
            reader -> size = chunk -> size;
            reader -> at = 0;
            *version = chunk -> version;
            return 0;
        }
        at = end;
    }
    return -1;
}

/*
 * Applies a state: every chunk, in order, the board last. A module refuses a chunk whose version or size isn't its
 * own; if any does, the machine as it was (rollback, saved just before) is put back
 */
FRNES_COLD_TEXT static int state_apply(const uint8_t* data, uint32_t size, const uint8_t* rollback, uint32_t rollback_size, int is_rollback) {
    const state_header_t* header = (const state_header_t*)data;
    if ((size < sizeof(*header)) || (memcmp(header -> magic, STATE_MAGIC, 8) != 0)) {
        printf("STATE: not a state\n");
        return -1;
    }
    if (header -> format != STATE_FORMAT) {
        printf("STATE: not a state this build reads\n");
        return STATE_OLDER;
    }
    if ((header -> crc32 != currentCRC32) || (header -> mapper != cartridge_mapper_number)) {
        printf("STATE: from another game (CRC32 %08lX, mapper %u)\n", (unsigned long)header -> crc32, header -> mapper);
        return -1;
    }
    const board_state_t* board = mapper_board_state(cartridge_mapper_number);
    // (from here on the state is this game's: one this build doesn't read is an older version's)
    int result = STATE_OLDER;
    // (the game's dirty window lines written back now, not over the state later)
    state_purge_windows();
    state_purge_buffers();
    static const char* const ids[] = { "CPU ", "PPU ", "APU ", "SYNT", "RAM ", "BORD" };
    int (*const loads[])(state_reader_t*, uint16_t) = { cpu_state_load, ppu_state_load, apu_state_load, apu_synth_state_load,
        memory_state_load, board ? board -> load : NULL };
    for (int i = 0; i < 6; i++) {
        state_reader_t reader;
        uint16_t version;
        if (state_find(data, size, ids[i], &reader, &version) != 0) {
            printf("STATE: no %.4s chunk\n", ids[i]);
            goto refused;
        }
        if ((loads[i] != NULL) && (loads[i](&reader, version) != 0)) {
            printf("STATE: the %.4s chunk (version %u, %lu bytes) doesn't fit this build\n", ids[i], version, (unsigned long)reader.size);
            goto refused;
        }
    }
    // (an FDS game's disks: optional, a state without them leaves the disks as they are; fds.c)
    if (fds_disk_state_apply(data, size) != 0) {
        goto refused;
    }
    // (the state was written through the buffers' addresses, and the boards' loads may have written through the windows)
    state_purge_windows();
    state_purge_buffers();
    return 0;
refused:
    if (!is_rollback && (rollback != NULL)) {
        state_apply(rollback, rollback_size, NULL, 0, 1);
    }
    return result;
}

FRNES_COLD_TEXT static void state_path(char* path, size_t size, int slot) {
    snprintf(path, size, "%s/%08lX_%d.fst", FRNES_STATE_DIRECTORY, (unsigned long)currentCRC32, slot);
}

FRNES_COLD_TEXT int state_capture(uint8_t** data, uint32_t* size) {
    state_writer_t writer = { NULL, 0, 0, 0 };
    if (state_build(&writer) != 0) {
        free(writer.data);
        return -1;
    }
    *data = writer.data;
    *size = writer.size;
    return 0;
}

FRNES_COLD_TEXT int state_restore(const uint8_t* data, uint32_t size) {
    // (the machine as it is, to put back if the state doesn't load)
    state_writer_t rollback = { NULL, 0, 0, 0 };
    if (state_build(&rollback) != 0) {
        free(rollback.data);
        return -1;
    }
    int result = state_apply(data, size, rollback.data, rollback.size, 0);
    free(rollback.data);
    return result;
}

FRNES_COLD_TEXT int state_save(int slot) {
    if ((slot < 0) || (slot >= STATE_SLOTS)) {
        return -1;
    }
    state_writer_t writer = { NULL, 0, 0, 0 };
    if (state_capture(&writer.data, &writer.size) != 0) {
        return -1;
    }
    char path[128];
    state_path(path, sizeof(path), slot);
    fs_mkdir(FRNES_STATE_DIRECTORY);
    file_t file = fs_open(path, O_WRONLY | O_TRUNC | O_CREAT);
    if ((file != -1) && (fs_write(file, writer.data, writer.size) == (ssize_t)writer.size)) {
        fs_close(file);
        printf("STATE: saved slot %d (%lu bytes) to %s\n", slot, (unsigned long)writer.size, path);
        free(writer.data);
        return 0;
    }
    if (file != -1) {
        fs_close(file);
    }
    printf("STATE: couldn't write slot %d to %s\n", slot, path);
    free(writer.data);
    return -1;
}

FRNES_COLD_TEXT int state_load(int slot) {
    if ((slot < 0) || (slot >= STATE_SLOTS)) {
        return -1;
    }
    uint8_t* data = NULL;
    uint32_t size = 0;
    char path[128];
    state_path(path, sizeof(path), slot);
    file_t file = fs_open(path, O_RDONLY);
    if (file != -1) {
        size = fs_total(file);
        data = malloc(size);
        if ((data == NULL) || (fs_read(file, data, size) != (ssize_t)size)) {
            free(data);
            data = NULL;
        }
        fs_close(file);
    }
    const uint8_t* state = data;
    uint32_t state_size = size;
    if (state == NULL) {
        printf("STATE: no slot %d for this game\n", slot);
        return -1;
    }
    int result = state_restore(state, state_size);
    printf("STATE: %s slot %d\n", (result == 0) ? "loaded" : "couldn't load", slot);
    free(data);
    return result;
}


