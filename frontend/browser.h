/*===================================================================*/
/*                                                                   */
/*  browser.h : The ROM browser                                      */
/*                                                                   */
/*===================================================================*/

#ifndef __BROWSER_H
#define __BROWSER_H

#include <kos.h>

/**
 * Shows the browser in the main window, in the directory it was last left in (options.browser_directory), or the
 * list of devices
 */
void browser_open();

/**
 * A dialog saying the ROM at path didn't start, and why as far as its file shows
 */
void browser_launch_failed(const char* path);

/**
 * The highlighted game's box at the status window's right (once it's loaded), in the translucent list
 */
void browser_draw(uint32 list);

// lets the box's texture go (before the PVR is shut down)
void browser_release_art();

/**
 * Acts on the input (INPUT_* events): moves, opens directories, starts games (A), goes up (B; from the device list,
 * back to the main menu)
 */
void browser_handle(uint32 input);

#endif
