/*===================================================================*/
/*                                                                   */
/*  text_window.h : The menus' windows, their rows and key lines     */
/*                                                                   */
/*===================================================================*/

#ifndef __TEXT_WINDOW_H
#define __TEXT_WINDOW_H

#include <kos.h>
#include <string.h>
#include "font.h"

// a slider's knob's triangles, a rounded corner's, and the windows' corners' radius
#define CIRCLE_TRIANGLES 64
#define CORNER_TRIANGLES 16
#define CORNER_RADIUS 16

// how a window looks
typedef struct __attribute__ ((packed, aligned(4))) window_style_s {
	// the border, the header's and the rows' text sizes, how many rows show, whether there's a header
	float border_thickness;
	float header_text_scale;
	float text_scale;
	int max_items;
	int use_header;

	// colours
	uint32 border_color;
	uint32 inside_color;
	uint32 header_text_color;
	uint32 selected_text_color;
	uint32 selected_background_color;
	uint32 text_color;

	// the rows' left margin and the space between them
	float left_margin;
	float line_spacing;
	// the rows start this much further right than the header (room at the left for a picture)
	float rows_indent;
	// and stop this much short of the right (room for a picture)
	float rows_right;
	// (not used: kept so the struct stays as it is)
	int is_transparent;
	int is_animating;
	int animation_mode;
} window_style_t;

// a line in a window: plain text, a label with a check box, a label with a value at the right, or a slider
#define ROW_TEXT 0
#define ROW_CHECK 1
#define ROW_VALUE 2
#define ROW_SLIDER 3
// a label with a value at the right between a left and a right arrow: left / right change it
#define ROW_CHOICE 4

typedef struct window_row_s {
	int kind;
	const char* label;
	// ROW_VALUE: the text at the right
	const char* value;
	// ROW_CHECK: checked; ROW_SLIDER: the value between min and max
	int number;
	int min;
	int max;
} window_row_t;

// a controller button in a window's key line: drawn as the Dreamcast controller's own (A red, B blue, X yellow,
// Y green; Start, the D-pad and the triggers grey), followed by what it does
#define KEY_A 0
#define KEY_B 1
#define KEY_X 2
#define KEY_Y 3
#define KEY_START 4
#define KEY_DPAD 5
#define KEY_LT 6
#define KEY_RT 7

typedef struct window_key_s {
	int button;
	// what it does (NULL: nothing, the next button follows close by, as LT RT for one action)
	const char* label;
} window_key_t;

// what a window shows, and where
typedef struct __attribute__ ((packed, aligned(4))) window_data_s {
	// its place and size on the screen
	float x;
	float y;
	float width;
	float height;

	// the header's text
	char* header_text;
	// a line of controller buttons in place of the header text (num_keys 0: none)
	const window_key_t* keys;
	int num_keys;
	// the lines: rows if set, otherwise data_strings as plain text
	const window_row_t* rows;
	char** data_strings;

	// the first row showing, and how many rows there are
	int top_index;
	int num_strings;
	// the highlighted row
	int highlighted_index;

	// the rows' font, and the header's (NULL: the rows' font)
	Font* font;
	Font* header_font;

	// an icon at the left of each row (NULL: none): drawn in the translucent list at (x, y), size by size, with the
	// row's text icon_width further right
	void (*row_icon)(int index, float x, float y, float size);
	float icon_width;
} window_data_t;

/**
 * Draws a window, its header (or key line), its rows and their marks, and its scroll bar while it has more rows than
 * show, in the list's pass (call it in both)
 */
void win_draw_textwindow(window_data_t* windata, window_style_t* winstyle, uint32 list);

/**
 * A flat rounded box (its own polygon header), as the windows' layers are drawn
 */
void draw_cornered_layer(uint32 list, float x, float y, float z, float width, float height, float corner_radius, uint32 color);

/**
 * A line of controller buttons (icons in the opaque list, letters and labels in the translucent one) from (x, y), 20
 * pixels high
 */
void win_draw_keys(Font* font, const window_key_t* keys, int count, float x, float y, uint32 text_color, uint32 list);

/**
 * Draws the windows that follow this far in front of the others (0: as usual), e.g. a dialog over the screen
 */
void win_set_z_lift(float lift);
// the lists' scroll bar (SCROLL_BAR_WIDTH wide) from top to bottom at x: 'view' of 'total' shown, from 'position'
#define SCROLL_BAR_WIDTH 16.0f
void win_draw_scroll_bar(uint32 list, float x, float top, float bottom, float view, float total, float position);

#endif