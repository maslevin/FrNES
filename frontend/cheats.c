/*===================================================================*/
/*                                                                   */
/*  cheats.c : Game Genie codes for a game (over the browser)        */
/*                                                                   */
/*===================================================================*/

#include "cheats.h"
#include "frontend.h"
#include "input.h"
#include "menu.h"
#include "options.h"
#include "text_window.h"
#include "game_name.h"

#include <ctype.h>
#include <zlib/zlib.h>
#include <strings.h>

// the window: the main window's place, in front of the screen (as a dialog is)
#define CHEATS_Z_LIFT 20.0f
#define CHEATS_TEXT_SCALE 0.5f
#define MAX_LIBRARY 128
// the codes that came with FrNES: a line each, "game<TAB>codes<TAB>what they do" (codes joined by '+', none for a
// note), made by tools/gg_codes.py from a compilation of the Game Genie's own code books
#define BUILT_IN_CODES "/rd/cheats/gg_nes.txt.gz"
#define LIBRARY_TEXT_LENGTH 100
#define MAX_ROWS (CHEAT_SLOTS + 4 + MAX_LIBRARY)
#define ROW_LENGTH 72

// the Game Genie's letters, in the order of their values (0-15)
static const char gg_letters[17] = "APZLGITYEOXUKSVN";

static int active = 0;
static char rom_name[64];

// the game's codes: from its own list, then FrNES's; each one to three codes (a cheat that needs several), or none
// for a note
static struct {
	char codes[CHEAT_SLOTS][CHEAT_CODE_LENGTH + 1];
	int count;
	char text[LIBRARY_TEXT_LENGTH + 1];
} library[MAX_LIBRARY];
static int library_count = 0;
static char library_from[96];

// the rows: the slots, a gap, "Enter a code", a gap, the list's heading, its codes
#define ROW_ENTER (CHEAT_SLOTS + 1)
#define ROW_LIBRARY (CHEAT_SLOTS + 4)
static window_data_t data;
static window_style_t style;
static window_row_t rows[MAX_ROWS];
static char row_text[MAX_ROWS][ROW_LENGTH];
static char help_line[96];
// (a message in the help line, kept over the next update)
static int keep_help = 0;

static const window_key_t cheats_keys[] = {
	{ KEY_A, "On / off / add" },
	{ KEY_X, "Type" },
	{ KEY_Y, "Clear" },
	{ KEY_B, "Back" }
};

// the keyboard: the 16 letters in two rows, then Delete and Done
static int keyboard = 0;
static int keyboard_slot = 0;
static int key_index = 0;
static char typed[CHEAT_CODE_LENGTH + 1];
#define KEY_DELETE 16
#define KEY_DONE 17
#define KEYBOARD_KEYS 18

static const window_key_t keyboard_keys[] = {
	{ KEY_DPAD, "Move" },
	{ KEY_A, "Press" },
	{ KEY_B, "Delete" },
	{ KEY_START, "Done" }
};

static int letter_value(char c) {
	const char* found = strchr(gg_letters, toupper((unsigned char)c));
	return ((found != NULL) && (c != '\0')) ? (int)(found - gg_letters) : -1;
}

int cheats_decode(const char* code, uint16* address, uint8* value, int* compare) {
	size_t length = strlen(code);
	if ((length != 6) && (length != 8)) {
		return -1;
	}
	int n[8];
	for (size_t i = 0; i < length; i++) {
		n[i] = letter_value(code[i]);
		if (n[i] < 0) {
			return -1;
		}
	}
	// (the Game Genie's scrambling of address and value bits across the letters, as the nesdev wiki has it)
	*address = 0x8000 | ((n[3] & 7) << 12) | ((n[5] & 7) << 8) | ((n[4] & 8) << 8) | ((n[2] & 7) << 4) | ((n[1] & 8) << 4) |
		(n[4] & 7) | (n[3] & 8);
	if (length == 6) {
		*value = ((n[1] & 7) << 4) | ((n[0] & 8) << 4) | (n[0] & 7) | (n[5] & 8);
		*compare = -1;
	} else {
		*value = ((n[1] & 7) << 4) | ((n[0] & 8) << 4) | (n[0] & 7) | (n[7] & 8);
		*compare = ((n[7] & 7) << 4) | ((n[6] & 8) << 4) | (n[6] & 7) | (n[5] & 8);
	}
	return 0;
}

