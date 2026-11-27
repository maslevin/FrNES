/*===================================================================*/
/*                                                                   */
/*  browser.c : The ROM browser                                      */
/*                                                                   */
/*  Lists a directory at a time, folders first, then .nes, .fds and */
/*  .nsf files (NSF: shown, not played yet)                         */
/*  files, each group in name order. The help window shows what the */
/*  highlighted ROM's header says.                                   */
/*                                                                   */
/*===================================================================*/

#include <errno.h>
#include "browser.h"
#include "frontend.h"
#include "input.h"
#include "menu.h"
#include "options.h"
#include "rom_info.h"
#include "dialog.h"
#include "image.h"
#include "cheats.h"
#include "art.h"
#include "viewer.h"
#include "movies.h"

#include "../emulator/sd_card.h"
#include "../emulator/cartridge.h"
#include <dc/fs_dcload.h>
#include <dc/dcload.h>
#include <strings.h>
#include <time.h>

typedef struct {
	char* name;
	uint8 is_directory;
	uint8 is_disk;
	uint8 is_nsf;
	// a movie (FrNES .frm, FCEUX .fm2, BizHawk .bk2): played back over its game, once the emulator has a playback mode
	uint8 is_movie;
	// the "Try again" row a read error leaves (browser_listed): A lists the folder again
	uint8 is_retry;
	uint32 size;
} Browser_Entry;

// the device list, then a directory's entries (directory == "" for the device list)
static char directory[OPTIONS_PATH_LENGTH] = "";
static Browser_Entry* entries = NULL;
static int entry_count = 0, entry_capacity = 0;
static window_row_t* rows = NULL;

typedef struct {
	const char* path;
	const char* label;
} Browser_Device;

static const Browser_Device devices[] = {
	{ "/cd", "CD-ROM" },
	{ "/sd", "SD card" },
	{ "/pc", "PC (dcload)" },
	{ "/rd", "Romdisk" }
};
#define NUM_DEVICES (sizeof(devices) / sizeof(devices[0]))
static int device_present[NUM_DEVICES];

static const window_key_t browser_keys[] = {
	{ KEY_DPAD, "Move" },
	{ KEY_A, "Play" },
	{ KEY_X, "Cheats" },
	{ KEY_Y, "Box / manual" },
	{ KEY_B, "Back" }
};
#define NUM_BROWSER_KEYS 5
// (with an SD card: START records the next game into a movie there. The triggers page through the list: not shown, the
// line has no room)
static const window_key_t browser_keys_sd[] = {
	{ KEY_DPAD, "Move" },
	{ KEY_A, "Play" },
	{ KEY_START, "Record" },
	{ KEY_X, "Cheats" },
	{ KEY_Y, "Box / manual" },
	{ KEY_B, "Back" }
};
#define NUM_BROWSER_KEYS_SD 6

static const window_key_t listing_keys[] = {
	{ KEY_B, "Stop" }
};

// the status window (below its key line): a status line, then the highlighted ROM's header
#define INFO_LINES 6
#define INFO_LENGTH ROM_INFO_LENGTH
static char info[INFO_LINES][INFO_LENGTH];
static char* info_lines[INFO_LINES];
static int info_index = -1;

// the highlighted game's art (art.h): looked up once the highlight has stayed on it a moment (scrolling past doesn't
// read every game's), its box shown small at the status window's right
#define ART_DELAY_MS 300
#define THUMB_WIDTH 58.0f
#define THUMB_HEIGHT 80.0f
static int art_index = -1;
static int art_looked = 1;
static uint64 art_since = 0;
static int art_found = 0;
static Art_Game art;
static Art_Texture thumb;
static char status[48] = "";

/*-------------------------------------------------------------------*/
/*  Listing                                                          */
/*-------------------------------------------------------------------*/
static void entries_clear() {
	for (int i = 0; i < entry_count; i++) {
		free(entries[i].name);
	}
	entry_count = 0;
}

