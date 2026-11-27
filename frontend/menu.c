/*===================================================================*/
/*                                                                   */
/*  menu.c : Menu pages as data                                      */
/*                                                                   */
/*===================================================================*/

#include "menu.h"
#include "frontend.h"
#include "input.h"
#include "options.h"

#define MENU_MAX_ROWS 64
#define MENU_VALUE_LENGTH 24
#define MENU_ROWS_INDENT 8.0f

static const Menu_Page* current_page = NULL;
static window_row_t rows[MENU_MAX_ROWS];
static char values[MENU_MAX_ROWS][MENU_VALUE_LENGTH];

// the highlighted row's help, wrapped into the status window's lines
#define HELP_LINES 6
#define HELP_LINE_LENGTH 100
static char help_text[HELP_LINES][HELP_LINE_LENGTH];
static char* help_lines[HELP_LINES];

/*
 * The layout: the title on one line at the top, the main window across the screen below it, and the help / status
 * window across the screen at the bottom (32 px side margins)
 */
#define WINDOW_LEFT 32.0f
#define WINDOW_WIDTH 576.0f
#define MAIN_TOP 48.0f
#define MAIN_HEIGHT 292.0f
#define HELP_TOP 352.0f
#define HELP_HEIGHT 120.0f

void menu_style_windows(const char* title, const window_key_t* keys, int key_count, float text_scale) {
	mydata.x = WINDOW_LEFT;
	mydata.y = MAIN_TOP;
	mydata.width = WINDOW_WIDTH;
	mydata.height = MAIN_HEIGHT;
	// (the lines in the monospace font, the headers in Neuro)
	mydata.font = font_mono;
	mydata.header_font = font;
	mydata.header_text = (char*)title;
	mydata.keys = NULL;
	mydata.num_keys = 0;
	mydata.row_icon = NULL;
	mydata.icon_width = 0.0f;

	mystyle.header_text_scale = 1.0f;
	mystyle.text_scale = (text_scale > 0.0f) ? text_scale : 0.60f;
	mystyle.border_thickness = 5.0f;
	mystyle.border_color = options.GUI_OutsideWindowColor;
	mystyle.inside_color = options.GUI_InsideWindowColor;
	mystyle.left_margin = 15;
	mystyle.line_spacing = 0.0f;
	// (room at the rows' left for the focus marker to move in)
	mystyle.rows_indent = MENU_ROWS_INDENT;
	mystyle.rows_right = 0.0f;
	mystyle.header_text_color = options.GUI_TextColor;
	mystyle.text_color = options.GUI_TextColor;
	mystyle.max_items = (mydata.height - (mydata.header_font -> fontHeight * mystyle.header_text_scale) - (2 * CORNER_RADIUS)) / ((float)mydata.font -> fontHeight * mystyle.text_scale);
	mystyle.selected_text_color = options.GUI_SelectedTextColor;
	mystyle.selected_background_color = options.GUI_SelectedTextColor;

	helpdata.x = WINDOW_LEFT;
	helpdata.y = HELP_TOP;
	helpdata.width = WINDOW_WIDTH;
	helpdata.height = HELP_HEIGHT;
	helpdata.font = font_mono;
	helpdata.header_font = font;
	// the key line at the top, the status / info lines (set by the screen, e.g. the browser's ROM header) below it
	helpdata.header_text = NULL;
	helpdata.keys = keys;
	helpdata.num_keys = key_count;
	helpdata.rows = NULL;
	helpdata.data_strings = NULL;
	helpdata.num_strings = 0;
	// (nothing highlighted)
	helpdata.highlighted_index = -1;
	helpdata.top_index = 0;

	// (the key line's room at the top, in place of a header)
	helpstyle.header_text_scale = 0.65f;
	helpstyle.text_scale = 0.5f;
	helpstyle.border_thickness = 5.0f;
	helpstyle.border_color = options.GUI_OutsideWindowColor;
	helpstyle.inside_color = options.GUI_InsideWindowColor;
	helpstyle.left_margin = 15;
	helpstyle.line_spacing = 0.0f;
	helpstyle.rows_indent = 0.0f;
	helpstyle.rows_right = 0.0f;
	helpstyle.header_text_color = options.GUI_TextColor;
	helpstyle.text_color = options.GUI_TextColor;
	helpstyle.max_items = (helpdata.height - (helpdata.header_font -> fontHeight * helpstyle.header_text_scale) - CORNER_RADIUS) / ((float)helpdata.font -> fontHeight * helpstyle.text_scale);
	helpstyle.selected_text_color = options.GUI_SelectedTextColor;
	helpstyle.selected_background_color = options.GUI_SelectedTextColor;
}

