/*===================================================================*/
/*                                                                   */
/*  palettes.c : The NES palettes to choose from                     */
/*                                                                   */
/*===================================================================*/

#include "palettes.h"
#include "options.h"

#include <strings.h>

#define PALETTE_NAME_LENGTH 32

static char names[MAX_PALETTES][PALETTE_NAME_LENGTH];
static const char* name_list[MAX_PALETTES];
static int count = 0;
static int current = 0;
static uint32 colors[64];

static int compare_names(const void* a, const void* b) {
	return strcasecmp((const char*)a, (const char*)b);
}

// a palette's first 64 colors from its file; 0, or -1 if it couldn't be read
static int palette_read(const char* name, uint8 rgb[192]) {
	char path[96];
	snprintf(path, sizeof(path), "%s/%s.pal", PALETTE_DIRECTORY, name);
	file_t handle = fs_open(path, O_RDONLY);
	if (handle == -1) {
		return -1;
	}
	int got = fs_read(handle, rgb, 192);
	fs_close(handle);
	return (got == 192) ? 0 : -1;
}

void palettes_scan() {
	count = 0;
	file_t handle = fs_open(PALETTE_DIRECTORY, O_RDONLY | O_DIR);
	if (handle != -1) {
		const dirent_t* entry;
		while (((entry = fs_readdir(handle)) != NULL) && (count < MAX_PALETTES)) {
			const char* extension = strrchr(entry -> name, '.');
			size_t length = (extension != NULL) ? (size_t)(extension - entry -> name) : 0;
			if ((extension == NULL) || (strcasecmp(extension, ".pal") != 0) || (length == 0) || (length >= PALETTE_NAME_LENGTH)) {
				continue;
			}
			memcpy(names[count], entry -> name, length);
			names[count][length] = '\0';
			count++;
		}
		fs_close(handle);
	}
	// the default first, then the rest by name
	qsort(names, count, PALETTE_NAME_LENGTH, compare_names);
	for (int i = 1; i < count; i++) {
		if (strcmp(names[i], PALETTE_DEFAULT) == 0) {
			char held[PALETTE_NAME_LENGTH];
			memcpy(held, names[i], PALETTE_NAME_LENGTH);
			memmove(names[1], names[0], i * PALETTE_NAME_LENGTH);
			memcpy(names[0], held, PALETTE_NAME_LENGTH);
			break;
		}
	}
	for (int i = 0; i < count; i++) {
		name_list[i] = names[i];
	}
	if (count == 0) {
		snprintf(names[0], PALETTE_NAME_LENGTH, "%s", PALETTE_DEFAULT);
		name_list[0] = names[0];
		count = 1;
	}

	// the options' palette, if it's still there
	int chosen = 0;
	for (int i = 0; i < count; i++) {
		if (strcmp(names[i], options.palette_name) == 0) {
			chosen = i;
		}
	}
	palettes_choose(chosen);
}

int palettes_count() {
	return count;
}

const char* const* palettes_names() {
	return name_list;
}

int palettes_current() {
	return current;
}

void palettes_choose(int index) {
	if ((index < 0) || (index >= count)) {
		index = 0;
	}
	current = index;
	uint8 rgb[192];
	int read = palette_read(names[index], rgb);
	// (the default is the emulator's own colors: it needs none passed)
	int is_default = strcmp(names[index], PALETTE_DEFAULT) == 0;
	snprintf(options.palette_name, sizeof(options.palette_name), "%s", is_default ? "" : names[index]);
	options.emulator.palette_set = (!is_default && (read == 0)) ? 1 : 0;
	for (int i = 0; i < 64; i++) {
		uint8 r = 0, g = 0, b = 0;
		if (read == 0) {
			r = rgb[i * 3];
			g = rgb[i * 3 + 1];
			b = rgb[i * 3 + 2];
		}
		colors[i] = 0xFF000000 | ((uint32)r << 16) | ((uint32)g << 8) | b;
		options.palette[i] = (uint16)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
	}
}

const uint32* palettes_colors() {
	return colors;
}
