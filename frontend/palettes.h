/*===================================================================*/
/*                                                                   */
/*  palettes.h : The NES palettes to choose from                     */
/*                                                                   */
/*===================================================================*/

#ifndef __PALETTES_H
#define __PALETTES_H

#include <kos.h>

/*
 * The .pal files in /rd/palettes (64 colors as RGB, or 512 with the emphasis bits' colors after: the first 64 are
 * used). FrNES.pal holds the emulator's own colors and is listed first; choosing it leaves the emulator's palette as
 * it is.
 */
#define PALETTE_DIRECTORY "/rd/palettes"
#define PALETTE_DEFAULT "FrNES"
#define MAX_PALETTES 32

/**
 * Lists the palettes, and takes options.palette_name's colors (or the default's, if it's gone)
 */
void palettes_scan();

int palettes_count();
const char* const* palettes_names();

// the chosen palette's index in the list
int palettes_current();

/**
 * Chooses a palette: its name and colors into the options (options.palette_name, options.palette)
 */
void palettes_choose(int index);

/**
 * The chosen palette's 64 colors (ARGB), for drawing it
 */
const uint32* palettes_colors();

#endif
