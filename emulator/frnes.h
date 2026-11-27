#ifndef __FRNES_H
#define __FRNES_H

#include <stdint.h>

/*
 * The emulator as the frontend sees it: settings, and frnes_run to play a game with them.
 */

// how the 256x240 picture fills the 640x480 screen
#define FRNES_PICTURE_FULL 0        // the whole screen (pixels 2.5 wide, 2 high)
#define FRNES_PICTURE_2X 1          // exactly 2x (512x480, centred: square pixels)
#define FRNES_PICTURE_8_7 2         // the NES's own pixel aspect, 8:7 (585x480, centred)
#define FRNES_PICTURE_MODES 3

// what paces the emulation (video.h's video_pacing, the same values)
#define FRNES_PACING_SOUND 0        // the NES's own rate, held by the sound buffer (a frame isn't shown about every 6 s)
#define FRNES_PACING_VBLANK 1       // one frame per display refresh (vsync), the sound resampled to match; PAL video
                                    // always paces to the sound
#define FRNES_PACING_MODES 2

// one Dreamcast controller's mapping onto an NES pad: each NES button is pressed by any of the CONT_* buttons in its
// mask; the left trigger can be Select too (past half way, and not while the right trigger is pressed)
typedef struct {
    uint32_t a_buttons;
    uint32_t b_buttons;
    uint32_t select_buttons;
    uint8_t select_on_left_trigger;
    // the analog stick moves the D-pad as well
    uint8_t analog_dpad;
    // turbo: the Dreamcast button (FRNES_TURBO_* below) that presses NES A over and over while it's held (low four
    // bits), and NES B's (high four); 0 for none
    uint8_t turbo_buttons;
    // how fast: 0 30 presses a second, 1 20, 2 15, 3 10 (low two bits); the Dreamcast button for NES Start in the high
    // four, FRNES_START_* (0, as frnes_default_settings and every mapping before it have: Start)
    uint8_t turbo_rate;
} frnes_pad_mapping_t;

// a turbo button (frnes_pad_mapping_t.turbo_buttons)
#define FRNES_TURBO_NONE 0
#define FRNES_TURBO_A 1
#define FRNES_TURBO_B 2
#define FRNES_TURBO_X 3
#define FRNES_TURBO_Y 4
#define FRNES_TURBO_C 5
#define FRNES_TURBO_Z 6
#define FRNES_TURBO_RATES 4
// NES Start's Dreamcast button (frnes_pad_mapping_t.turbo_rate's high four bits)
#define FRNES_START_START 0
#define FRNES_START_A 1
#define FRNES_START_B 2
#define FRNES_START_X 3
#define FRNES_START_Y 4
#define FRNES_START_C 5
#define FRNES_START_Z 6
#define FRNES_START_NONE 7
#define FRNES_START_X_OR_B 8
#define FRNES_START_A_OR_C 9
#define FRNES_START_SHIFT 4

// the hotkeys' button combinations (the menus' list, in this order)
#define FRNES_HOTKEY_OFF 0
#define FRNES_HOTKEY_RT 1               // the right trigger held alone
#define FRNES_HOTKEY_RT_A 2             // RT + A, B, X, Y
#define FRNES_HOTKEY_LT_RT 6
#define FRNES_HOTKEY_LT_A 7             // LT + A, B, X, Y
#define FRNES_HOTKEY_C_Z 11             // C + Z (the arcade stick's: it has no triggers)
#define FRNES_HOTKEY_COUNT 12

// where save states are kept: the state directory (FRNES_STATE_DIRECTORY, the SD card's), the VMU (compressed with
// bzip2), or automatic: the directory, the VMU if it can't be written (no SD card) or has no state for the slot
#define FRNES_STATE_AUTO 0
#define FRNES_STATE_SD 1
#define FRNES_STATE_VMU 2
// no save states (the hotkeys and the game menu say so)
#define FRNES_STATE_OFF 3
#define FRNES_STATE_DEVICES 4

// where battery saves go (battery_set_place, main.c): the VMU (and SAVE_DIRECTORY's file, dev setups' /pc, at a
// game's end), the SD card's FrNES/saves, or nowhere (not loaded either: every game starts fresh)
#define FRNES_BATTERY_VMU 0
#define FRNES_BATTERY_SD 1
#define FRNES_BATTERY_OFF 2
#define FRNES_BATTERY_PLACES 3
#define FRNES_BATTERY_SD_DIRECTORY "/sd/FrNES/saves"
void battery_set_place(uint8_t place);

// NES pads read from Dreamcast controllers 1 and 2
#define FRNES_PADS 4