static int entries_add(const char* name, int is_directory, uint32 size) {
	if (entry_count == entry_capacity) {
		int capacity = entry_capacity ? entry_capacity * 2 : 64;
		Browser_Entry* grown = realloc(entries, capacity * sizeof(Browser_Entry));
		window_row_t* grown_rows = realloc(rows, (capacity + 1) * sizeof(window_row_t));
		if (grown != NULL) {
			entries = grown;
		}
		if (grown_rows != NULL) {
			rows = grown_rows;
		}
		if ((grown == NULL) || (grown_rows == NULL)) {
			return 0;
		}
		entry_capacity = capacity;
	}
	Browser_Entry* entry = &entries[entry_count];
	entry -> name = strdup(name);
	if (entry -> name == NULL) {
		return 0;
	}
	entry -> is_directory = is_directory;
	entry -> is_retry = 0;
	const char* extension = strrchr(name, '.');
	entry -> is_disk = (extension != NULL) && (strcasecmp(extension, ".fds") == 0);
	entry -> is_nsf = (extension != NULL) && (strcasecmp(extension, ".nsf") == 0);
	entry -> is_movie = (extension != NULL) && ((strcasecmp(extension, ".frm") == 0) || (strcasecmp(extension, ".fm2") == 0) ||
		(strcasecmp(extension, ".bk2") == 0));
	entry -> size = size;
	entry_count++;
	return 1;
}

static int is_rom_file(const char* name) {
	const char* extension = strrchr(name, '.');
	return (extension != NULL) && ((strcasecmp(extension, ".nes") == 0) || (strcasecmp(extension, ".fds") == 0) ||
		(strcasecmp(extension, ".nsf") == 0) || (strcasecmp(extension, ".frm") == 0) || (strcasecmp(extension, ".fm2") == 0) ||
		(strcasecmp(extension, ".bk2") == 0));
}

static int entry_compare(const void* a, const void* b) {
	const Browser_Entry* x = (const Browser_Entry*)a;
	const Browser_Entry* y = (const Browser_Entry*)b;
	if (x -> is_directory != y -> is_directory) {
		return y -> is_directory - x -> is_directory;
	}
	return strcasecmp(x -> name, y -> name);
}

/*
 * Listing a directory takes a while over dc-load-ip (each entry is a network round trip, ~25 ms; a 6000 file folder
 * is minutes), so it's done a little each frame, with the count shown and B to give up. Under /pc the directory is
 * read with dc-load's own calls, whose entries say whether they're directories: KOS's readdir asks dc-tool for every
 * entry's stat as well, a second round trip each.
 */
#define LIST_FRAME_MS 12

static struct {
	int active;
	// dc-load's handle (under /pc), or KOS's
	int dcload;
	int handle;
	// the row to highlight when it's done: this name, or else this index
	char select_name[OPTIONS_PATH_LENGTH];
	int select_index;
	// a read that failed partway (KOS's FAT driver's readdir gives NULL with errno set, as at the end with it clear)
	int read_error;
} listing;

static int under_dcload(const char* path) {
	return (dcload_type != DCLOAD_TYPE_NONE) && (strncmp(path, "/pc", 3) == 0) && ((path[3] == '\0') || (path[3] == '/'));
}

static void browser_list_end() {
	if (listing.dcload) {
		dcload_closedir(listing.handle);
	} else {
		fs_close(listing.handle);
	}
	listing.active = 0;
	qsort(entries, entry_count, sizeof(Browser_Entry), entry_compare);
}

// starts listing 'directory' ("" for the devices: done at once); 0 if it can't be read
static int browser_list_begin() {
	entries_clear();
	listing.active = 0;
	listing.read_error = 0;
	if (directory[0] == '\0') {
		for (uint32 i = 0; i < NUM_DEVICES; i++) {
			if (device_present[i]) {
				entries_add(devices[i].path, 1, 0);
			}
		}
		return 1;
	}
	listing.dcload = under_dcload(directory);
	if (listing.dcload) {
		listing.handle = dcload_opendir(directory[3] ? &directory[3] : "/");
		if (listing.handle == 0) {
			printf("browser: unable to open [%s]\n", directory);
			return 0;
		}
	} else {
		listing.handle = fs_open(directory, O_DIR | O_RDONLY);
		if (listing.handle == -1) {
			printf("browser: unable to open [%s]\n", directory);
			return 0;
		}
	}
	listing.active = 1;
	return 1;
}

