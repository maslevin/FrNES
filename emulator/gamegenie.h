#ifndef __GAMEGENIE_H
#define __GAMEGENIE_H

#include "frnes.h"

// the ROM bytes the codes on now changed, and what they were (the caller keeps it: no static storage in the core)
typedef struct gamegenie_undo gamegenie_undo_t;

/**
 * Puts the Game Genie codes in codes (FRNES_CHEATS of them, "" for none; NULL: none at all) on the loaded game's ROM,
 * taking off the ones in *undo first: at load (*undo NULL), and again whenever the codes are changed between frames.
 * Cold code; not for FDS games (their program is loaded from the disk as they run)
 */
void gamegenie_apply(const char (*codes)[9], gamegenie_undo_t** undo);
// at the game's end (the ROM is freed with it)
void gamegenie_free(gamegenie_undo_t** undo);
// the codes on in the game being played (main.c applies frnes_cheats() at its start; the game menu switches them)
extern gamegenie_undo_t* gamegenie_undo;

#endif