// a list line's codes ("CODE+CODE") and text into the library; 0, or -1 if it isn't a list of codes
static int library_add(const char* codes, size_t codes_length, const char* text) {
	if (library_count >= MAX_LIBRARY) {
		return -1;
	}
	int count = 0;
	const char* code = codes;
	while ((code < codes + codes_length) && (count < CHEAT_SLOTS)) {
		size_t length = strcspn(code, "+ \t");
		if (length > (size_t)(codes + codes_length - code)) {
			length = codes + codes_length - code;
		}
		char one[CHEAT_CODE_LENGTH + 1];
		if ((length != 6) && (length != 8)) {
			return -1;
		}
		for (size_t i = 0; i < length; i++) {
			one[i] = toupper((unsigned char)code[i]);
		}
		one[length] = '\0';
		uint16 address;
		uint8 value;
		int compare;
		if (cheats_decode(one, &address, &value, &compare) != 0) {
			return -1;
		}
		snprintf(library[library_count].codes[count++], CHEAT_CODE_LENGTH + 1, "%s", one);
		code += length;
		while ((*code == '+') || (*code == ' ')) {
			code++;
		}
	}
	library[library_count].count = count;
	snprintf(library[library_count].text, LIBRARY_TEXT_LENGTH + 1, "%s", text);
	library_count++;
	return 0;
}

// the game's own list: beside it, or in /sd/FrNES/cheats ("CODE[+CODE...] what it does" a line, '#' for comments)
static void library_read_own(const char* rom_path) {
	char beside[OPTIONS_PATH_LENGTH + 16], on_sd[OPTIONS_PATH_LENGTH + 16];
	snprintf(beside, sizeof(beside), "%s", rom_path);
	char* extension = strrchr(beside, '.');
	if (extension != NULL) {
		*extension = '\0';
	}
	strncat(beside, ".cheats", sizeof(beside) - strlen(beside) - 1);
	snprintf(on_sd, sizeof(on_sd), "/sd/FrNES/cheats/%s.cheats", rom_name);
	const char* places[2] = { beside, on_sd };
	file_t handle = -1;
	for (int i = 0; (i < 2) && (handle == -1); i++) {
		handle = fs_open(places[i], O_RDONLY);
		if (handle != -1) {
			snprintf(library_from, sizeof(library_from), "%s", places[i]);
		}
	}
	if (handle == -1) {
		return;
	}
	char buffer[4096];
	int got = fs_read(handle, buffer, sizeof(buffer) - 1);
	fs_close(handle);
	if (got <= 0) {
		return;
	}
	buffer[got] = '\0';
	for (char* line = strtok(buffer, "\r\n"); line != NULL; line = strtok(NULL, "\r\n")) {
		while ((*line == ' ') || (*line == '\t')) {
			line++;
		}
		if ((*line == '#') || (*line == '\0')) {
			continue;
		}
		size_t codes_length = strcspn(line, " \t");
		const char* text = line + codes_length;
		while ((*text == ' ') || (*text == '\t')) {
			text++;
		}
		library_add(line, codes_length, text);
	}
}

// FrNES's own codes for the game, if its name is in the list
static void library_read_built_in() {
	// (the ROM's name whole and up to a subtitle, against each game's both ways)
	char rom_key[96], rom_title[96];
	name_key(rom_name, rom_key, sizeof(rom_key), 0);
	name_key(rom_name, rom_title, sizeof(rom_title), 1);
	gzFile list = gzopen(BUILT_IN_CODES, "rb");
	if (list == NULL) {
		return;
	}
	char line[512], game_key[96], game_title[96], last_game[128] = "";
	int matching = 0;
	while (gzgets(list, line, sizeof(line)) != NULL) {
		line[strcspn(line, "\r\n")] = '\0';
		char* codes = strchr(line, '\t');
		if (codes == NULL) {
			continue;
		}
		*codes++ = '\0';
		char* text = strchr(codes, '\t');
		if (text == NULL) {
			continue;
		}
		*text++ = '\0';
		// (the lines run game by game: each game's name looked at once)
		if (strcmp(line, last_game) != 0) {
			snprintf(last_game, sizeof(last_game), "%s", line);
			name_key(line, game_key, sizeof(game_key), 0);
			name_key(line, game_title, sizeof(game_title), 1);
			matching = game_key[0] && ((strcmp(game_key, rom_key) == 0) || (strcmp(game_key, rom_title) == 0) ||
				(strcmp(game_title, rom_key) == 0) || (strcmp(game_title, rom_title) == 0));
			if (matching && (library_from[0] == '\0')) {
				snprintf(library_from, sizeof(library_from), "the Game Genie code book (%s)", line);
			}
		}
		if (!matching) {
			continue;
		}
		if (codes[0] == '\0') {
			// (a note: kept as a line of its own, with no codes)
			if (library_count < MAX_LIBRARY) {
				library[library_count].count = 0;
				snprintf(library[library_count].text, LIBRARY_TEXT_LENGTH + 1, "%s", text);
				library_count++;
			}
		} else {
			library_add(codes, strlen(codes), text);
		}
	}
	gzclose(list);
}