typedef struct {
    // NES pixels hidden at each edge (0-64 left / right, 0-60 top / bottom): they show as black, the rest of the
    // picture keeps its size and place
    uint8_t crop_left, crop_right, crop_top, crop_bottom;
    uint8_t picture;            // FRNES_PICTURE_*
    uint8_t bilinear;           // smooth the picture (the PVR's bilinear filter)
    uint8_t pacing;             // FRNES_PACING_* (the default: the build's, FRNES_DEFAULT_PACING in video.h)
    uint8_t frameskip;          // reserved: frames emulated but not shown in each run of frameskip + 1 (0 for now)
    uint8_t hud_mode;           // the frame display at start: 0 off, 1 timing + operand cache misses, 2 + I-cache
    uint8_t palette_set;        // 1: the NES colors are frnes_palette()'s, 0: the emulator's own
    // the hotkeys, FRNES_HOTKEY_* combinations: save state (low nibble) and load state (high nibble) of hotkeys[0]; [1]
    // reserved for the frame display and the game menu
    uint8_t hotkeys[2];
    // where save states go: FRNES_STATE_* (the struct's padding before pads: its size doesn't change)
    uint8_t state_device;
    // the Zapper's reticle speed, FRNES_ZAPPER_SPEED_MIN-MAX (speed x 0.8 px a frame with the D-pad or the stick all
    // the way over), 0 (options saved before it) as the default (also the struct's padding)
    uint8_t zapper_speed;
    // NES port 2's device: FRNES_PORT2_* (the last byte of padding before pads)
    uint8_t port2_device;
    frnes_pad_mapping_t pads[FRNES_PADS];
} frnes_settings_t;

#define FRNES_ZAPPER_SPEED_MIN 1
#define FRNES_ZAPPER_SPEED_MAX 10
#define FRNES_ZAPPER_SPEED_DEFAULT 5
// what NES port 2 has: the game's (its header, or the game database), a controller, or the Zapper (NES games only)
#define FRNES_PORT2_AUTO 0
#define FRNES_PORT2_CONTROLLER 1
#define FRNES_PORT2_ZAPPER 2
#define FRNES_PORT2_MODES 3

#define FRNES_CROP_MAX_X 64
#define FRNES_CROP_MAX_Y 60

/**
 * The settings frnes_run plays with: frnes_default_settings() until the frontend writes its options here before a
 * game (player builds keep the defaults)
 */
extern frnes_settings_t frnes_settings;
void frnes_default_settings(frnes_settings_t* settings);

/**
 * The 64 NES colors (RGB565) when frnes_settings.palette_set: a .pal file's, chosen in the menus. Defined by whatever
 * sets palette_set (player_main.c from the handoff block, the menus in a one-binary build), read at video_init
 */
const uint16_t* frnes_palette(void);

/**
 * The Game Genie codes to play the game with (FRNES_CHEATS of them, 6 or 8 letters, "" for none), or NULL: defined
 * beside frnes_palette(), read once the ROM is loaded (they're applied to the ROM then)
 */
#define FRNES_CHEATS 3
const char (*frnes_cheats(void))[9];

/**
 * Plays a ROM (.nes, or .fds with disksys.rom beside it) until the player exits (A+B+X+Y+Start), then shuts the
 * emulator down and returns; it can be called again for the next game. The PVR is the emulator's while it runs
 * (video_init starts it, video_shutdown stops it): a caller that draws with the PVR shuts it down first and starts it
 * again after.
 * Returns 0, or -1 if the ROM couldn't be loaded.
 */
int frnes_run(const char* rom_path);
// the same with a movie (movie.h: .frm) playing in place of the pads (movie_path), or the pads recorded into a new movie
// in record_directory (both NULL: neither), from power on
int frnes_run_movie(const char* rom_path, const char* movie_path, const char* record_directory);

/*
 * Whether FrNES can play a ROM, by its iNES / NES 2.0 header, before any game loads (the browser's status bar): the
 * loader's own rules (mapper_boards.c). header: the file's first 16 bytes; file_size: the whole file's; crc32: the
 * CRC32 of the file after its header (the game database's mapper for an iNES header, the board overrides, the Vs. /
 * Zapper database's controller), 0 if not known (the header alone). *reason (when reason isn't NULL): a static string
 * of at most 40 characters saying why for FRNES_ROM_NEEDS_DEVICE and FRNES_ROM_UNSUPPORTED, NULL for
 * FRNES_ROM_SUPPORTED
 */
#define FRNES_ROM_SUPPORTED 0
#define FRNES_ROM_NEEDS_DEVICE 1    // it runs, but its controller (expansion device) isn't emulated: Hyper Shot...
#define FRNES_ROM_UNSUPPORTED 2
int frnes_rom_support(const uint8_t* header, uint32_t file_size, uint32_t crc32, const char** reason);

#endif