// reads entries for up to LIST_FRAME_MS; 1 when the directory is done
static int browser_list_step() {
	uint64 start = timer_ms_gettime64();
	while (listing.active && (timer_ms_gettime64() - start < LIST_FRAME_MS)) {
		const char* name;
		int is_directory;
		uint32 size = 0;
		if (listing.dcload) {
			struct dirent* entry = dcload_readdir(listing.handle);
			if (entry == NULL) {
				browser_list_end();
				break;
			}
			name = entry -> d_name;
			// (hidden: ".", "..", and macOS's "._" AppleDouble files, 4 kB companions of every file it copies to FAT)
			// (KOS's FAT driver also returns a directory with an empty name, one whose long name it can't convert:
			// it read as "." and opened its own directory again)
			if ((name[0] == '.') || (name[0] == '\0')) {
				continue;
			}
			// (4 DT_DIR, 8 DT_REG; a link or an unknown type takes a stat)
			if (entry -> d_type == 4) {
				is_directory = 1;
			} else if (entry -> d_type == 8) {
				is_directory = 0;
			} else {
				char path[OPTIONS_PATH_LENGTH + 260];
				struct stat st;
				snprintf(path, sizeof(path), "%s/%s", directory, name);
				is_directory = (fs_stat(path, &st, 0) == 0) && S_ISDIR(st.st_mode);
			}
		} else {
			errno = 0;
			dirent_t* entry = fs_readdir(listing.handle);
			if (entry == NULL) {
				// (the end of the directory, or a read that failed: an SD card's block that didn't come back, which
				// would otherwise show as an empty folder)
				if (errno != 0) {
					printf("browser: error %d reading [%s] after %d entries\n", errno, directory, entry_count);
					listing.read_error = 1;
				}
				browser_list_end();
				break;
			}
			name = entry -> name;
#ifdef FRNES_BROWSER_TRACE
			{
				char hex[64] = "";
				for (int i = 0; name[i] && (i < 16); i++) {
					snprintf(hex + strlen(hex), sizeof(hex) - strlen(hex), "%02x", (uint8)name[i]);
				}
				printf("browser: [%s] entry [%s] (%s) attr %lx size %ld\n", directory, name, hex, (unsigned long)entry -> attr, (long)entry -> size);
			}
#endif
			// (KOS's FAT driver also returns a directory with an empty name, one whose long name it can't convert:
			// it read as "." and opened its own directory again)
			if ((name[0] == '.') || (name[0] == '\0')) {
				continue;
			}
			is_directory = (entry -> attr & O_DIR) != 0;
			size = (entry -> size > 0) ? (uint32)entry -> size : 0;
		}
		if (is_directory || is_rom_file(name)) {
			if (!entries_add(name, is_directory, size)) {
				printf("browser: out of memory listing [%s]\n", directory);
				browser_list_end();
				break;
			}
		}
	}
	return !listing.active;
}

// gives up on a listing in progress
static void browser_list_cancel() {
	if (listing.active) {
		browser_list_end();
	}
}

// START armed recording: the next game played is recorded (then it's off again)
static int record_armed = 0;

// whether the SD card was there when the browser opened
static int sd_present() {
	for (uint32 i = 0; i < NUM_DEVICES; i++) {
		if (strcmp(devices[i].path, "/sd") == 0) {
			return device_present[i];
		}
	}
	return 0;
}