void menu_move(window_data_t* data, window_style_t* style, int delta) {
	if (data -> num_strings <= 0) {
		return;
	}
	int index = data -> highlighted_index + delta;
	if (index < 0) {
		index = 0;
	} else if (index >= data -> num_strings) {
		index = data -> num_strings - 1;
	}
	data -> highlighted_index = index;
	if (index < data -> top_index) {
		data -> top_index = index;
	} else if (index >= data -> top_index + style -> max_items) {
		data -> top_index = index - style -> max_items + 1;
	}
}

static int item_selectable(const Menu_Item* item) {
	return item -> kind != MENU_TEXT;
}

static int item_get(const Menu_Item* item) {
	if (item -> get != NULL) {
		return item -> get(item);
	}
	return (item -> value != NULL) ? *item -> value : 0;
}

static void item_set(const Menu_Item* item, int value) {
	if (item -> set != NULL) {
		item -> set(item, value);
	} else if (item -> value != NULL) {
		*item -> value = (uint8)value;
	}
	if (item -> action != NULL) {
		item -> action(item);
	}
}

// the window's rows from the page's items and their values
static void menu_refresh() {
	const Menu_Page* page = current_page;
	int count = (page -> count < MENU_MAX_ROWS) ? page -> count : MENU_MAX_ROWS;
	for (int i = 0; i < count; i++) {
		const Menu_Item* item = &page -> items[i];
		window_row_t* row = &rows[i];
		memset(row, 0, sizeof(*row));
		row -> label = item -> label;
		switch (item -> kind) {
			case MENU_TOGGLE:
				row -> kind = ROW_CHECK;
				row -> number = item_get(item);
				break;
			case MENU_CHOICE: {
				int value = item_get(item);
				row -> kind = ROW_CHOICE;
				row -> value = ((value >= 0) && (value < item -> count)) ? item -> choices[value] : "?";
			} break;
			case MENU_NUMBER:
				row -> kind = ROW_CHOICE;
				snprintf(values[i], MENU_VALUE_LENGTH, item -> format ? item -> format : "%d", item_get(item));
				row -> value = values[i];
				break;
			case MENU_SLIDER:
				row -> kind = ROW_SLIDER;
				row -> number = item_get(item);
				row -> min = item -> min;
				row -> max = item -> max;
				break;
			default:
				row -> kind = ROW_TEXT;
				break;
		}
	}
	mydata.rows = rows;
	mydata.num_strings = count;
}

// help into the status window, a word at a time onto each line while it fits
static void menu_show_help() {
	const Menu_Page* page = current_page;
	int index = mydata.highlighted_index;
	// (a page without rows, a Markdown page, keeps the status window to itself)
	if ((page != NULL) && (page -> count == 0)) {
		return;
	}
	const char* text = ((page != NULL) && (index >= 0) && (index < page -> count)) ? page -> items[index].help : NULL;
	int lines = 0;
	float width = helpdata.width - (2 * helpstyle.left_margin) - (2 * helpstyle.border_thickness);
	int max_lines = (helpstyle.max_items < HELP_LINES) ? helpstyle.max_items : HELP_LINES;
	while ((text != NULL) && (*text != '\0') && (lines < max_lines)) {
		char* line = help_text[lines];
		size_t length = 0;
		line[0] = '\0';
		for (;;) {
			// the next word, and the spaces before it
			const char* end = text;
			while (*end == ' ') {
				end++;
			}
			while ((*end != ' ') && (*end != '\0') && (*end != '\n')) {
				end++;
			}
			size_t take = end - text;
			if ((take == 0) || (length + take >= HELP_LINE_LENGTH)) {
				break;
			}
			char trial[HELP_LINE_LENGTH];
			memcpy(trial, line, length);
			memcpy(trial + length, text, take);
			trial[length + take] = '\0';
			float measured;
			measureLine(helpdata.font, (length == 0) ? trial + strspn(trial, " ") : trial, helpstyle.text_scale, &measured);
			if ((length > 0) && (measured > width)) {
				break;
			}
			memcpy(line, trial, length + take + 1);
			length += take;
			text = end;
		}
		// (no leading spaces on a line; a newline starts the next)
		memmove(line, line + strspn(line, " "), strlen(line + strspn(line, " ")) + 1);
		help_lines[lines++] = line;
		while (*text == ' ') {
			text++;
		}
		if (*text == '\n') {
			text++;
		}
	}
	helpdata.data_strings = help_lines;
	helpdata.num_strings = lines;
	helpdata.top_index = 0;
}

// the options as they were when a page was opened from the main menu: back at the main menu, if they changed, they're
// saved to the VMU (as "Save options to VMU" does)
static Options_t options_on_entry;
static int options_entered = 0;

static void options_autosave(const Menu_Page* page) {
	if (page -> parent == NULL) {
		if (options_entered && (memcmp(&options, &options_on_entry, sizeof(Options_t)) != 0)) {
			printf("menu: options changed, saving to the VMU\n");
			save_options_to_VMU();
		}
		options_entered = 0;
	} else if ((current_page == NULL) || (current_page -> parent == NULL)) {
		memcpy(&options_on_entry, &options, sizeof(Options_t));
		options_entered = 1;
	}
}

