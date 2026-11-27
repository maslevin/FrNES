#ifndef __ROM_INFO_H
#define __ROM_INFO_H

#include <stddef.h>
#include <stdint.h>

/*
 * What a ROM file's header says, as lines of text for the browser's status pane: an iNES / NES 2.0 ROM's board,
 * memory, mirroring and region; an FDS image's sides and disk info; an NSF's title, artist and songs.
 */

#define ROM_INFO_LENGTH 80

typedef enum {
	ROM_KIND_NES,
	ROM_KIND_FDS,
	ROM_KIND_NSF,
	ROM_KIND_MOVIE
} Rom_Kind;

// the mapper's board name ("MMC3"), or NULL for one this table doesn't know
const char* rom_info_mapper_name(int mapper);

// whether the emulator plays an iNES / NES 2.0 ROM, from its 16-byte header and file size (0 when not known), as a
// line of text: "Supported", "Runs, but: Needs a Hyper Shot (not emulated)" or "Not supported: Mapper 188 not supported"
void rom_info_support(const uint8_t* header, uint32_t size, char* text, size_t length);

/**
 * Writes up to max_lines lines about the file to lines[] from the first got bytes of it (up to 128) and its size
 * (0 when not known); returns how many
 */
int rom_info_lines(Rom_Kind kind, const uint8_t* header, int got, uint32_t size, char lines[][ROM_INFO_LENGTH], int max_lines);

#endif
