#ifndef __INPUT_RECORDING_H
#define __INPUT_RECORDING_H

#include <kos.h>

/**
 * FrNES 1.0.0 input recording and playback
 *
 * A recording is the controller state latched for each emulated frame, from power on. Frame N's input is the
 * state latched into the controller ports at the end of frame N - 1 (version 3: at its vblank; frame 0's is always 0),
 * the same point the player polls the Dreamcast controller, so a recording replays exactly as long as power on is
 * deterministic.
 *
 * File format (little endian, all fields 32-bit unless noted):
 *   header   magic "FRNESINP", version, header size, ROM CRC32 (PRG + CHR, without the iNES header, the same
 *            CRC FrNES uses), number of frames, number of runs, flags (bit 0: starts at power on), reserved
 *   runs     { frames, pad 1 (8 bit), pad 2 (8 bit), event (16 bit) }, frames > 0, covering frames 0 .. n - 1
 *
 * Version 2 adds events, applied at a run's first frame where the pads are latched (version 1's reserved field, 0):
 *   0x100 | n   the FDS's side n goes in by hand (fds_insert_side)
 *
 * Version 3 sets flags bit 1 (INPUT_RECORDING_FLAG_VBLANK): frame N's pads (and event) are latched at frame N - 1's
 * vblank (scanline 241) instead of its end, as Mesen 2 does and the player does since then (a game that reads its
 * controller twice a frame, before and after that point, saw a press at different frames: Summer Island's demo never
 * took it). Versions 1 and 2 keep the end of the frame.
 *
 * The pad bytes are in the order the NES controller shifts them out (A, B, Select, Start, Up, Down, Left, Right).
 *
 * Optional blocks follow the header, inside its header size, each there when its flag is set and in the order of the
 * flags' bits (readers that don't know one skip them all, to the header size):
 *   INPUT_RECORDING_FLAG_ROM_INFO   the game it was recorded on: its file name (no directory or extension, 64 bytes)
 *                                   and its path then (192 bytes), UTF-8, NUL padded (input_recording_rom_info_t)
 */

#define INPUT_RECORDING_MAGIC "FRNESINP"
#define INPUT_RECORDING_VERSION 3
#define INPUT_RECORDING_EVENT_FDS_SIDE 0x100
// save / load state slot 0 at the frame's end (the test builds' replays: test_main.c)
#define INPUT_RECORDING_EVENT_STATE_SAVE 0x200
#define INPUT_RECORDING_EVENT_STATE_LOAD 0x400
// the Vs. System's coin 1 in for the frame (mapper_boards.c, vs_recording_event)
#define INPUT_RECORDING_EVENT_VS_COIN 0x800
// the console's reset button before the frame runs (frnes_soft_reset: every reset goes through it, live or replayed)
#define INPUT_RECORDING_EVENT_RESET 0x1000
// the ROM info block (input_recording_rom_info_t) after the header
#define INPUT_RECORDING_FLAG_ROM_INFO 0x40
// version 4: the extension block (input_recording_ext_t) after the ROM info block (or the header), and runs of
// extended records: each run's 8 bytes, then its input_recording_ext_run_t (players 3 and 4, the Zappers), record_size
// bytes in all. Files without it stay version 3, and readers of version 3 or less turn version 4 down
#define INPUT_RECORDING_FLAG_EXT 0x80
#define INPUT_RECORDING_VERSION_EXT 4
#define INPUT_RECORDING_FLAG_POWER_ON 0x1
#define INPUT_RECORDING_FLAG_VBLANK 0x2
// RAM starts with FCEUX's pattern ($FF where address bit 2 is set, else $00), as BizHawk's NesHawk core powers on:
// tools/bk2_import.py sets it on imported TAS movies (input_recording_power_on_ram applies it)
#define INPUT_RECORDING_FLAG_RAM_FCEUX 0x4
// RAM starts as all $FF (older BizHawk NesHawk versions; Mesen 1's .bk2 import: "NesHawk initializes memory to 1s")
#define INPUT_RECORDING_FLAG_RAM_ONES 0x8
// the CPU / PPU alignment at power on of BizHawk's NesHawk, which changed in BizHawk 2.6.3 (PPU.cs: start_up_offset
// 2 -> 4, idleSynch false -> true): its first vblank comes 241 * 341 - start_up_offset PPU dots after power on, ours
// (PPU_POWER_ON_DOT 28, Mesen's) 241 * 341 - 27, and idleSynch is its odd frame flag. 2.6.3 and later
// (INPUT_RECORDING_FLAG_NESHAWK_ALIGN): the PPU starts at dot 5, even frame (TASVideos #4840 Castlevania reaches
// Dracula only so); earlier (INPUT_RECORDING_FLAG_NESHAWK_OLD_ALIGN): dot 3, odd frame (dot = start_up_offset + 1:
// tools/neshawk_ref shows every vblank on the same dot as NesHawk with these). tools/bk2_import.py sets one
// by the movie's emuVersion
#define INPUT_RECORDING_FLAG_NESHAWK_ALIGN 0x10
#define INPUT_RECORDING_FLAG_NESHAWK_OLD_ALIGN 0x20