static void device_scan() {
	for (uint32 i = 0; i < NUM_DEVICES; i++) {
		if (strcmp(devices[i].path, "/sd") == 0) {
			// (tried again each time the browser opens: a card put in since)
			device_present[i] = sd_card_mount();
			continue;
		}
		file_t handle = fs_open(devices[i].path, O_DIR | O_RDONLY);
		device_present[i] = (handle != -1);
		if (handle != -1) {
			fs_close(handle);
		}
	}
}

/*-------------------------------------------------------------------*/
/*  The highlighted ROM's header, in the help window                 */
/*-------------------------------------------------------------------*/
static void entry_path(const Browser_Entry* entry, char* path, size_t size) {
	if (directory[0] == '\0') {
		snprintf(path, size, "%s", entry -> name);
	} else {
		snprintf(path, size, "%s/%s", directory, entry -> name);
	}
}

static void art_forget() {
	art_free(&thumb);
	art_found = 0;
	helpstyle.rows_right = 0.0f;
}

// once the highlight has rested: the game's art, and its box (US, else Famicom) as the thumbnail
static void art_look() {
	if (art_looked || (timer_ms_gettime64() - art_since < ART_DELAY_MS)) {
		return;
	}
	art_looked = 1;
	int index = art_index;
	if ((index < 0) || (index >= entry_count) || entries[index].is_directory || entries[index].is_nsf || (directory[0] == '\0')) {
		return;
	}
	char path[OPTIONS_PATH_LENGTH + 64];
	entry_path(&entries[index], path, sizeof(path));
	if (art_find(path, &art) != 0) {
		return;
	}
	art_found = 1;
	if (art.has_box || art.has_box_j) {
		art_load(&art, art.has_box ? "box" : "box_j", &thumb);
		helpstyle.rows_right = THUMB_WIDTH + 10.0f;
	}
}

void browser_release_art() {
	art_forget();
	// (looked up again when the menus are back)
	info_index = -1;
}

void browser_draw(uint32 list) {
	if ((list != PVR_LIST_TR_POLY) || (directory[0] == '\0') || (thumb.texture == NULL)) {
		return;
	}
	art_draw(&thumb, helpdata.x + helpdata.width - 12.0f - THUMB_WIDTH, helpdata.y + 34.0f, THUMB_WIDTH, THUMB_HEIGHT, 34.0f, 0.0f, 0.0f, 1.0f, 1.0f);
}

static void info_show() {
	int index = mydata.highlighted_index;
	if (index == info_index) {
		return;
	}
	info_index = index;
	// (the art waits for the highlight to rest)
	art_forget();
	art_index = index;
	art_looked = 0;
	art_since = timer_ms_gettime64();
	int lines = 0;
	if (movies_pending()) {
		snprintf(info[lines++], INFO_LENGTH, "Choose the movie's game (or leave the browser)");
	}
	if (record_armed) {
		snprintf(info[lines++], INFO_LENGTH, "Recording the next game you play");
	}
	if (status[0] != '\0') {
		snprintf(info[lines++], INFO_LENGTH, "%s", status);
	}
	const Browser_Entry* entry = ((index >= 0) && (index < entry_count)) ? &entries[index] : NULL;
	if ((entry == NULL) || entry -> is_directory) {
		// (a directory: nothing more than the status)
	} else {
		char path[OPTIONS_PATH_LENGTH + 64];
		// (an NSF's header is 128 bytes: its title, artist and sound chips)
		uint8 header[128];
		entry_path(entry, path, sizeof(path));
		file_t handle = fs_open(path, O_RDONLY);
		int got = 0;
		if (handle != -1) {
			got = fs_read(handle, header, sizeof(header));
			fs_close(handle);
		}
#ifdef FRNES_BROWSER_TRACE
		printf("browser: header of [%s]: handle %d, read %d: %02x %02x %02x %02x %02x %02x %02x %02x\n", path, (int)handle, got,
			header[0], header[1], header[2], header[3], header[4], header[5], header[6], header[7]);
#endif
		// (the size, for an FDS image's sides and the iNES size check: a dc-load listing doesn't have it)
		uint32 size = entry -> size;
		struct stat st;
		int have_stat = (fs_stat(path, &st, 0) == 0);
		if ((size == 0) && have_stat) {
			size = st.st_size;
		}
		if (handle == -1) {
			snprintf(info[lines++], INFO_LENGTH, "Can't open the file");
		} else {
			Rom_Kind kind = entry -> is_nsf ? ROM_KIND_NSF : (entry -> is_disk ? ROM_KIND_FDS : (entry -> is_movie ? ROM_KIND_MOVIE : ROM_KIND_NES));
			lines += rom_info_lines(kind, header, (got > 0) ? got : 0, size, &info[lines], INFO_LINES - lines);
		}
		// the file's date (as its file system has it: the SD card's in local time, a dc-load folder's in UTC)
		if (have_stat && (st.st_mtime > 0) && (lines < INFO_LINES)) {
			time_t mtime = st.st_mtime;
			struct tm when;
			gmtime_r(&mtime, &when);
			snprintf(info[lines++], INFO_LENGTH, "Date: %04d-%02d-%02d %02d:%02d", when.tm_year + 1900, when.tm_mon + 1,
				when.tm_mday, when.tm_hour, when.tm_min);
		}
	}
	for (int i = 0; i < lines; i++) {
		info_lines[i] = info[i];
	}
	helpdata.data_strings = info_lines;
	// (the pane doesn't scroll: no more lines than it shows, so it draws no scroll bar)
	if ((helpstyle.max_items > 0) && (lines > helpstyle.max_items)) {
		lines = helpstyle.max_items;
	}
	helpdata.num_strings = lines;
	helpdata.highlighted_index = -1;
	helpdata.top_index = 0;
}