static void library_read(const char* rom_path) {
	library_count = 0;
	library_from[0] = '\0';
	library_read_own(rom_path);
	library_read_built_in();
}

// the rows from the slots and the list
static void rows_update() {
	int count = 0;
	for (int slot = 0; slot < CHEAT_SLOTS; slot++, count++) {
		window_row_t* row = &rows[count];
		memset(row, 0, sizeof(*row));
		if (options.cheat_codes[slot][0]) {
			snprintf(row_text[count], ROW_LENGTH, "%d  %-8s  %s", slot + 1, options.cheat_codes[slot], options.cheat_texts[slot]);
			row -> kind = ROW_CHECK;
			row -> number = options.cheat_on[slot];
		} else {
			snprintf(row_text[count], ROW_LENGTH, "%d  (empty)", slot + 1);
			row -> kind = ROW_TEXT;
		}
		row -> label = row_text[count];
	}
	memset(&rows[count++], 0, sizeof(window_row_t));
	memset(&rows[count], 0, sizeof(window_row_t));
	rows[count].label = "Type a code";
	count++;
	memset(&rows[count++], 0, sizeof(window_row_t));
	memset(&rows[count], 0, sizeof(window_row_t));
	if (library_count > 0) {
		snprintf(row_text[count], ROW_LENGTH, "Codes from %s:", library_from);
	} else {
		snprintf(row_text[count], ROW_LENGTH, "No list of codes for this game (see Help)");
	}
	rows[count].label = row_text[count];
	count++;
	for (int i = 0; i < library_count; i++, count++) {
		memset(&rows[count], 0, sizeof(window_row_t));
		if (library[i].count == 0) {
			snprintf(row_text[count], ROW_LENGTH, "  (%s)", library[i].text);
		} else if (library[i].count == 1) {
			snprintf(row_text[count], ROW_LENGTH, "%-8s  %s", library[i].codes[0], library[i].text);
		} else {
			snprintf(row_text[count], ROW_LENGTH, "%-8s+%d %s", library[i].codes[0], library[i].count - 1, library[i].text);
		}
		rows[count].label = row_text[count];
	}
	data.rows = rows;
	data.num_strings = count;
}

static int row_selectable(int row) {
	return (row < CHEAT_SLOTS) || (row == ROW_ENTER) ||
		((row >= ROW_LIBRARY) && (row < ROW_LIBRARY + library_count) && (library[row - ROW_LIBRARY].count > 0));
}

// the highlighted code, decoded, under the rows
static void help_update() {
	if (keep_help) {
		keep_help = 0;
		return;
	}
	int row = data.highlighted_index;
	const char* code = NULL;
	if ((row >= 0) && (row < CHEAT_SLOTS)) {
		code = options.cheat_codes[row];
	} else if ((row >= ROW_LIBRARY) && (row < ROW_LIBRARY + library_count)) {
		// (a cheat of several codes: all of them, and the slots it takes)
		if (library[row - ROW_LIBRARY].count > 1) {
			char* end = help_line;
			for (int i = 0; i < library[row - ROW_LIBRARY].count; i++) {
				end += sprintf(end, "%s%s", i ? " + " : "", library[row - ROW_LIBRARY].codes[i]);
			}
			sprintf(end, ": takes %d slots", library[row - ROW_LIBRARY].count);
			return;
		}
		code = library[row - ROW_LIBRARY].codes[0];
	}
	uint16 address;
	uint8 value;
	int compare;
	if ((code != NULL) && code[0] && (cheats_decode(code, &address, &value, &compare) == 0)) {
		if (compare < 0) {
			snprintf(help_line, sizeof(help_line), "%s: reads of $%04X give $%02X", code, address, value);
		} else {
			snprintf(help_line, sizeof(help_line), "%s: reads of $%04X give $%02X where the game has $%02X", code, address, value, compare);
		}
	} else if (row == ROW_ENTER) {
		snprintf(help_line, sizeof(help_line), "Type a code on the Game Genie's letters into the first empty slot");
	} else {
		help_line[0] = '\0';
	}
}

