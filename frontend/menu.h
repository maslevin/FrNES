/*===================================================================*/
/*                                                                   */
/*  menu.h : Menu pages as data                                      */
/*                                                                   */
/*===================================================================*/

#ifndef __MENU_H
#define __MENU_H

#include <kos.h>
#include "text_window.h"

// what a row is: a line of text, something to do (A), an on / off (A or left / right), one of a list of choices,
// a number (left / right, repeating while held), or a number shown as a slider
#define MENU_TEXT 0
#define MENU_ACTION 1
#define MENU_TOGGLE 2
#define MENU_CHOICE 3
#define MENU_NUMBER 4
#define MENU_SLIDER 5

typedef struct Menu_Item_s {
	int kind;
	const char* label;
	// the value (toggle, choice, number, slider): a byte, or get / set for anything else
	uint8* value;
	int (*get)(const struct Menu_Item_s* item);
	void (*set)(const struct Menu_Item_s* item, int value);
	// MENU_CHOICE: the names of the values 0 .. count - 1
	const char* const* choices;
	int count;
	// MENU_NUMBER / MENU_SLIDER: the range; MENU_NUMBER: a format for the value (default "%d")
	int min, max;
	const char* format;
	// MENU_ACTION: what A does; others: called after the value changes
	void (*action)(const struct Menu_Item_s* item);
	// for get / set / action: which of several similar rows this is
	int tag;
	// what it does, in the status window while it's highlighted (wrapped to the window)
	const char* help;
} Menu_Item;

typedef struct Menu_Page_s {
	const char* title;
	const Menu_Item* items;
	int count;
	// the buttons for the status window's key line
	const window_key_t* keys;
	int key_count;
	// B on this page: the page to go back to (NULL: the main menu's own B does nothing)
	const struct Menu_Page_s* parent;
	// the rows' text size (0: the default)
	float text_scale;
	// draws more in the main window under the rows, in each list (NULL: nothing)
	void (*draw)(uint32 list);
	// takes the input before the rows do: nonzero if it used it (NULL: none)
	int (*input)(uint32 input);
	// the rows start this far right of the title (room for draw's picture)
	float rows_indent;
} Menu_Page;

/**
 * Shows a page in the main window (and its buttons in the status window's key line), with its first selectable row
 * highlighted
 */
void menu_open(const Menu_Page* page);
const Menu_Page* menu_current();
// the current page's own drawing (Menu_Page.draw), after the windows
void menu_draw_page(uint32 list);
// another screen (the ROM browser) has the windows: no page is current
void menu_close();

/**
 * Acts on the input (input.h's INPUT_* events) and refreshes the window's rows
 */
void menu_handle(uint32 input);

/**
 * Sets up the main and help windows' places and the colors from the options (after the colors change too)
 */
void menu_style_windows(const char* title, const window_key_t* keys, int key_count, float text_scale);

/**
 * Moves the highlight in a window of 'count' rows by 'delta' (page moves too), keeping it in view
 */
void menu_move(window_data_t* data, window_style_t* style, int delta);

#endif
