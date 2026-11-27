/*===================================================================*/
/*                                                                   */
/*  frontend.h : FrNES menus, options and ROM launching              */
/*                                                                   */
/*  2001/12/22  ReGex   FrNES 0.60 Source Release                    */
/*                                                                   */
/*===================================================================*/

#ifndef __FRONTEND_H
#define __FRONTEND_H

#include <kos.h>

#include "font.h"
#include "text_window.h"

#define APP_STRING "FrNES"
#define APP_VERSION "1.0.0"

// Neuro (titles, headers) and the monospace font (lists, values, help)
extern Font* font;
extern Font* font_mono;

// the main window (menu pages, the ROM browser) and the help window
extern window_style_t mystyle;
extern window_data_t mydata;
extern window_style_t helpstyle;
extern window_data_t helpdata;

/**
 * Plays a ROM with the options' emulator settings and comes back to the menus; 0, or -1 if it couldn't be started
 */
int launchEmulator(const char* path);
// the same with a movie (a .frm) playing over the game, or the game recorded into a new movie in record_directory
int launchEmulatorMovie(const char* path, const char* movie, const char* record_directory);

// the main menu, or the ROM browser, has the windows
void frontend_main_menu();
void frontend_browsing();

int draw_VMU_icon(maple_device_t* vmu, char* icon);

#endif
