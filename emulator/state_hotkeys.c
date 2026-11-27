#include <kos.h>

#include "state.h"
#include "memory.h"
#include "frame_hud.h"
#include "frnes.h"
#include "vmu_save.h"
#include "game_menu.h"
#include "cartridge.h"
#include "trigger_gesture.h"
#include "input.h"
#include <stdlib.h>

/*
 * The save / load state hotkeys (state.h's state_hotkeys / state_hotkey_run): the frontend's settings, the frame
 * display's messages. Apart from state.c, which is the core's alone
 */
// does a hotkey combination (FRNES_HOTKEY_*) match the pad: triggers pressed past half way, the face buttons
FRNES_COLD_TEXT static int hotkey_down(uint32_t combination, uint32_t buttons, int ltrig, int rtrig) {
    static const uint32_t faces[4] = { CONT_A, CONT_B, CONT_X, CONT_Y };
    // (past half way: bit 7, as the frame loop tests before calling)
    int lt = (ltrig & 0x80) != 0, rt = (rtrig & 0x80) != 0;
    if (combination == FRNES_HOTKEY_RT) {
        return rt && !lt;
    }
    if (combination == FRNES_HOTKEY_LT_RT) {
        return rt && lt;
    }
    if ((combination >= FRNES_HOTKEY_RT_A) && (combination < FRNES_HOTKEY_RT_A + 4)) {
        return rt && !lt && (buttons & faces[combination - FRNES_HOTKEY_RT_A]);
    }
    if ((combination >= FRNES_HOTKEY_LT_A) && (combination < FRNES_HOTKEY_LT_A + 4)) {
        return lt && !rt && (buttons & faces[combination - FRNES_HOTKEY_LT_A]);
    }
    if (combination == FRNES_HOTKEY_C_Z) {
        return (buttons & (CONT_C | CONT_Z)) == (CONT_C | CONT_Z);
    }
    return 0;
}

// the game menu's hotkey for this game (state_hotkeys_start: LT + RT is C + Z on an arcade stick), and
// STATE_HOTKEYS_POLL when a hotkey has no trigger, so the frame loop looks at the buttons every frame
#define STATE_HOTKEYS_POLL 0x40
static uint8_t menu_hotkey FRNES_COLD_BSS, hotkeys_poll FRNES_COLD_BSS;

FRNES_COLD_TEXT static void state_hotkeys_start(void) {
    // (player 1's controller without triggers, with C and Z: the arcade stick)
    maple_device_t* first = (maple_device_t*)input_first_device();
    int stick = first && !cont_has_capabilities(first, CONT_CAPABILITY_LTRIG | CONT_CAPABILITY_RTRIG) &&
        cont_has_capabilities(first, CONT_CAPABILITY_C | CONT_CAPABILITY_Z);
    menu_hotkey = frnes_settings.hotkeys[1] >> 4;
    if (stick && (menu_hotkey == FRNES_HOTKEY_LT_RT)) {
        menu_hotkey = FRNES_HOTKEY_C_Z;
    }
    hotkeys_poll = ((menu_hotkey == FRNES_HOTKEY_C_Z) || ((frnes_settings.hotkeys[0] & 15) == FRNES_HOTKEY_C_Z) ||
        ((frnes_settings.hotkeys[0] >> 4) == FRNES_HOTKEY_C_Z)) ? STATE_HOTKEYS_POLL : 0;
}

FRNES_COLD_TEXT uint8_t state_hotkeys_held_start(void) {
    return hotkeys_poll;
}

FRNES_COLD_TEXT uint8_t state_hotkeys(uint32_t buttons, int ltrig, int rtrig, uint8_t* held) {
    uint8_t down = 0;
    if (hotkey_down(frnes_settings.hotkeys[0] & 15, buttons, ltrig, rtrig)) {
        down |= STATE_HOTKEY_SAVE;
    }
    if (hotkey_down(frnes_settings.hotkeys[0] >> 4, buttons, ltrig, rtrig)) {
        down |= STATE_HOTKEY_LOAD;
    }
    if (hotkey_down(menu_hotkey, buttons, ltrig, rtrig)) {
        down |= STATE_HOTKEY_MENU;
    }
    uint8_t pressed = down & ~*held;
    *held = down | hotkeys_poll;
    // (two at once: neither)
    return (pressed == STATE_HOTKEY_SAVE || pressed == STATE_HOTKEY_LOAD || pressed == STATE_HOTKEY_MENU) ? pressed : 0;
}

