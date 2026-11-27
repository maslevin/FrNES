#ifndef __INPUT_H
#define __INPUT_H

#include <stdint.h>
#include "frnes.h"

/*
 * The NES ports' controllers and devices: the frontend's settings (set any time, before a game too, and kept across
 * games like the Vs. System's DIP switches; each game's load takes them), and what the loaded game has. In
 * mapper_boards.c with the Zapper's
 */

// which Dreamcast controller drives NES port 0-3: a Dreamcast port A-D (0-3), INPUT_SOURCE_AUTO (the default: NES port
// n's Dreamcast port n, A-D for players 1-4; player 1 with nothing in port A takes the lowest numbered controller no
// other port has) or INPUT_SOURCE_NONE. A Zapper takes its port's controller only when its source is set (or with a
// Zapper on each port), else player 1's (input_ports.c's routing)
#define INPUT_SOURCE_AUTO 0xfe
#define INPUT_SOURCE_NONE 0xff
void input_set_port_source(uint8_t nes_port, uint8_t source);
uint8_t input_port_source(uint8_t nes_port);

// the device on NES port 0-1: INPUT_DEVICE_AUTO (the game's: NES 2.0 byte 15, or the game database's for an older
// header), INPUT_DEVICE_PAD or INPUT_DEVICE_ZAPPER (whatever the game). Vs. games take their own
#define INPUT_DEVICE_AUTO 0
#define INPUT_DEVICE_PAD 1
#define INPUT_DEVICE_ZAPPER 2
// (NES port 1 only: the Arkanoid (Vaus) paddle, the NES one or the Famicom one as the game has it)
#define INPUT_DEVICE_PADDLE 3
void input_set_port_device(uint8_t nes_port, uint8_t device);

// four players: INPUT_FOUR_SCORE_AUTO (the game's: the NES Four Score or the Famicom's four player adapter, NES 2.0
// byte 15 or the game database), INPUT_FOUR_SCORE_OFF, INPUT_FOUR_SCORE_ON (the game's kind, the Four Score for a game
// that has none)
#define INPUT_FOUR_SCORE_AUTO 0
#define INPUT_FOUR_SCORE_OFF 1
#define INPUT_FOUR_SCORE_ON 2
void input_set_four_score(uint8_t mode);

// the loaded game's (valid after its load): INPUT_FOUR_SCORE_NONE, _NES (the Four Score / NES Satellite) or _FAMICOM
// (the Famicom's four player adapter: players 3 and 4 on $4016 / $4017 bit 1)
#define INPUT_FOUR_SCORE_NONE 0
#define INPUT_FOUR_SCORE_NES 1
#define INPUT_FOUR_SCORE_FAMICOM 2
uint8_t input_four_score_active(void);
// the device on NES port 0-1 in the loaded game: INPUT_DEVICE_PAD or INPUT_DEVICE_ZAPPER (the Vs. Zapper on port 0)
uint8_t input_port_device_active(uint8_t nes_port);

// the Dreamcast controller (KOS's maple_device_t) for NES port 0-3 by its source alone (NULL: none; the player builds'
// only); the routing's: player 1's (main.c's: or with none the first found, for the hotkeys), a Zapper's and whether
// it's player 1's pad's too (zapper_player_input)
struct maple_device;
struct maple_device* input_device(uint8_t nes_port);
struct maple_device* input_zapper_device(uint8_t nes_port);
int input_zapper_shares_pad(uint8_t nes_port);
int input_pad_masked(void);
// the Vaus paddle's controller or mouse (NULL: none), the routing's
struct maple_device* input_vaus_device(void);
// whether the frontend set a port to Paddle (input_set_port_device(1, INPUT_DEVICE_PADDLE))
int input_paddle_chosen(void);
// the Dreamcast ports (bit n for port n) the frontend set to plug into nothing: nothing in them is used, a light gun
// or mouse included
void input_set_ports_disabled(uint8_t mask);
uint8_t input_ports_disabled(void);
// main.c's controller, for NES port 1's pad, the hotkeys and the exit buttons: NES port 1's, or with none there the
// first found (its pad then stays released: input_ports_start)
struct maple_device* input_first_device(void);
// when a game starts, NES ports 2-4's controllers (main.c; with four players on, 3 and 4's)
void input_ports_start(void);
// once a frame: the NES ports' pads from their controllers through frnes_settings' mappings (main.c: NES port 1's
// controller state, KOS's cont_state_t, which main.c reads for its hotkeys too)
void input_poll_ports(const void* first_state, const frnes_pad_mapping_t* mappings, uint32_t frame);

// the Zapper on NES port 0-1 turned off (0) or on in the game, between frames (the frontend's menu: a game such as
// The Adventures of Bayou Billy uses the pad most of the time). Off: plugged in but idle, the reticle hidden, its
// controller the pad's as usual; on: its controller's stick, D-pad, A and B aim and fire, Start and Select still the
// pad's. On again at each game's load. (The Vs. Zapper is port 0's)
void zapper_set_enabled(uint8_t nes_port, uint8_t on);
// the Zapper on NES port 0-1's aim (y < 0 or >= 240: off the screen) and trigger for the frame (movies, the host)
void zapper_port_aim(uint8_t nes_port, int x, int y, int trigger);
uint8_t zapper_enabled(uint8_t nes_port);

// movies with players 3 and 4 or the Zappers (input_recording.h's version 4, movie.c): the extension block for the
// loaded game (0: it needs none), a frame's extension from what it used, and a played frame's applied. Flags:
#define INPUT_EXT_ZAPPER_ON(port) (1 << (port))
#define INPUT_EXT_TRIGGER(port) (4 << (port))
#include "input_recording.h"
int input_ext_block(input_recording_ext_t* ext);
void input_ext_capture(input_recording_ext_run_t* ext);
void input_ext_apply(const input_recording_ext_run_t* ext);

// the strobe's fall with four players on (6502_writebyte.c)
void input_strobe_fall(void);

#endif