void cheats_open(const char* rom_path) {
	// (the game's name: the file's, without its directory or extension)
	const char* name = strrchr(rom_path, '/');
	snprintf(rom_name, sizeof(rom_name), "%s", name ? name + 1 : rom_path);
	char* extension = strrchr(rom_name, '.');
	if (extension != NULL) {
		*extension = '\0';
	}
	// (another game's codes make way)
	if (strcmp(options.cheat_rom, rom_path) != 0) {
		snprintf(options.cheat_rom, sizeof(options.cheat_rom), "%s", rom_path);
		memset(options.cheat_codes, 0, sizeof(options.cheat_codes));
		memset(options.cheat_texts, 0, sizeof(options.cheat_texts));
		memset(options.cheat_on, 0, sizeof(options.cheat_on));
	}
	library_read(rom_path);

	memset(&data, 0, sizeof(data));
	data.x = mydata.x;
	data.y = mydata.y;
	data.width = mydata.width;
	data.height = mydata.height;
	data.font = font_mono;
	data.header_font = font;
	static char title[80];
	snprintf(title, sizeof(title), "Cheats: %s", rom_name);
	data.header_text = title;
	style = mystyle;
	style.text_scale = CHEATS_TEXT_SCALE;
	style.rows_indent = 8.0f;
	// (room under the rows for the decoded code and the keys)
	style.max_items = (int)((data.height - 32.0f - 2 * CORNER_RADIUS - 44.0f) / (font_mono -> fontHeight * CHEATS_TEXT_SCALE));
	rows_update();
	data.highlighted_index = 0;
	data.top_index = 0;
	keyboard = 0;
	help_update();
	active = 1;
}

int cheats_active() {
	return active;
}

static int first_empty_slot() {
	for (int slot = 0; slot < CHEAT_SLOTS; slot++) {
		if (!options.cheat_codes[slot][0]) {
			return slot;
		}
	}
	return CHEAT_SLOTS - 1;
}

static void slot_set(int slot, const char* code, const char* text) {
	snprintf(options.cheat_codes[slot], sizeof(options.cheat_codes[0]), "%s", code);
	snprintf(options.cheat_texts[slot], sizeof(options.cheat_texts[0]), "%s", text);
	options.cheat_on[slot] = code[0] != '\0';
}

// a cheat into the empty slots (all its codes: one that needs more slots than are empty doesn't go in)
static void library_choose(int index) {
	int empty = 0;
	for (int slot = 0; slot < CHEAT_SLOTS; slot++) {
		empty += !options.cheat_codes[slot][0];
	}
	if (empty < library[index].count) {
		snprintf(help_line, sizeof(help_line), "This needs %d empty slots: Y clears a slot", library[index].count);
		keep_help = 1;
		return;
	}
	for (int i = 0; i < library[index].count; i++) {
		slot_set(first_empty_slot(), library[index].codes[i], library[index].text);
	}
}

static void keyboard_open(int slot) {
	keyboard = 1;
	keyboard_slot = slot;
	key_index = 0;
	snprintf(typed, sizeof(typed), "%s", options.cheat_codes[slot]);
}

static void keyboard_input(uint32 input) {
	int column = key_index % 9, row = key_index / 9;
	if (input & INPUT_LEFT) column = (column + 8) % 9;
	if (input & INPUT_RIGHT) column = (column + 1) % 9;
	if (input & (INPUT_UP | INPUT_DOWN)) row = 1 - row;
	// (the grid: 8 letters and Delete on the top row, 8 letters and Done below)
	key_index = row * 9 + column;
	size_t length = strlen(typed);
	int done = (input & INPUT_START) != 0;
	if (input & INPUT_ACCEPT) {
		if (column == 8) {
			if (row == 0) {
				if (length > 0) {
					typed[length - 1] = '\0';
				}
			} else {
				done = 1;
			}
		} else if (length < CHEAT_CODE_LENGTH) {
			typed[length] = gg_letters[row * 8 + column];
			typed[length + 1] = '\0';
		}
	}
	if (input & INPUT_BACK) {
		if (length > 0) {
			typed[length - 1] = '\0';
		} else {
			keyboard = 0;
		}
	}
	if (done) {
		uint16 address;
		uint8 value;
		int compare;
		if (typed[0] == '\0') {
			slot_set(keyboard_slot, "", "");
			keyboard = 0;
		} else if (cheats_decode(typed, &address, &value, &compare) == 0) {
			slot_set(keyboard_slot, typed, "Typed in");
			keyboard = 0;
		}
		// (a code that isn't 6 or 8 letters stays open to finish)
		rows_update();
		help_update();
	}
}