void menu_open(const Menu_Page* page) {
	options_autosave(page);
	current_page = page;
	menu_style_windows(page -> title, page -> keys, page -> key_count, page -> text_scale);
	mystyle.rows_indent = MENU_ROWS_INDENT + page -> rows_indent;
	mydata.highlighted_index = 0;
	mydata.top_index = 0;
	menu_refresh();
	// the first row that does something (a page of text has nothing highlighted)
	mydata.highlighted_index = -1;
	for (int i = 0; i < mydata.num_strings; i++) {
		if (item_selectable(&page -> items[i])) {
			mydata.highlighted_index = i;
			break;
		}
	}
	menu_show_help();
}

const Menu_Page* menu_current() {
	return current_page;
}

void menu_draw_page(uint32 list) {
	if ((current_page != NULL) && (current_page -> draw != NULL)) {
		current_page -> draw(list);
	}
}

void menu_close() {
	current_page = NULL;
}

// up / down to the next selectable row (on a page with none, scrolls)
static void menu_step(int direction) {
	const Menu_Page* page = current_page;
	int index = mydata.highlighted_index;
	for (int i = index + direction; (index >= 0) && (i >= 0) && (i < mydata.num_strings); i += direction) {
		if (item_selectable(&page -> items[i])) {
			menu_move(&mydata, &mystyle, i - index);
			// (text rows above the first selectable one stay in view)
			return;
		}
	}
	// nothing selectable that way: scroll the text
	int top = mydata.top_index + direction;
	if ((top >= 0) && (top + mystyle.max_items <= mydata.num_strings)) {
		mydata.top_index = top;
	}
}

static void menu_change(const Menu_Item* item, int direction) {
	int value = item_get(item);
	switch (item -> kind) {
		case MENU_TOGGLE:
			item_set(item, !value);
			break;
		case MENU_CHOICE:
			value += direction;
			if (value < 0) {
				value = item -> count - 1;
			} else if (value >= item -> count) {
				value = 0;
			}
			item_set(item, value);
			break;
		case MENU_NUMBER:
		case MENU_SLIDER:
			value += direction;
			if (value < item -> min) {
				value = item -> min;
			} else if (value > item -> max) {
				value = item -> max;
			}
			item_set(item, value);
			break;
		default:
			break;
	}
}

void menu_handle(uint32 input) {
	const Menu_Page* page = current_page;
	if (page == NULL) {
		return;
	}
	// (the page's own keys first: a page that takes the input may open another)
	if ((page -> input != NULL) && page -> input(input)) {
		if (current_page == page) {
			int highlighted = mydata.highlighted_index, top = mydata.top_index;
			menu_refresh();
			mydata.highlighted_index = highlighted;
			mydata.top_index = top;
			menu_show_help();
		}
		return;
	}
	if (input & INPUT_UP) {
		menu_step(-1);
	}
	if (input & INPUT_DOWN) {
		menu_step(1);
	}
	if ((input & (INPUT_PAGE_UP | INPUT_PAGE_DOWN)) && (mydata.highlighted_index >= 0)) {
		menu_move(&mydata, &mystyle, (input & INPUT_PAGE_UP) ? -mystyle.max_items : mystyle.max_items);
		// (onto a selectable row)
		if (!item_selectable(&page -> items[mydata.highlighted_index])) {
			menu_step((input & INPUT_PAGE_UP) ? 1 : -1);
		}
	}
	int index = mydata.highlighted_index;
	const Menu_Item* item = ((index >= 0) && (index < page -> count)) ? &page -> items[index] : NULL;
	if (item != NULL) {
		// sliders move faster: 255 steps one at a time is a long way
		int step = (item -> kind == MENU_SLIDER) ? 4 : 1;
		if (input & INPUT_LEFT) {
			menu_change(item, -step);
		}
		if (input & INPUT_RIGHT) {
			menu_change(item, step);
		}
		if (input & INPUT_ACCEPT) {
			if (item -> kind == MENU_ACTION) {
				if (item -> action != NULL) {
					item -> action(item);
				}
			} else if ((item -> kind == MENU_TOGGLE) || (item -> kind == MENU_CHOICE)) {
				menu_change(item, 1);
			}
		}
	}
	if ((input & INPUT_BACK) && (current_page == page) && (page -> parent != NULL)) {
		menu_open(page -> parent);
		return;
	}
	// (an action may have opened another page, or another screen)
	if (current_page == page) {
		int highlighted = mydata.highlighted_index, top = mydata.top_index;
		menu_refresh();
		mydata.highlighted_index = highlighted;
		mydata.top_index = top;
		menu_show_help();
	}
}
