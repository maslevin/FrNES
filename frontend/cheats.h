/*===================================================================*/
/*                                                                   */
/*  cheats.h : Game Genie codes for a game (over the browser)        */
/*                                                                   */
/*===================================================================*/

#ifndef __CHEATS_H
#define __CHEATS_H

#include <kos.h>

#define CHEAT_SLOTS 3
#define CHEAT_CODE_LENGTH 8
#define CHEAT_TEXT_LENGTH 40

/*
 * A game's Game Genie codes: three slots, each a code (6 or 8 letters), what it does, and on or off, kept in the
 * options for the game last set up (for the emulator's Game Genie, when it has one). Codes come from typing them on
 * the Game Genie's letters, or from a list beside the game: "<the ROM's path without .nes>.cheats" or
 * "/sd/FrNES/cheats/<the ROM's name without .nes>.cheats", a line each, the code then what it does ('#' starts a
 * comment line).
 */

/**
 * Opens the cheats window over the screen for the ROM at path (its slots, if they were this game's)
 */
void cheats_open(const char* rom_path);

int cheats_active();

// takes the input while it's open (1 if it did)
int cheats_input(uint32 input);

void cheats_draw(uint32 list);

/**
 * A code's address, value and compare value (-1 for a 6-letter code); 0, or -1 if it isn't a Game Genie code
 */
int cheats_decode(const char* code, uint16* address, uint8* value, int* compare);

#endif
