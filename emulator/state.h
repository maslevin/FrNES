#ifndef __STATE_H
#define __STATE_H

#include <stdint.h>
#include "memory.h"

// (state code goes after ordinary .text, where its size moves nothing that runs every frame; builds before
// FRNES_COLD_TEXT existed put it with the other cold code)
#ifndef FRNES_COLD_TEXT
#define FRNES_COLD_TEXT FRNES_COLD
#endif

/*
 * Save states: the whole emulated machine at a frame boundary, as a file of chunks.
 *
 *   header   "FRNESST1", format version, the ROM's CRC32, mapper number, submapper
 *   chunks   { char id[4]; uint16_t version; uint16_t reserved; uint32_t size; size bytes }
 *            "CPU " the CPU's timing and interrupts (cpu.c), "PPU " (ppu.c), "APU " (apu.c), "SYNT" the sound
 *            synthesis, expansion chips included (apu_synth.c), "RAM " the 2 kB RAM, the register page, nametable
 *            VRAM, OAM and palette, PRG RAM and CHR RAM (memory.c), "BORD" the board (mapper_board_state, from the
 *            mapper files)
 *
 * Saved and loaded only at the end of a frame with no DMA under way (main.c's frame loop, state_hotkey_run). A load
 * checks the game and loads the chunks in order, the board last; if one doesn't match this build's version and size,
 * the machine as it was just before is loaded back. All of it is cold code; states are on the heap only while
 * they're saved or loaded.
 */

// a state file: this header, then the chunks, each its header and its bytes
typedef struct __attribute__((packed)) {
    char magic[8];
    uint32_t format;
    uint32_t crc32;
    uint16_t mapper;
    uint8_t submapper;
    uint8_t reserved;
} state_header_t;

typedef struct __attribute__((packed)) {
    char id[4];
    uint16_t version;
    uint16_t reserved;
    uint32_t size;
} state_chunk_t;

typedef struct state_writer {
    uint8_t* data;
    uint32_t size, capacity;
    int failed;
} state_writer_t;

typedef struct state_reader {
    const uint8_t* data;
    uint32_t size, at;
} state_reader_t;

// appends bytes to the state (the writer grows; on running out of memory it fails, and the save is abandoned)
void state_put(state_writer_t* writer, const void* data, uint32_t size);
// takes bytes from a chunk; 0, or -1 if the chunk is short
int state_get(state_reader_t* reader, void* data, uint32_t size);

// a variable or array as it is
#define STATE_PUT_VAR(writer, var) state_put((writer), &(var), sizeof(var))
#define STATE_GET_VAR(reader, var) state_get((reader), &(var), sizeof(var))

/*
 * A module's chunk as a list of its variables, X(var) each, A(array) for an array: STATE_SIZE(list) is the chunk's
 * size, STATE_SAVE(list) and STATE_LOAD(list) write and read them (in functions whose writer / reader are called
 * writer / reader). By value, through a copy: a variable's address taken here would let it escape, and the compiler
 * would reload it after every call in the hot code that uses it
 */
#define STATE_X_SIZE(var) + (uint32_t)sizeof(var)
#define STATE_X_SAVE(var) { __typeof__(var) state_value = (var); state_put(writer, &state_value, sizeof(state_value)); }
#define STATE_X_LOAD(var) { __typeof__(var) state_value; state_get(reader, &state_value, sizeof(state_value)); (var) = state_value; }
#define STATE_A_SAVE(array) for (uint32_t state_i = 0; state_i < sizeof(array) / sizeof((array)[0]); state_i++) STATE_X_SAVE((array)[state_i])
#define STATE_A_LOAD(array) for (uint32_t state_i = 0; state_i < sizeof(array) / sizeof((array)[0]); state_i++) STATE_X_LOAD((array)[state_i])
#define STATE_SIZE(list) (0 list(STATE_X_SIZE, STATE_X_SIZE))
#define STATE_SAVE(list) list(STATE_X_SAVE, STATE_A_SAVE)
#define STATE_LOAD(list) list(STATE_X_LOAD, STATE_A_LOAD)

/**
 * A board's state (mapper_board_state, from the mapper files): its registers, IRQ counters, its own RAM; load
 * restores them and maps PRG / CHR / mirroring as its register writes would, checking every size and version before
 * changing anything (-1: refused, nothing changed)
 */
typedef struct {
    uint16_t version;
    void (*save)(state_writer_t* writer);
    int (*load)(state_reader_t* reader, uint16_t version);
} board_state_t;

/**
 * The loaded board's hooks, or NULL if its state can't be saved (states are refused). The mapper files define it;
 * until they do, a weak default allows the boards with no state of their own (NROM, CNROM's bank aside...) only
 */