// FDS games' flip disk hotkey (state_set_flip_hotkey), and whether it was down last frame
static uint8_t flip_hotkey FRNES_COLD_BSS, flip_held FRNES_COLD_BSS;

FRNES_COLD_TEXT void state_set_flip_hotkey(uint8_t combination) {
    flip_hotkey = (combination && (combination < FRNES_HOTKEY_COUNT)) ? combination : FRNES_HOTKEY_RT;
}

FRNES_COLD_TEXT int state_flip_requested(uint32_t buttons, int ltrig, int rtrig, uint32_t gesture) {
    uint8_t combination = flip_hotkey ? flip_hotkey : FRNES_HOTKEY_RT;
    if (combination == FRNES_HOTKEY_RT) {
        return gesture == TRIGGER_HOLD;
    }
    int down = hotkey_down(combination, buttons, ltrig, rtrig);
    int pressed = down && !flip_held;
    flip_held = down;
    return pressed;
}

// a hotkey combination's name on the screen (the frame display's glyphs: capitals, digits, '+')
static FRNES_COLD_TEXT const char* hotkey_text(uint8_t combination) {
    static const char* const names[FRNES_HOTKEY_COUNT] = { "", "RT", "RT+A", "RT+B", "RT+X", "RT+Y", "LT+RT", "LT+A",
        "LT+B", "LT+X", "LT+Y", "C+Z" };
    return (combination < FRNES_HOTKEY_COUNT) ? names[combination] : "";
}

FRNES_COLD_TEXT const char* state_start_message(int movie_playing) {
    static char message[96] FRNES_COLD_BSS;
    // how to leave, how to open the game menu (its hotkey, when it has one), and on an FDS game how to flip the disk;
    // a movie's first line says it's playing (up to three lines: frame_hud_message)
    state_hotkeys_start();
    uint8_t menu = menu_hotkey;
    char menu_line[32] = "";
    if (menu && (menu < FRNES_HOTKEY_COUNT)) {
        snprintf(menu_line, sizeof(menu_line), "\n%s TO OPEN MENU", hotkey_text(menu));
    }
    if (movie_playing) {
        snprintf(message, sizeof(message), "MOVIE PLAYING\nA+B+X+Y+START TO EXIT%s", menu_line);
    } else if (cartridge_mapper_number != 20) {
        // (a game with a pad and a Zapper, as Bayou Billy, starts with the Zapper off: the game menu turns it on)
        int zapper_off = 0;
        for (uint8_t port = 0; port < 2; port++) {
            zapper_off |= (input_port_device_active(port) == INPUT_DEVICE_ZAPPER) && !zapper_enabled(port);
        }
        // (a Vs. System game: how to put a coin in, main.c's VS_COIN_PRESSED, and to start: the Zapper's games on a shot,
        // the others' 1P start on Start and 2P start on Select, input_ports.c)
        // (Nintendo World Championships 1990, mapper 105, and Campus Challenge 1991, 555: their title waits for Start on
        // the $4017 pad, NES port 2's)
        const char* third = ((cartridge_mapper_number == 105) || (cartridge_mapper_number == 555)) ? "\nSTART ON PAD 2 TO BEGIN" :
            !(cpu_registers -> VS_Flags & VS_SYSTEM) ? (zapper_off ? "\nTOGGLE ZAPPER IN MENU" : "") :
            ((cpu_registers -> VS_Flags & VS_ZAPPER) ? "\nLT+START COIN / SHOOT TO START" : "\nLT+START COIN / SELECT 2P START");
        snprintf(message, sizeof(message), "A+B+X+Y+START TO EXIT%s%s", menu_line, third);
    } else {
        uint8_t flip = flip_hotkey ? flip_hotkey : FRNES_HOTKEY_RT;
        snprintf(message, sizeof(message), "A+B+X+Y+START TO EXIT%s\n%s %s TO FLIP DISK", menu_line,
            (flip == FRNES_HOTKEY_RT) ? "HOLD" : "PRESS", hotkey_text(flip));
    }
    return message;
}

// (main.c's: the game being played)
extern const char* frnes_rom_path;

// a state for the VMU, captured now and written in the background (main.c's thread compresses and writes it, its
// result a message when it's done): 0 when it was handed over, or -1
FRNES_COLD_TEXT static int state_vmu_save(int slot) {
    uint8_t* data;
    uint32_t size;
    if (state_capture(&data, &size) != 0) {
        return -1;
    }
    return autosave_state_write(data, size, slot);
}