typedef struct __attribute__((packed)) {
    char magic[8];
    uint32_t version;
    uint32_t header_size;
    uint32_t rom_crc32;
    uint32_t num_frames;
    uint32_t num_runs;
    uint32_t flags;
    uint32_t reserved[2];
} input_recording_header_t;

typedef struct __attribute__((packed)) {
    char name[64];
    char path[192];
} input_recording_rom_info_t;

typedef struct __attribute__((packed)) {
    uint32_t frames;
    uint8_t pad1;
    uint8_t pad2;
    uint16_t event;
} input_recording_run_t;

// the extension block (version 4): its version (1), what NES ports 1 and 2 had (input.h's INPUT_DEVICE_*), the four
// players adapter (INPUT_FOUR_SCORE_*) and each run record's size (sizeof(input_recording_run_t) +
// sizeof(input_recording_ext_run_t))
#define INPUT_RECORDING_EXT_BLOCK_VERSION 1
typedef struct __attribute__((packed)) {
    uint8_t block_version;
    uint8_t port_device[2];
    uint8_t four_score;
    uint16_t record_size;
    uint8_t reserved[10];
} input_recording_ext_t;

// a run's extension (version 4): pads 3 and 4 (the $4016 order), flags (the movie core's: the Zappers' triggers ...),
// and the Zappers' aim on NES ports 1 and 2 (x0, y0; x1, y1)
typedef struct __attribute__((packed)) {
    uint8_t pad3;
    uint8_t pad4;
    uint8_t flags;
    uint8_t reserved;
    uint16_t x0, y0, x1, y1;
} input_recording_ext_run_t;

typedef struct {
    input_recording_header_t header;
    input_recording_run_t* runs;
    uint32_t capacity;
    // playback position
    uint32_t run_index;
    uint32_t run_start_frame;
    // version 4 (INPUT_RECORDING_FLAG_EXT): each run's extension, beside runs (NULL otherwise), and the block
    input_recording_ext_run_t* ext_runs;
    input_recording_ext_t ext;
} input_recording_t;

// CRC32 (zlib / FrNES polynomial) of a buffer
uint32_t input_recording_crc32(const uint8_t* data, uint32_t length);
// the CRC32 of more data after 'crc' (0 to start): crc32_update(crc32(a), b) == crc32(a then b)
uint32_t input_recording_crc32_update(uint32_t crc, const uint8_t* data, uint32_t length);