const board_state_t* mapper_board_state(uint32_t mapper_number);

// the modules' chunks (each in its own file, cold)
void cpu_state_save(state_writer_t* writer);
int cpu_state_load(state_reader_t* reader, uint16_t version);
void ppu_state_save(state_writer_t* writer);
int ppu_state_load(state_reader_t* reader, uint16_t version);
void apu_state_save(state_writer_t* writer);
int apu_state_load(state_reader_t* reader, uint16_t version);
void apu_synth_state_save(state_writer_t* writer);
int apu_synth_state_load(state_reader_t* reader, uint16_t version);
void memory_state_save(state_writer_t* writer);
int memory_state_load(state_reader_t* reader, uint16_t version);
// (cpu_exact.c) 1 when no DMA is under way: states are only taken and loaded then (the DMA's state isn't in them)
int cpu_exact_dma_idle(void);
// (cpu_exact.c, ppu.c) for state_core.c's loads: cpu_exact_polled set, the PPU's caches invalidated
void cpu_exact_polled_set(uint8_t polled);
#define CPU_STATE_VERSION 1
#define PPU_STATE_VERSION 1
#define APU_STATE_VERSION 1
#define SYNTH_STATE_VERSION 1
#define MEMORY_STATE_VERSION 1

/**
 * Saves the machine as state slot (0-9) for the game being played, or loads it; 0, or -1 (and why on the console);
 * a load STATE_OLDER for a state of this game this build doesn't read (an older version's)
 */
#define STATE_OLDER (-2)
int state_save(int slot);
int state_load(int slot);

// the machine as a state in memory (*data malloc'd, the caller frees it), and one put back (the machine as it was if it
// doesn't fit this build or game); 0, -1, or STATE_OLDER (this game's, a chunk this build doesn't read). For states kept
// elsewhere than files (the VMU's, vmu_save.c)
int state_capture(uint8_t** data, uint32_t* size);
int state_restore(const uint8_t* data, uint32_t size);

/**
 * The save / load state hotkeys (frnes_settings.hotkeys[0]) on the first pad, each frame: STATE_HOTKEY_SAVE or
 * STATE_HOTKEY_LOAD as one is pressed (held: the hotkeys down last time, kept by the caller), else 0. The frame loop
 * runs it (state_hotkey_run) at the end of a frame with no DMA under way, with a message on the screen
 */
#define STATE_HOTKEY_SAVE 1
#define STATE_HOTKEY_LOAD 2
// the game menu's hotkey (frnes_settings.hotkeys[1]'s high nibble), and what state_hotkey_run returns when the menu
// chose to end the game
#define STATE_HOTKEY_MENU 4
#define STATE_HOTKEY_EXIT 0x80
uint8_t state_hotkeys(uint32_t buttons, int ltrig, int rtrig, uint8_t* held);
// the frame loop's held hotkeys at the game's start (after state_start_message): non-zero when a hotkey has no trigger
// (C + Z, the arcade stick's game menu), so the buttons are looked at every frame
uint8_t state_hotkeys_held_start(void);
// (the action still to do: put off to the next frame's end while a DMA is under way; STATE_HOTKEY_EXIT: end the game)
uint8_t state_hotkey_run(uint8_t action);
// the slot the hotkeys save to and load from (0-9, the game menu sets it), and a save / load of a slot where the SD
// Card page's setting says, with a message on the screen: 0, or -1
extern uint8_t state_slot;
int state_slot_save(int slot);
int state_slot_load(int slot);
// FDS games' flip disk hotkey, a FRNES_HOTKEY_* combination (FRNES_HOTKEY_RT: the right trigger held for a second,
// main.c's trigger gesture; set by the frontend before a game, kept across games); once a frame on FDS games (main.c):
// 1 when the disk is to be flipped, given the right trigger's gesture (trigger_gesture.h)
void state_set_flip_hotkey(uint8_t combination);
int state_flip_requested(uint32_t buttons, int ltrig, int rtrig, uint32_t gesture);
// the message a game starts with: how to leave it and open the game menu, under "MOVIE PLAYING" for a movie, and on
// an FDS game how to flip the disk
const char* state_start_message(int movie_playing);
// (main.c) a state for the VMU handed to the battery saves' thread, which writes it while the game goes on (data taken:
// 0 when handed over), and waiting for its write in flight
int autosave_state_write(uint8_t* data, uint32_t size, int slot);
void autosave_wait();
// (state_core.c) a replay's save / load state event at this frame (test_main.c), STATE_HOTKEY_* or 0
#include "input_recording.h"
uint8_t recording_state_action(const input_recording_t* recording, uint32_t frame);

#endif
