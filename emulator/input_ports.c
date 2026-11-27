/*
 * The NES ports' controllers in the player builds (input.h): which Dreamcast controller drives each NES port, and once
 * a frame the pads from their controllers through frnes_settings' mappings (frontend's Controls), players 3 and 4
 * with four players on. Built without LTO and cold, as mapper_boards.c: nothing here changes the hot paths' code;
 * main.c makes one call a frame
 */
#include <kos.h>
#include <string.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>
#include "cpu.h"
#include "memory.h"
#include "input.h"
#include "mapper.h"
/*
 * The Dreamcast controller for NES port 0-3 by its source (input_set_port_source): a Dreamcast port's (A-D: its first
 * unit, if it's a controller), by default (INPUT_SOURCE_AUTO) the port's own, NES port n's Dreamcast port n (Matt,
 * 2026-11-02: A, B, C, D for players 1-4); none for INPUT_SOURCE_NONE
 */
static FRNES_COLD_TEXT maple_device_t* input_port_controller(uint32_t dc_port) {
    // (a port set to plug into nothing: Matt)
    if (input_ports_disabled() & (1 << dc_port)) {
        return NULL;
    }
    maple_device_t* device = maple_enum_dev(dc_port, 0);
    return (device && (device -> info.functions & MAPLE_FUNC_CONTROLLER)) ? device : NULL;
}

// a Dreamcast port's mouse (a Zapper's or the Vaus paddle's only: never a pad's)
static FRNES_COLD_TEXT maple_device_t* input_port_mouse(uint32_t dc_port) {
    if (input_ports_disabled() & (1 << dc_port)) {
        return NULL;
    }
    maple_device_t* device = maple_enum_dev(dc_port, 0);
    return (device && (device -> info.functions & MAPLE_FUNC_MOUSE)) ? device : NULL;
}

// NES port 0-3's source as it stands: one naming a Dreamcast port with no controller (or mouse) in it counts as
// INPUT_SOURCE_AUTO (Matt: a mapping for a controller not plugged in is left out; frontend's settings can name one)
static FRNES_COLD_TEXT uint8_t input_source(uint8_t nes_port) {
    uint8_t source = input_port_source(nes_port);
    return ((source <= 3) && !input_port_controller(source) && !input_port_mouse(source)) ? INPUT_SOURCE_AUTO : source;
}

// NES port 0-3's controller or mouse by its source (a Zapper's or the paddle's)
static FRNES_COLD_TEXT maple_device_t* input_device_or_mouse(uint8_t nes_port) {
    uint8_t source = input_source(nes_port);
    if (source == INPUT_SOURCE_AUTO) {
        source = nes_port;
    }
    if (source > 3) {
        return NULL;
    }
    maple_device_t* device = input_port_controller(source);
    return device ? device : input_port_mouse(source);
}

FRNES_COLD_TEXT struct maple_device* input_device(uint8_t nes_port) {
    uint8_t source = input_source(nes_port);
    if (source == INPUT_SOURCE_AUTO) {
        source = nes_port;
    }
    return (source <= 3) ? input_port_controller(source) : NULL;
}

/*
 * The NES ports' pads, once a frame (main.c's input_poll_ports, the player builds' only): NES ports 1 and 2's latches,
 * and with four players on players 3 and 4's for the strobe's fall (PAD_INPUT), from the controllers their sources
 * give (input_ports_start, when the game starts; NES port 1's is main.c's own, for its hotkeys too)
 */
// NES controller button bits, in the order the controller shifts them out
#define NES_BUTTON_A      0x01
#define NES_BUTTON_B      0x02
#define NES_BUTTON_SELECT 0x04
#define NES_BUTTON_START  0x08
#define NES_BUTTON_UP     0x10
#define NES_BUTTON_DOWN   0x20
#define NES_BUTTON_LEFT   0x40
#define NES_BUTTON_RIGHT  0x80
// left trigger as Select (0-255): pressed above, released below; ignored while the right trigger is above IDLE
#define LEFT_TRIGGER_PRESS 128
#define LEFT_TRIGGER_RELEASE 96
#define RIGHT_TRIGGER_IDLE 64
// the analog stick as the D-pad (frnes_pad_mapping_t's analog_dpad): past 90% of its travel (-128-127)
#define ANALOG_THRESHOLD 114

/*
 * A Dreamcast controller's state as an NES pad, through its mapping (frnes_settings): D-pad (or the analog stick) and
 * Start as they are, A / B / Select from the mapping's buttons. The left trigger is Select too past half way
 * (released below 3/8, so a trigger resting near the threshold doesn't flicker), but not while the right trigger is
 * pressed: both triggers held is the recording's save gesture, which mustn't press Select.
 */