/*-------------------------------------------------------------------*/
/*  The window                                                       */
/*-------------------------------------------------------------------*/
// a ROM's name without its extension
static char* display_names = NULL;

// each entry's icon: a folder (or the way up), an NES cartridge, an FDS disk, an NSF's notes, a movie's film
static void row_icon(int index, float x, float y, float size) {
	if ((index < 0) || (index >= entry_count)) {
		return;
	}
	const Browser_Entry* entry = &entries[index];
	Icon_Id icon = ICON_NES;
	if (entry -> is_directory) {
		icon = (strcmp(entry -> name, "..") == 0) ? ICON_UP : ICON_FOLDER;
	} else if (entry -> is_disk) {
		icon = ICON_FDS;
	} else if (entry -> is_nsf) {
		icon = ICON_NSF;
	} else if (entry -> is_movie) {
		icon = ICON_MOVIE;
	}
	image_draw_icon(icon, x, y, size + 1.0f, 35.0f);
}

static void browser_show(int highlighted) {
	int sd = sd_present();
	menu_style_windows(directory[0] ? directory : "Select a ROM", sd ? browser_keys_sd : browser_keys,
		sd ? NUM_BROWSER_KEYS_SD : NUM_BROWSER_KEYS, 0.50f);
	// (each name in full: the list cuts a long one short with "..." where it runs out of room)
	free(display_names);
	size_t names_size = 1;
	for (int i = 0; i < entry_count; i++) {
		names_size += strlen(entries[i].name) + 1;
	}
	display_names = malloc(names_size);
	char* next_name = display_names;
	for (int i = 0; i < entry_count; i++) {
		const Browser_Entry* entry = &entries[i];
		window_row_t* row = &rows[i];
		memset(row, 0, sizeof(*row));
		char* name = entry -> name;
		if (display_names) {
			name = next_name;
			strcpy(name, entry -> name);
			next_name += strlen(name) + 1;
			char* extension = strrchr(name, '.');
			if (!entry -> is_directory && (extension != NULL)) {
				*extension = '\0';
			}
		}
		if (directory[0] == '\0') {
			// the device list
			row -> kind = ROW_VALUE;
			row -> label = entry -> name;
			for (uint32 d = 0; d < NUM_DEVICES; d++) {
				if (strcmp(devices[d].path, entry -> name) == 0) {
					row -> value = devices[d].label;
				}
			}
		} else {
			// (the icon says what it is)
			row -> kind = ROW_TEXT;
			row -> label = name;
		}
	}
	if (directory[0] != '\0') {
		mydata.row_icon = row_icon;
		mydata.icon_width = 22.0f;
	}
#ifdef FRNES_BROWSER_TRACE
	for (int i = 0; (i < entry_count) && (i < 6); i++) {
		printf("browser: row %d label [%s] value [%s] kind %d (entry [%s] dir %d)\n", i, rows[i].label ? rows[i].label : "(null)",
			rows[i].value ? rows[i].value : "(null)", rows[i].kind, entries[i].name, entries[i].is_directory);
	}
#endif
	mydata.rows = rows;
	mydata.num_strings = entry_count;
	mydata.highlighted_index = 0;
	mydata.top_index = 0;
	if (entry_count > 0) {
		menu_move(&mydata, &mystyle, (highlighted < entry_count) ? highlighted : entry_count - 1);
	}
	info_index = -1;
	info_show();
}

