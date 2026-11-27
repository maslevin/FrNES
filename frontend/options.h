#ifndef _OPTIONS_H
#define _OPTIONS_H

#include <kos.h>
#include "../emulator/frnes.h"

/*
 * The user's options, kept on the VMU as FrNES.opt: the menus' colors, the emulator's settings (frnes.h) and where
 * the ROM browser was. The file starts with a header (magic, version, size); a file from another version is ignored
 * and the defaults used, so OPTIONS_VERSION goes up whenever Options_t changes.
 */
#define OPTIONS_VERSION 7
#define OPTIONS_PATH_LENGTH 128

// the Dreamcast controller in one port (A-D) as an NES controller (1-4): the buttons (CONT_* masks, 0 for none) that
// press each NES button. Ports A and B's A / B / Select and the two flags are the emulator's (options_sync_pads puts
// them in emulator.pads); Start, turbo and ports C and D are kept for when the emulator takes them
typedef struct {
    uint32 a, b, select, start, turbo_a, turbo_b;
    uint8 turbo_rate;           // OPTIONS_TURBO_RATES' index
    uint8 select_on_left_trigger;
    uint8 analog_dpad;
    // where it plugs in and what it is (options_port_nes / options_port_device): bits 0-2 the NES port 1-4, 5 none, 0
    // its own (A on 1 ... D on 4, as options saved before it have); bits 3-4 OPTIONS_DEVICE_*
    uint8 plug;
} Port_Options;

#define OPTIONS_NES_NONE 4
#define OPTIONS_DEVICE_AUTO 0
#define OPTIONS_DEVICE_PAD 1
#define OPTIONS_DEVICE_ZAPPER 2
// (NES port 2: the Arkanoid paddle)
#define OPTIONS_DEVICE_PADDLE 3

#define OPTIONS_PORTS 4

// the hotkeys (OPTIONS_HOTKEY_* combinations), kept for when the emulator takes them
typedef enum {
    HOTKEY_SAVE_STATE,
    HOTKEY_LOAD_STATE,
    HOTKEY_FRAME_DISPLAY,
    HOTKEY_GAME_MENU,
    HOTKEY_COUNT
} Hotkey_Id;

typedef struct Options_s {
    uint32 GUI_BGColor;
    uint32 GUI_TextColor;
    uint32 GUI_SelectedTextColor;
    uint32 GUI_InsideWindowColor;
    uint32 GUI_OutsideWindowColor;
    frnes_settings_t emulator;
    // the ROM browser's directory and highlighted row when it was last left
    char browser_directory[OPTIONS_PATH_LENGTH];
    int32 browser_index;
    // the NES palette's name (a .pal file in /rd/palettes, without the .pal; "" for the emulator's own), and its
    // colors (RGB565) for the emulator (frnes_palette(), emulator.palette_set)
    char palette_name[32];
    uint16 palette[64];
    Port_Options ports[OPTIONS_PORTS];
    uint8 hotkeys[HOTKEY_COUNT];
    // where saves go, kept for when the emulator takes them: the VMU for battery saves and for save states (0: the
    // first with room, else 1 + port * 2 + unit - 1: A1 is 1, A2 2, B1 3 ...), and what goes on the SD card instead
    uint8 battery_vmu;
    uint8 state_vmu;
    // (0 a VMU, 1 the SD card, 2 off)
    uint8 battery_on_sd;
    uint8 states_on_sd;
    uint8 movies_on_sd;
    // FDS games' flip disk hotkey, FRNES_HOTKEY_* (0, as options saved before it have: the right trigger held)
    uint8 flip_disk_hotkey;
    // four players: INPUT_FOUR_SCORE_* (input.h: 0 the game's)
    uint8 four_score;
    // the sound: 0 on, 1 off (no sound emulated, the frames paced by the timer, as FrNES 0.x could), in what was the
    // reserved byte (0, as files saved before it have: on)
    uint8 sound_off;
    // the Game Genie codes (cheats.h) of the game last set up: its path, and each slot's code, what it does, on or off
    char cheat_rom[96];
    char cheat_codes[3][9];
    char cheat_texts[3][41];
    uint8 cheat_on[3];
    // (version 7) the reticle over the picture for a light gun (0 shown, 1 hidden; a pad's Zapper always has it)
    uint8 gun_hide_reticle;
    // the sound's output filter (APU_SYNTH_FILTER_*: off, NES, Famicom) and quality (APU_SYNTH_QUALITY_*: standard,
    // high), in what was version 7's padding (0, as files saved before them have: off and standard, the old sound)
    uint8 sound_filter;
    uint8 sound_quality;
    // (version 7) the light gun's calibration (the Controls page's light gun rows): the screen x of the gun's H counter
    // is (H - the mode's bitmapx) * gun_x_scale / 1000 + gun_x_offset, y (V - bitmapy, doubled when interlaced) *
    // gun_y_scale / 1000 + gun_y_offset; gun_x_scale 0 (as version 6's files are moved): the emulator's defaults
    int16 gun_x_scale, gun_x_offset, gun_y_scale, gun_y_offset;
} Options_t;

// version 6's options: these, without the light gun's calibration
#define OPTIONS_V6_SIZE 0x300
// the light gun's defaults (mapper_boards.c's, BLUE's gun on a composite TV)
#define OPTIONS_GUN_X_SCALE 1000
#define OPTIONS_GUN_X_OFFSET (-125)
#define OPTIONS_GUN_Y_SCALE 1000
#define OPTIONS_GUN_Y_OFFSET 0
// the light gun's calibration as it stands (the defaults until it's set), and setting it
void options_gun_calibration(int16* x_scale, int16* x_offset, int16* y_scale, int16* y_offset);
void options_set_gun_calibration(int16 x_scale, int16 x_offset, int16 y_scale, int16 y_offset);

extern Options_t options;

void options_set_defaults();
// a port's defaults (the emulator's player mapping, Start on Start, no turbo)
void options_default_port(int port);
// ports A and B's buttons into the emulator's pads
void options_sync_pads();
// a Dreamcast port's NES port (0-3, OPTIONS_NES_NONE) and device (OPTIONS_DEVICE_*), and setting them (a port another
// Dreamcast port had is swapped with it)
int options_port_nes(int port);
int options_port_device(int port);
void options_set_port_nes(int port, int nes);
void options_set_port_device(int port, int device);
// the ports, devices and four players handed to the core (input.h) before a game
void options_input_to_core();
bool load_options_from_VMU();
// a version 5 options file's contents (after its header) into options; 0 when it was one
int options_from_v5(const uint8_t* data, uint32_t size);
bool save_options_to_VMU();
bool delete_options_from_VMU();

#endif