int cheats_input(uint32 input) {
	if (!active) {
		return 0;
	}
	if (keyboard) {
		keyboard_input(input);
		return 1;
	}
	if (input & INPUT_BACK) {
		active = 0;
		return 1;
	}
	int row = data.highlighted_index;
	int step = (input & INPUT_UP) ? -1 : ((input & INPUT_DOWN) ? 1 : 0);
	for (int next = row + step; step && (next >= 0) && (next < data.num_strings); next += step) {
		if (row_selectable(next)) {
			menu_move(&data, &style, next - row);
			break;
		}
	}
	row = data.highlighted_index;
	if (input & INPUT_ACCEPT) {
		if (row < CHEAT_SLOTS) {
			if (options.cheat_codes[row][0]) {
				options.cheat_on[row] = !options.cheat_on[row];
			} else {
				keyboard_open(row);
			}
		} else if (row == ROW_ENTER) {
			keyboard_open(first_empty_slot());
		} else if ((row >= ROW_LIBRARY) && (row < ROW_LIBRARY + library_count)) {
			library_choose(row - ROW_LIBRARY);
		}
	}
	if ((input & INPUT_X) && (row < CHEAT_SLOTS)) {
		keyboard_open(row);
	}
	if ((input & INPUT_Y) && (row < CHEAT_SLOTS)) {
		slot_set(row, "", "");
	}
	rows_update();
	help_update();
	return 1;
}

static void draw_keyboard(uint32 list) {
	// a window in front of the cheats window: the code so far, the letters, Delete and Done
	float width = 9 * 44.0f + 40.0f, height = 170.0f;
	float x = (640.0f - width) / 2, y = 150.0f;
	win_set_z_lift(CHEATS_Z_LIFT + 15.0f);
	if (list == PVR_LIST_OP_POLY) {
		draw_cornered_layer(list, x, y, 65.0f, width, height, 16.0f, options.GUI_OutsideWindowColor);
		draw_cornered_layer(list, x + 5.0f, y + 5.0f, 65.1f, width - 10.0f, height - 10.0f, 11.0f, options.GUI_InsideWindowColor);
		for (int key = 0; key < KEYBOARD_KEYS; key++) {
			int row = key / 9, column = key % 9;
			float kx = x + 20.0f + column * 44.0f, ky = y + 62.0f + row * 40.0f;
			uint32 color = (key == key_index) ? options.GUI_SelectedTextColor : options.GUI_OutsideWindowColor;
			draw_cornered_layer(list, kx, ky, 65.5f, 38.0f, 32.0f, 6.0f, color);
		}
	} else {
		char shown[32];
		snprintf(shown, sizeof(shown), "Slot %d: %s_", keyboard_slot + 1, typed);
		draw_string(font_mono, list, shown, x + 20.0f, y + 18.0f, 66.0f, width - 40.0f, 30.0f, SINGLE, LEFT, options.GUI_TextColor, 0.7f);
		for (int key = 0; key < KEYBOARD_KEYS; key++) {
			int row = key / 9, column = key % 9;
			float kx = x + 20.0f + column * 44.0f, ky = y + 62.0f + row * 40.0f;
			char label[4];
			if (column == 8) {
				snprintf(label, sizeof(label), "%s", row ? "OK" : "Del");
			} else {
				snprintf(label, sizeof(label), "%c", gg_letters[row * 8 + column]);
			}
			float scale = (column == 8) ? 0.45f : 0.6f, label_width;
			measureLine(font_mono, label, scale, &label_width);
			uint32 color = (key == key_index) ? options.GUI_InsideWindowColor : options.GUI_TextColor;
			draw_string(font_mono, list, label, kx + (38.0f - label_width) / 2, ky + (32.0f - font_mono -> fontHeight * scale) / 2, 66.0f,
				label_width + 2.0f, 32.0f, SINGLE, LEFT, color, scale);
		}
	}
	win_draw_keys(font_mono, keyboard_keys, 4, x + 20.0f, y + height - 30.0f, options.GUI_TextColor, list);
	win_set_z_lift(0.0f);
}

void cheats_draw(uint32 list) {
	if (!active) {
		return;
	}
	win_set_z_lift(CHEATS_Z_LIFT);
	win_draw_textwindow(&data, &style, list);
	float bottom = data.y + data.height;
	if (list == PVR_LIST_TR_POLY) {
		draw_string(font_mono, list, help_line, data.x + 15.0f, bottom - 50.0f, 55.0f, data.width - 30.0f, 20.0f, SINGLE, LEFT,
			options.GUI_TextColor, CHEATS_TEXT_SCALE);
	}
	win_draw_keys(font_mono, cheats_keys, 4, data.x + 15.0f, bottom - 30.0f, options.GUI_TextColor, list);
	win_set_z_lift(0.0f);
	if (keyboard) {
		draw_keyboard(list);
	}
}