// the listing is done: the window, with the row asked for highlighted
static void browser_listed() {
	if (listing.read_error) {
		snprintf(status, sizeof(status), "Couldn't read all of this folder");
		const char* lines[2] = { directory, "The card didn't answer. Choose Try again to read the folder again." };
		dialog_show("Read error", lines, 2);
	} else if ((entry_count == 0) && (directory[0] != '\0')) {
		snprintf(status, sizeof(status), "No ROMs here");
	}
	// ".." first in every directory (not the device list): B's way up, as a row
	int up = 0;
	if ((directory[0] != '\0') && entries_add("..", 1, 0)) {
		Browser_Entry parent = entries[entry_count - 1];
		memmove(&entries[1], &entries[0], (entry_count - 1) * sizeof(Browser_Entry));
		entries[0] = parent;
		up = 1;
	}
	// (a read error: a row to read the folder again, after "..", highlighted)
	if (listing.read_error && entries_add("Try again", 1, 0)) {
		Browser_Entry retry = entries[entry_count - 1];
		retry.is_retry = 1;
		memmove(&entries[up + 1], &entries[up], (entry_count - 1 - up) * sizeof(Browser_Entry));
		entries[up] = retry;
		browser_show(up);
		return;
	}
	// (a directory just entered: its first entry rather than "..")
	int highlighted = ((listing.select_name[0] == '\0') && (listing.select_index == 0) && (entry_count > up)) ? up : listing.select_index;
	if (listing.select_name[0] != '\0') {
		for (int i = 0; i < entry_count; i++) {
			if (strcmp(entries[i].name, listing.select_name) == 0) {
				highlighted = i;
			}
		}
	}
	browser_show(highlighted);
}

// while listing: the directory's name above an empty list, and the count so far in the help window
static void browser_show_progress() {
	static char progress[INFO_LENGTH];
	static char* progress_lines[1];
	snprintf(progress, sizeof(progress), "Reading... %d", entry_count);
	progress_lines[0] = progress;
	helpdata.data_strings = progress_lines;
	helpdata.num_strings = 1;
	helpdata.highlighted_index = -1;
	helpdata.top_index = 0;
}

// goes to 'path' ("" for the devices), then highlights the row named 'select' (if any) or row 'highlighted'; falls
// back to the devices
static void browser_enter_selecting(const char* path, const char* select, int highlighted) {
	snprintf(directory, sizeof(directory), "%s", path);
	snprintf(listing.select_name, sizeof(listing.select_name), "%s", select ? select : "");
	listing.select_index = highlighted;
	if (!browser_list_begin()) {
		snprintf(status, sizeof(status), "Unable to read %s", path);
		directory[0] = '\0';
		browser_list_begin();
		listing.select_name[0] = '\0';
		listing.select_index = 0;
	}
	if (!listing.active) {
		browser_listed();
		return;
	}
	menu_style_windows(directory, listing_keys, 1, 0.50f);
	mydata.rows = rows;
	mydata.num_strings = 0;
	mydata.highlighted_index = 0;
	mydata.top_index = 0;
	browser_show_progress();
}