FRNES_COLD_TEXT static int state_vmu_load(int slot) {
    uint8_t* data;
    uint32_t size;
    if (vmu_state_load(currentCRC32, slot, &data, &size) != 0) {
        return -1;
    }
    int result = state_restore(data, size);
    free(data);
    return result;
}

uint8_t state_slot FRNES_COLD_BSS;

// which slots hold a state for the game being played, where the SD Card page says (learned at a slot's first load, set
// by each save): a load from an empty slot then opens no files (a load hotkey held down with the fire button stalled
// a frame on every shot, a failed open over dc-load-ip or the SD card each time)
static uint32_t slots_crc FRNES_COLD_BSS;
static uint16_t slots_known FRNES_COLD_BSS, slots_present FRNES_COLD_BSS;
static uint8_t slots_device FRNES_COLD_BSS, slots_valid FRNES_COLD_BSS;

static FRNES_COLD_TEXT void slots_for_game(void) {
    if (!slots_valid || (slots_crc != currentCRC32) || (slots_device != frnes_settings.state_device)) {
        slots_crc = currentCRC32;
        slots_device = frnes_settings.state_device;
        slots_known = slots_present = 0;
        slots_valid = 1;
    }
}

// save states turned off (the Saves page): said on the screen instead
static FRNES_COLD_TEXT int states_off(void) {
    if (frnes_settings.state_device != FRNES_STATE_OFF) {
        return 0;
    }
    frame_hud_message("SAVE STATES ARE OFF", 120);
    return 1;
}

FRNES_COLD_TEXT int state_slot_save(int slot) {
    uint8_t device = frnes_settings.state_device;
    if (states_off()) {
        return -1;
    }
    int saved = -1;
    if (device != FRNES_STATE_VMU) {
        saved = state_save(slot);
    }
    int handed_over = 0;
    if ((saved != 0) && (device != FRNES_STATE_SD)) {
        // (handed over: the message when it's written)
        saved = state_vmu_save(slot);
        handed_over = (saved == 0);
    }
    slots_for_game();
    slots_known |= 1 << slot;
    slots_present = (saved == 0) ? (slots_present | (1 << slot)) : (slots_present & ~(1 << slot));
    if (handed_over) {
        return 0;
    }
    char message[24];
    snprintf(message, sizeof(message), (saved == 0) ? "STATE %d SAVED" : "STATE %d NOT SAVED", slot);
    frame_hud_message(message, 120);
    return saved;
}

FRNES_COLD_TEXT int state_slot_load(int slot) {
    uint8_t device = frnes_settings.state_device;
    if (states_off()) {
        return -1;
    }
    int loaded = -1;
    char message[32];
    slots_for_game();
    if ((slots_known & ~slots_present) & (1 << slot)) {
        snprintf(message, sizeof(message), "NO STATE IN SLOT %d", slot);
        frame_hud_message(message, 120);
        return -1;
    }
    int older = 0;
    if (device != FRNES_STATE_VMU) {
        loaded = state_load(slot);
        older = loaded == STATE_OLDER;
    }
    if ((loaded != 0) && (device != FRNES_STATE_SD)) {
        // (after a state still being written)
        autosave_wait();
        loaded = state_vmu_load(slot);
        older |= loaded == STATE_OLDER;
    }
    slots_known |= 1 << slot;
    // (a state from an older version is there, and says so)
    slots_present = ((loaded == 0) || older) ? (slots_present | (1 << slot)) : (slots_present & ~(1 << slot));
    snprintf(message, sizeof(message), (loaded == 0) ? "STATE %d LOADED" : (older ? "STATE FROM AN OLDER VERSION" : "NO STATE IN SLOT %d"), slot);
    frame_hud_message(message, 120);
    return loaded;
}

FRNES_COLD_TEXT uint8_t state_hotkey_run(uint8_t action) {
    if (!cpu_exact_dma_idle()) {
        // (a DMA under way: at the next frame's end)
        return action;
    }
    if (action == STATE_HOTKEY_SAVE) {
        state_slot_save(state_slot);
    } else if (action == STATE_HOTKEY_LOAD) {
        state_slot_load(state_slot);
    } else if (action == STATE_HOTKEY_MENU) {
        return game_menu_run() ? STATE_HOTKEY_EXIT : 0;
    }
    return 0;
}