/*
 * Turbo (frnes_pad_mapping_t's turbo_buttons): while a turbo button is held its NES button is pressed every other
 * stretch of frames (on 1 of 2 at 30 a second, 1 of 3 at 20, 2 of 4 at 15, 3 of 6 at 10). Taken with the pads, once a
 * frame; cold, as only a game played with turbo set runs it
 */
static const uint32_t turbo_masks[] FRNES_COLD_RODATA = { 0, CONT_A, CONT_B, CONT_X, CONT_Y, CONT_C, CONT_Z, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t turbo_periods[FRNES_TURBO_RATES] FRNES_COLD_RODATA = { 2, 3, 4, 6 };
// NES Start's buttons (FRNES_START_*)
static const uint32_t start_masks[16] FRNES_COLD_RODATA = { CONT_START, CONT_A, CONT_B, CONT_X, CONT_Y, CONT_C, CONT_Z, 0,
    CONT_X | CONT_B, CONT_A | CONT_C, CONT_START, CONT_START, CONT_START, CONT_START, CONT_START, CONT_START };

static FRNES_COLD_TEXT uint8_t turbo_state(uint32_t buttons, const frnes_pad_mapping_t* mapping, uint32_t frame) {
    uint32_t period = turbo_periods[mapping -> turbo_rate & (FRNES_TURBO_RATES - 1)];
    if ((frame % period) >= (period / 2)) {
        return 0;
    }
    return ((buttons & turbo_masks[mapping -> turbo_buttons & 15]) ? NES_BUTTON_A : 0) |
        ((buttons & turbo_masks[mapping -> turbo_buttons >> 4]) ? NES_BUTTON_B : 0);
}

static FRNES_COLD_TEXT uint8_t pad_state(const cont_state_t* state, const frnes_pad_mapping_t* mapping,
    uint8_t* left_trigger_select, uint32_t frame) {
    if (state -> ltrig > LEFT_TRIGGER_PRESS) {
        *left_trigger_select = 1;
    } else if (state -> ltrig < LEFT_TRIGGER_RELEASE) {
        *left_trigger_select = 0;
    }
    uint32_t buttons = state -> buttons;
    if (mapping -> analog_dpad) {
        buttons |= ((state -> joyx < -ANALOG_THRESHOLD) ? CONT_DPAD_LEFT : 0) | ((state -> joyx > ANALOG_THRESHOLD) ? CONT_DPAD_RIGHT : 0) |
            ((state -> joyy < -ANALOG_THRESHOLD) ? CONT_DPAD_UP : 0) | ((state -> joyy > ANALOG_THRESHOLD) ? CONT_DPAD_DOWN : 0);
    }
    uint32_t select = (buttons & mapping -> select_buttons) ||
        (mapping -> select_on_left_trigger && *left_trigger_select && (state -> rtrig < RIGHT_TRIGGER_IDLE));
    uint8_t turbo = mapping -> turbo_buttons ? turbo_state(buttons, mapping, frame) : 0;
    return turbo | ((buttons & mapping -> a_buttons) ? NES_BUTTON_A : 0) |
        ((buttons & mapping -> b_buttons) ? NES_BUTTON_B : 0) |
        (select ? NES_BUTTON_SELECT : 0) |
        ((buttons & start_masks[mapping -> turbo_rate >> FRNES_START_SHIFT]) ? NES_BUTTON_START : 0) |
        ((buttons & CONT_DPAD_UP) ? NES_BUTTON_UP : 0) |
        ((buttons & CONT_DPAD_DOWN) ? NES_BUTTON_DOWN : 0) |
        ((buttons & CONT_DPAD_LEFT) ? NES_BUTTON_LEFT : 0) |
        ((buttons & CONT_DPAD_RIGHT) ? NES_BUTTON_RIGHT : 0);
}


// the routing, at the game's start (input_ports_resolve): each NES port's pad's controller (NULL: released), each
// Zapper's controller (NES ports 0-1), and whether it's player 1's pad's too; each port's left trigger as Select
static maple_device_t* input_pad_devices[4] FRNES_COLD_BSS;
static maple_device_t* input_zapper_devices[2] FRNES_COLD_BSS;
static maple_device_t* input_vaus FRNES_COLD_BSS;
static uint8_t input_zapper_shares[2] FRNES_COLD_BSS;
static uint8_t input_left_trigger_select[4] FRNES_COLD_BSS;

static FRNES_COLD_TEXT int input_device_taken(const maple_device_t* device) {
    for (uint32_t i = 0; i < 4; i++) {
        if (device == input_pad_devices[i]) {
            return 1;
        }
    }
    return (device == input_zapper_devices[0]) || (device == input_zapper_devices[1]) || (device == input_vaus);
}

// a controller another NES port's source names (not auto, not none): player 1's fallback doesn't take it
static FRNES_COLD_TEXT int input_device_claimed(const maple_device_t* device) {
    for (uint8_t port = 1; port < 4; port++) {
        uint8_t source = input_source(port);
        if ((source <= 3) && (input_port_controller(source) == device)) {
            return 1;
        }
    }
    return 0;
}

// the lowest numbered Dreamcast port's controller no NES port has or names (NULL: none)
static FRNES_COLD_TEXT maple_device_t* input_free_controller(void) {
    for (uint32_t port = 0; port < 4; port++) {
        maple_device_t* device = input_port_controller(port);
        if (device && !input_device_taken(device) && !input_device_claimed(device)) {
            return device;
        }
    }
    return NULL;
}

// the lowest numbered Dreamcast port's mouse no NES port has (NULL: none)
static FRNES_COLD_TEXT maple_device_t* input_free_mouse(void) {
    for (uint32_t port = 0; port < 4; port++) {
        maple_device_t* device = input_port_mouse(port);
        if (device && !input_device_taken(device)) {
            return device;
        }
    }
    return NULL;
}

// the lowest numbered Dreamcast port's light gun no NES port has (NULL: none)
static FRNES_COLD_TEXT maple_device_t* input_free_gun(void) {
    for (uint32_t port = 0; port < 4; port++) {
        maple_device_t* device = input_port_controller(port);
        if (device && (device -> info.functions & MAPLE_FUNC_LIGHTGUN) && !input_device_taken(device)) {
            return device;
        }
    }
    return NULL;
}

/*
 * Which controller drives what (input.h's sources and devices, the game's Zappers and four players; a source naming a
 * Dreamcast port with nothing in it counts as auto: input_source), in this order:
 * - player 1's pad (NES port 0, when it isn't a Zapper's): its source's controller; on auto with nothing in Dreamcast
 *   port A, the lowest numbered controller no other port's source names (a lone pad in port B plays);
 * - a Zapper whose port's source is set (or with a Zapper on each port: Chiller) takes that controller, player 1's
 *   pad's included (the frontend names the Zapper's port, often player 1's: it's then shared);
 * - the other NES ports' pads (players 3 and 4 only with four players), their sources' controllers, none another port
 *   has (a Zapper's never becomes a pad's);
 * - any other Zapper (its port's source auto) takes a light gun no port has (Matt: a gun plugged in is the Zapper),
 *   or else player 1's controller: a Zapper on NES port 0 its own (or the lowest numbered
 *   free one), one on port 1 player 1's pad's.
 * A Zapper on player 1's pad's controller leaves that pad only Start and Select (zapper_player_input)
 */
static FRNES_COLD_TEXT void input_ports_resolve(void) {
    memset(input_pad_devices, 0, sizeof(input_pad_devices));
    memset(input_zapper_devices, 0, sizeof(input_zapper_devices));
    memset(input_zapper_shares, 0, sizeof(input_zapper_shares));
    input_vaus = NULL;
    int zapper[2] = { input_port_device_active(0) == INPUT_DEVICE_ZAPPER, input_port_device_active(1) == INPUT_DEVICE_ZAPPER };
    int two = zapper[0] && zapper[1];
    if (!zapper[0]) {
        input_pad_devices[0] = input_device(0);
        if (!input_pad_devices[0] && (input_source(0) == INPUT_SOURCE_AUTO)) {
            input_pad_devices[0] = input_free_controller();
        }
    }
    for (uint8_t port = 0; port < 2; port++) {
        if (zapper[port] && (two || (input_source(port) != INPUT_SOURCE_AUTO))) {
            maple_device_t* device = input_device_or_mouse(port);
            if (device && (device == input_pad_devices[0])) {
                input_zapper_devices[port] = device;
                input_zapper_shares[port] = 1;
            } else if (device && !input_device_taken(device)) {
                input_zapper_devices[port] = device;
            }
        }
    }
    uint32_t ports = PAD_INPUT -> four_score ? 4 : 2;
    for (uint8_t port = 1; port < ports; port++) {
        if ((port < 2) && zapper[port]) {
            continue;
        }
        maple_device_t* device = input_device(port);
        input_pad_devices[port] = (device && !input_device_taken(device)) ? device : NULL;
    }
    for (uint8_t port = 0; port < 2; port++) {
        if (!zapper[port] || input_zapper_devices[port] || (two && input_device(port))) {
            continue;
        }
        // (a light gun no port has, or else a mouse: Matt)
        maple_device_t* gun = input_free_gun();
        if (!gun) {
            gun = input_free_mouse();
        }
        if (gun) {
            input_zapper_devices[port] = gun;
        } else if (port == 0) {
            maple_device_t* device = input_device(0);
            input_zapper_devices[0] = (device && !input_device_taken(device)) ? device : input_free_controller();
        } else {
            input_zapper_devices[1] = input_pad_devices[0];
            input_zapper_shares[1] = input_pad_devices[0] != NULL;
        }
    }
}

// the Vaus paddle's controller or mouse: its port's source's when set, else a mouse no port has, else player 1's
// controller (shared with the pad: the Vaus games read no pad) (input_ports_resolve's last step)
static FRNES_COLD_TEXT void input_vaus_resolve(void) {
    if (input_port_device_active(1) != INPUT_DEVICE_PADDLE) {
        return;
    }
    // (a port the frontend set to Paddle: NES port 2's source then. Not any device NES port 2's source names: with
    // the ports moved round, that's a light gun's or a pad's, Matt's gun in port B turning the knob)
    if (input_paddle_chosen() && (input_source(1) != INPUT_SOURCE_AUTO)) {
        input_vaus = input_device_or_mouse(1);
        if (input_vaus && (input_vaus == input_pad_devices[1])) {
            input_pad_devices[1] = NULL;
        }
        return;
    }
    input_vaus = input_free_mouse();
    if (!input_vaus) {
        input_vaus = input_pad_devices[0];
    }
}

FRNES_COLD_TEXT struct maple_device* input_vaus_device(void) {
    return input_vaus;
}

// main.c's controller: player 1's pad's, or with none the first found (the hotkeys and the exit buttons only)
FRNES_COLD_TEXT struct maple_device* input_first_device(void) {
    input_ports_resolve();
    input_vaus_resolve();
    return input_pad_devices[0] ? input_pad_devices[0] : maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
}

FRNES_COLD_TEXT void input_ports_start(void) {
    input_ports_resolve();
    input_vaus_resolve();
    memset(input_left_trigger_select, 0, sizeof(input_left_trigger_select));
    // (main.c's game start, after its battery load and before the CPU runs: mapper_boards.c's board_fresh_wram)
    board_fresh_wram();
}

// a Zapper's controller (zapper_player_input: NULL, none), and whether it's player 1's pad's too
FRNES_COLD_TEXT struct maple_device* input_zapper_device(uint8_t nes_port) {
    return (nes_port < 2) ? input_zapper_devices[nes_port] : NULL;
}

FRNES_COLD_TEXT int input_zapper_shares_pad(uint8_t nes_port) {
    return (nes_port < 2) && input_zapper_shares[nes_port];
}

// whether player 1's pad keeps only Start and Select from its controller this frame: while a Zapper sharing it is on
// (off, the pad has all of it: zapper_set_enabled)
FRNES_COLD_TEXT int input_pad_masked(void) {
    for (uint8_t port = 0; port < 2; port++) {
        if (input_zapper_shares[port] && zapper_enabled(port)) {
            return 1;
        }
    }
    return 0;
}

// (the mappings: frnes_settings' pads, FRNES_PADS of them, the last one's for the ports past them)
FRNES_COLD_TEXT void input_poll_ports(const void* first_state, const frnes_pad_mapping_t* mappings, uint32_t frame) {
    // (a reset the game menu asked for, here at the input point: frnes_soft_reset)
    frnes_reset_input_point();
    cpu_registers -> PAD1_Latch = (input_pad_devices[0] && first_state) ?
        pad_state((const cont_state_t*)first_state, &mappings[0], &input_left_trigger_select[0], frame) : 0;
    uint32_t ports = PAD_INPUT -> four_score ? 4 : 2;
    uint8_t buttons[4];
    for (uint32_t i = 1; i < ports; i++) {
        cont_state_t* state = input_pad_devices[i] ? (cont_state_t*)maple_dev_status(input_pad_devices[i]) : NULL;
        buttons[i] = state ? pad_state(state, &mappings[(i < FRNES_PADS) ? i : FRNES_PADS - 1], &input_left_trigger_select[i], frame) : 0;
    }
    cpu_registers -> PAD2_Latch = buttons[1];
    if (ports == 4) {
        PAD_INPUT -> pad3 = buttons[2];
        PAD_INPUT -> pad4 = buttons[3];
    }
    // (a Vs. System game's 1P start is the Select bit: Start is it, the Select buttons 2P start)
    if (cpu_registers -> VS_Flags) {
        vs_start_swap();
    }
}