static void browser_enter(const char* path, int highlighted) {
	browser_enter_selecting(path, NULL, highlighted);
}

void browser_launch_failed(const char* path) {
	// why, as far as the file shows: it won't open, isn't a ROM, is too large, its board isn't supported, an FDS game
	// without the BIOS
	char reason[DIALOG_LINE_LENGTH] = "The emulator couldn't load it.";
	uint8 header[128];
	file_t handle = fs_open(path, O_RDONLY);
	int got = 0;
	if (handle == -1) {
		snprintf(reason, sizeof(reason), "The file couldn't be opened.");
	} else {
		got = fs_read(handle, header, sizeof(header));
		fs_close(handle);
		const char* extension = strrchr(path, '.');
		if ((extension != NULL) && (strcasecmp(extension, ".fds") == 0)) {
			snprintf(reason, sizeof(reason), "Check that disksys.rom (the FDS BIOS) is there.");
		} else if ((got < 16) || (memcmp(header, "NES\x1a", 4) != 0)) {
			snprintf(reason, sizeof(reason), "It doesn't have an iNES header.");
		} else {
			// (the emulator loader's own rules: the board, the sizes, a truncated file)
			struct stat st;
			uint32 size = (fs_stat(path, &st, 0) == 0) ? (uint32)st.st_size : 0;
			char support[ROM_INFO_LENGTH];
			rom_info_support(header, size, support, sizeof(support));
			if (strncmp(support, "Not supported", 13) == 0) {
				snprintf(reason, sizeof(reason), "%s.", support);
			}
		}
	}
	const char* name = strrchr(path, '/');
	const char* lines[2] = { name ? name + 1 : path, reason };
	dialog_show("Unable to start that ROM", lines, 2);
}

void browser_open() {
	menu_close();
	frontend_browsing();
	device_scan();
	status[0] = '\0';
	browser_enter(options.browser_directory, options.browser_index);
}

static void browser_remember() {
	snprintf(options.browser_directory, OPTIONS_PATH_LENGTH, "%s", directory);
	options.browser_index = mydata.highlighted_index;
}

// B / up: the parent directory, with the one we were in highlighted; above a device, the device list
static void browser_up() {
	if (directory[0] == '\0') {
		browser_remember();
		movies_cancel();
		frontend_main_menu();
		return;
	}
	char child[OPTIONS_PATH_LENGTH], parent[OPTIONS_PATH_LENGTH];
	snprintf(child, sizeof(child), "%s", directory);
	snprintf(parent, sizeof(parent), "%s", directory);
	char* slash = strrchr(parent, '/');
	if ((slash == NULL) || (slash == parent)) {
		// a device's top: back to the devices
		parent[0] = '\0';
	} else {
		*slash = '\0';
	}
	const char* name = (parent[0] == '\0') ? child : &child[strlen(parent) + 1];
	status[0] = '\0';
	browser_enter_selecting(parent, name, 0);
}