// start an empty recording for the given ROM
void input_recording_begin(input_recording_t* recording, uint32_t rom_crc32);
// appends the input for the next frame (call once per frame, starting with frame 0)
void input_recording_add_frame(input_recording_t* recording, uint8_t pad1, uint8_t pad2);
// the same with an event at this frame (it starts a run of its own)
void input_recording_add_frame_event(input_recording_t* recording, uint8_t pad1, uint8_t pad2, uint16_t event);
// writes the recording, returns 0 on success
int input_recording_save(const input_recording_t* recording, const char* path);
// the same with the ROM info block when rom isn't NULL (INPUT_RECORDING_FLAG_ROM_INFO)
int input_recording_save_rom(const input_recording_t* recording, const input_recording_rom_info_t* rom, const char* path);
// a write to a recording's file, made in memory to be done later (malloc'd: the caller frees it; NULL when out of
// memory): 'prefix' (its path) and its NUL, then the writes. Either the whole file input_recording_save_rom writes
// (from_run 0), or only the runs from from_run on, written after the file's first from_run runs, then its header with
// the counts that take them in: written in that order, so a file cut short by a crash is still the recording as far
// as its header says (the runs before from_run must be on file as they are now: input_recording_add_frame_split)
uint8_t* input_recording_image(const input_recording_t* recording, const input_recording_rom_info_t* rom, const char* prefix,
    uint32_t from_run, uint32_t* size);
// does such a write, giving 'yield' a turn between pieces (NULL for none); 0 when it was written
int input_recording_write_image(const uint8_t* image, uint32_t size, void (*yield)(void));
// input_recording_add_frame_event without adding the frame to the last run (the runs saved so far stay as on file)
void input_recording_add_frame_split(input_recording_t* recording, uint8_t pad1, uint8_t pad2, uint16_t event);
// version 4: a recording begun as input_recording_begin's with the extension block (its record_size set here), and
// frames with their extension (split as input_recording_add_frame_split: 1 for a new run whatever the last)
void input_recording_begin_ext(input_recording_t* recording, uint32_t rom_crc32, const input_recording_ext_t* ext);
void input_recording_add_frame_ext(input_recording_t* recording, uint8_t pad1, uint8_t pad2, uint16_t event,
    const input_recording_ext_run_t* ext, int split);
// playing: the extension of the run input_recording_frame gave the frame (zeros past the end or for version 3)
void input_recording_ext_frame(const input_recording_t* recording, input_recording_ext_run_t* ext);
// a recording file's extension block (the browser's, a movie's settings), 0 when it has one
int input_recording_ext_info(const char* path, input_recording_ext_t* ext);
// a recording's ROM info block (the browser's: which game it's for), 0 when it has one
int input_recording_rom_info(const char* path, input_recording_rom_info_t* rom);
// ROM info: a name (NULL: the path's file name without its directory or extension) and a path (NULL for none)
void input_recording_rom_info_set(input_recording_rom_info_t* rom, const char* name, const char* path);

// loads and validates a recording, returns 0 on success. A ROM CRC mismatch is an error unless expected_crc32 is 0
int input_recording_load(input_recording_t* recording, const char* path, uint32_t expected_crc32);
// the power on RAM contents a recording asks for (INPUT_RECORDING_FLAG_RAM_FCEUX), into the 2 kB at 'ram'
void input_recording_power_on_ram(const input_recording_t* recording, uint8_t* ram);
// all of a recording's power on state (RAM, and the PPU's start dot with INPUT_RECORDING_FLAG_NESHAWK_ALIGN): after
// the PPU's initialization, before the first frame
void input_recording_power_on(const input_recording_t* recording, uint8_t* ram);
// the input for a frame during playback (frames must be asked for in increasing order), returns 0 past the end
int input_recording_frame(input_recording_t* recording, uint32_t frame, uint8_t* pad1, uint8_t* pad2);
// the event at that frame (after input_recording_frame for it), 0 for none
uint16_t input_recording_event(const input_recording_t* recording, uint32_t frame);

void input_recording_free(input_recording_t* recording);

#endif
