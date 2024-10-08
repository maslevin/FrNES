/*===================================================================*/
/*                                                                   */
/*  pNesX_System_DC.h : DC System Functions, GUI                     */
/*                                                                   */
/*  2001/12/22  ReGex   FrNES 0.60 Source Release                    */
/*                                                                   */
/*===================================================================*/

#ifndef __PNESX_SYS_DC_H
#define __PNESX_SYS_DC_H

#include <kos.h>

#include "font.h"
#include "TextWindow.h"
#include "ROMLoad.h"

//#define table for GUI
#define PAGE_ROMSELECT          0
#define PAGE_VIDEOOPTIONS       1
#define PAGE_CONTROLOPTIONS     2
#define PAGE_SYSTEMOPTIONS      3
#define PAGE_GUIOPTIONS         4
#define PAGE_HELP               5
#define PAGE_CREDITS            6
#define MENUNUM_MAIN            0
#define MENUNUM_ROMSELECT       1
#define MENUNUM_VIDEOOPTIONS    2
#define MENUNUM_CONTROLOPTIONS  3
#define MENUNUM_SYSTEMOPTIONS   4
#define MENUNUM_GUIOPTIONS      5
#define MENUNUM_FONTOPTIONS     6
#define MENUNUM_HELP            7
#define MENUNUM_CREDITS         8
#define MENUNUM_AUTOROM         99

#define RECORDING_MODE_DISABLED 0
#define RECORDING_MODE_ENABLED 1
#define RECORDING_MODE_PLAYBACK 2

extern uint8 recordingMode;

extern bool log_enabled;

extern char szRomPath[];
extern uint32 RomSize;

void Load_VMU_Options();
void Save_VMU_Options();
void initialize_controllers();
void rescan_controllers();

void calculateOutputScreenGeometry();

int LoadSRAM();
int SaveSRAM();

int draw_VMU_icon(maple_device_t* vmu, char* icon);

void launchEmulator();

#ifdef DEBUG
void pNesX_DebugPrint(char* fmt, ...);
#else
#define pNesX_DebugPrint(...) (0)
#endif

#endif