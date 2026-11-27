/*===================================================================*/
/*                                                                   */
/*  dialog.h : A message over the screen, closed with A              */
/*                                                                   */
/*===================================================================*/

#ifndef __DIALOG_H
#define __DIALOG_H

#include <kos.h>

#define DIALOG_LINES 4
#define DIALOG_LINE_LENGTH 128

/**
 * Shows a dialog over the screen, a title and up to DIALOG_LINES lines (copied, wrapped where they're too wide, the
 * dialog as wide as they need); the screen under it takes no input
 * until A (or B) closes it
 */
void dialog_show(const char* title, const char* const* lines, int count);

// whether one is up; its input (A or B closes it)
int dialog_active();
void dialog_handle(uint32 input);

/**
 * Draws it, if one is up, after the screen's own windows in each list: a shade over them, then the dialog in front
 */
void dialog_draw(uint32 list);

#endif