void browser_handle(uint32 input) {
	if (listing.active) {
		// B gives up on the listing: back to the devices
		if (input & INPUT_BACK) {
			browser_list_cancel();
			snprintf(status, sizeof(status), "Stopped reading %s", directory);
			browser_enter("", 0);
			return;
		}
		if (browser_list_step()) {
			browser_listed();
		} else {
			browser_show_progress();
		}
		return;
	}
	if (input & INPUT_UP) {
		menu_move(&mydata, &mystyle, -1);
	}
	if (input & INPUT_DOWN) {
		menu_move(&mydata, &mystyle, 1);
	}
	if (input & INPUT_PAGE_UP) {
		menu_move(&mydata, &mystyle, -mystyle.max_items);
	}
	if (input & INPUT_PAGE_DOWN) {
		menu_move(&mydata, &mystyle, mystyle.max_items);
	}
	if (input & INPUT_BACK) {
		browser_up();
		return;
	}
	int index = mydata.highlighted_index;
	art_look();
	// Y: the highlighted game's boxes and manual
	if ((input & INPUT_Y) && art_found) {
		viewer_open(&art);
		return;
	}
	// X: the highlighted game's Game Genie codes (cartridge games)
	if ((input & INPUT_X) && (index >= 0) && (index < entry_count) && !entries[index].is_directory && !entries[index].is_disk &&
		!entries[index].is_nsf && !entries[index].is_movie) {
		char rom[OPTIONS_PATH_LENGTH + 64];
		entry_path(&entries[index], rom, sizeof(rom));
		cheats_open(rom);
		return;
	}
	// START (with an SD card): the next game played is recorded, or not after all
	if ((input & INPUT_START) && sd_present()) {
		record_armed = !record_armed;
		const char* lines[1] = { record_armed ? "The next game you play is recorded as a movie in /sd/FrNES/movies." :
			"The next game you play isn't recorded." };
		dialog_show(record_armed ? "Recording next run" : "Recording disabled", lines, 1);
		info_index = -1;
		return;
	}
	if ((input & INPUT_ACCEPT) && (index >= 0) && (index < entry_count)) {
		const Browser_Entry* entry = &entries[index];
		char path[OPTIONS_PATH_LENGTH];
		if (entry -> is_directory && (strcmp(entry -> name, "..") == 0)) {
			browser_up();
			return;
		}
		if (entry -> is_retry) {
			status[0] = '\0';
			char again[OPTIONS_PATH_LENGTH];
			snprintf(again, sizeof(again), "%s", directory);
			browser_enter(again, 0);
			return;
		}
		if (entry -> is_directory) {
			if (directory[0] == '\0') {
				snprintf(path, sizeof(path), "%s", entry -> name);
			} else if ((strlen(directory) + 1 + strlen(entry -> name)) >= sizeof(path)) {
				snprintf(status, sizeof(status), "Path too long");
				info_index = -1;
				return;
			} else {
				snprintf(path, sizeof(path), "%s/%s", directory, entry -> name);
			}
			status[0] = '\0';
			browser_enter(path, 0);
			return;
		}
		// (NSF tunes play in the emulator: its NSF board, left / right change the song)
		// (a name can be up to 255 characters, Japanese ones 3 bytes each in UTF-8: the handoff block's path is 256 bytes)
		char rom[256];
		entry_path(entry, rom, sizeof(rom));
		browser_remember();
		// a movie over its game (movies.c), or, when its game wasn't found, the game picked next
		if (entry -> is_movie) {
			if (!movies_choose(rom)) {
				info_index = -1;
				info_show();
				return;
			}
			status[0] = '\0';
			browser_show(index);
			return;
		}
		if (movies_pending() && !entry -> is_nsf) {
			movies_pick(rom);
			status[0] = '\0';
			browser_show(index);
			return;
		}
		// (recording armed with START: this game, then recording is off again)
		if (record_armed && !entry -> is_nsf) {
			record_armed = 0;
			const char* record_directory = movies_record_directory();
			if (record_directory == NULL) {
				const char* lines[1] = { "The SD card isn't there any more: nothing was recorded." };
				dialog_show("Not recorded", lines, 1);
				return;
			}
			int result = launchEmulatorMovie(rom, NULL, record_directory);
			status[0] = '\0';
			if (result == 0) {
				movies_recorded(rom);
				snprintf(status, sizeof(status), "Recorded to %s", record_directory);
			} else {
				browser_launch_failed(rom);
			}
			info_index = -1;
			browser_show(index);
			return;
		}
		int result = launchEmulator(rom);
		// (the menus' PVR and font were set up again: the windows too)
		status[0] = '\0';
		if (result != 0) {
			browser_launch_failed(rom);
		}
		browser_show(index);
		return;
	}
	info_show();
}
